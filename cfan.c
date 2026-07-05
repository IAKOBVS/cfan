/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 James Tirta Halim <tirtajames45 at gmail dot com> */

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <sys/stat.h>

#include "path.h"
#include "cfan.h"
#include "macros.h"
#include "util.h"
#include "config.h"
#include "temp.h"
#include "step.h"
#include "table-temp.h"

#include "table-fans.generated.h"
#include "cpu.generated.h"

static int c_temp_fds[LEN(c_table_temps)];
static int c_fan_fds[LEN(c_table_fans)];
static volatile sig_atomic_t c_sig_caught;
static const struct timespec c_sleeptime = {INTERVAL_UPDATE, 0};

#define _(x) x

const unsigned char *temptospeed = FAN_CURVE_DEFAULT;

#if CFAN_PRINT_TEMP_CPU
static int global_fd_temp_cpu = -1;
static unsigned int global_temp_cpu_old_sz = 0;
static unsigned int global_temp_cpu_max = 0;
#endif

unsigned int
c_temp_sysfs_max_get(void)
{
	unsigned int max = 0;
	unsigned int valid = 0;
	for (unsigned int i = 0, curr; i < LEN(c_table_temps); ++i) {
		curr = c_temp_fd_get(c_temp_fds[i]);
#if CFAN_PRINT_TEMP_CPU
		if (i == CFAN_TEMP_CPU_IDX)
			global_temp_cpu_max = curr;
#endif
		if (unlikely(curr == (unsigned int)-1)) {
			DBG(fprintf(stderr, "%s:%d:%s: failed to get temperature from %s.\n", __FILE__, __LINE__, ASSERT_FUNC, c_table_temps[i]));
			continue;
		}
		max = MAX(max, curr);
		++valid;
		DBG(fprintf(stderr, "%s:%d:%s: getting temperature: %d from %s.\n", __FILE__, __LINE__, ASSERT_FUNC, curr, c_table_temps[i]));
	}
	return (valid == 0) ? (unsigned int)-1 : max;
}

static ATTR_INLINE unsigned int
c_temp_max_get(void)
{
	unsigned int curr;
	unsigned int max = 0;
	unsigned int valid = 0;
	for (unsigned int i = 0; i < LEN(c_table_fn_temps); ++i) {
		curr = c_table_fn_temps[i]();
		if (unlikely(curr == (unsigned int)-1))
			continue;
		if (curr > max)
			max = curr;
		++valid;
	}
	if (unlikely(valid == 0))
		return (unsigned int)-1;
	DBG(fprintf(stderr, "%s:%d:%s: getting max temperature: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, max));
	return max;
}

static int
c_gets_len(const char *filename, char *dst, unsigned int *dst_len)
{
	int fd = open(filename, O_RDONLY);
	if (unlikely(fd == -1))
		return -1;
	int read_sz = read(fd, dst, *dst_len);
	if (unlikely(read_sz == -1)) {
		close(fd);
		return -1;
	}
	/* Don't read newline. */
	if (*(dst + read_sz - 1) == '\n')
		--read_sz;
	*(dst + read_sz) = '\0';
	*dst_len = (unsigned int)read_sz;
	if (unlikely(close(fd) == -1))
		return -1;
	return 0;
}

static unsigned int
c_fanspeed_get(const char *filename)
{
	char fanspeed[4];
	unsigned int fanspeed_len = sizeof(fanspeed);
	if (unlikely(c_gets_len(filename, fanspeed, &fanspeed_len)) == -1)
		return (unsigned int)-1;
	return c_atou_lt3(fanspeed, (int)fanspeed_len);
}

static unsigned int
c_fanspeed_max_get(void)
{
	unsigned int max = 0;
	unsigned int curr;
	for (unsigned int i = 0; i < LEN(c_table_fans); ++i) {
		curr = c_fanspeed_get(c_table_fans[i]);
		if (unlikely(curr == (unsigned int)-1))
			DIE_GRACEFUL();
		DBG(fprintf(stderr, "%s:%d:%s: getting fanspeed %d for fan %s.\n", __FILE__, __LINE__, ASSERT_FUNC, curr, c_table_fans[i]));
		if (curr > max)
			max = curr;
	}
	return max;
}

static int
c_puts_len(const char *filename, int oflag, const char *buf, unsigned int len)
{
	int fd = open(filename, O_WRONLY | oflag);
	if (unlikely(fd == -1))
		return -1;
	int write_sz = write(fd, buf, len);
	if (unlikely(write_sz == -1)) {
		close(fd);
		return -1;
	}
	if (unlikely(close(fd) == -1))
		return -1;
	return 0;
}

static ATTR_INLINE int
c_putchar(const char *filename, int oflag, char c)
{
	return c_puts_len(filename, oflag, &c, 1);
}

enum {
	PWM_ENABLE_FULLSPEED = '0',
	PWM_ENABLE_MANUAL = '1',
	PWM_ENABLE_AUTO = '2',
};

static ATTR_INLINE int
c_speeds_set(unsigned int speed)
{
	/* speed: 0-255 */
	char speeds[4];
	/* Convert speed to a string to pass to sysfs. */
	const unsigned int speeds_len = c_utoa_lt3_p(speed, speeds) - speeds;
#ifdef DEBUG
	if (unlikely((int)speed != atoi(speeds)))
		DIE_GRACEFUL(return -1);
#endif
	for (unsigned int i = 0; i < LEN(c_table_fans); ++i) {
		DBG(fprintf(stderr, "%s:%d:%s: setting speed: %s to fan %s.\n", __FILE__, __LINE__, ASSERT_FUNC, speeds, c_table_fans[i]));
		if (unlikely(pwrite(c_fan_fds[i], speeds, speeds_len, 0) != (ssize_t)speeds_len))
			DIE_GRACEFUL(return -1);
	}
	return 0;
}

static void
c_mode_setup()
{
	if (mkdir(CFAN_PATH, 0777) != 0)
		assert(errno == EEXIST);
	if (unlikely(c_puts_len(CFAN_PATH "/" CFAN_FILE_LOCK, O_CREAT | O_EXCL, "", 0) == -1)) {
		fprintf(stderr, "cfan: another instance is already running.\n");
		exit(EXIT_FAILURE);
	}
	if (temptospeed == c_table_temptospeed_med) {
		if (unlikely(c_puts_len(CFAN_PATH "/" CFAN_FILE_CURVE, O_CREAT | O_EXCL, S_LITERAL("medium\n")) == -1)) {
			fprintf(stderr, "cfan: can't write to %s.\n", CFAN_PATH "/" CFAN_FILE_CURVE);
			c_exit(EXIT_FAILURE);
		}
	} else if (temptospeed == c_table_temptospeed_high) {
		if (unlikely(c_puts_len(CFAN_PATH "/" CFAN_FILE_CURVE, O_CREAT | O_EXCL, S_LITERAL("high\n")) == -1)) {
			fprintf(stderr, "cfan: can't write to %s.\n", CFAN_PATH "/" CFAN_FILE_CURVE);
			c_exit(EXIT_FAILURE);
		}
	}
	if (unlikely(chmod(CFAN_PATH "/" CFAN_FILE_CURVE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)) {
		fprintf(stderr, "cfan: can't chmod %s.\n", CFAN_PATH "/" CFAN_FILE_CURVE);
		c_exit(EXIT_FAILURE);
	}
}

static void
c_mode_cleanup()
{
	(void)unlink(CFAN_PATH "/" CFAN_FILE_CURVE);
	(void)unlink(CFAN_PATH "/" CFAN_FILE_LOCK);
	(void)rmdir(CFAN_PATH);
}

static void
c_cleanup(void)
{
	/* Set safe speed before restoring auto mode to avoid fan spike. */
	if (unlikely(c_speeds_set(FANSPEED_DEFAULT) == -1))
		DIE_GRACEFUL();
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i) {
		/* Restore mode to auto. */
		if (unlikely(c_putchar(c_table_fans_enable[i], 0, PWM_ENABLE_AUTO) == -1))
			DIE_GRACEFUL();
	}
	for (unsigned int i = 0; i < LEN(c_fan_fds); ++i)
		close(c_fan_fds[i]);
	for (unsigned int i = 0; i < LEN(c_temp_fds); ++i)
		close(c_temp_fds[i]);
	c_mode_cleanup();
}

void
c_exit(int status)
{
	c_cleanup();
	_Exit(status);
}

static void
c_sig_handler(int signum)
{
	c_sig_caught = 1;
	(void)signum;
}

static void
c_sig_setup(void)
{
	if (unlikely(signal(SIGTERM, c_sig_handler) == SIG_ERR))
		DIE();
	if (unlikely(signal(SIGINT, c_sig_handler) == SIG_ERR))
		DIE();
	if (unlikely(signal(SIGHUP, c_sig_handler) == SIG_ERR))
		DIE();
}

static void
c_paths_sysfs_resolve(void)
{
	typedef struct {
		const char **data;
		const unsigned int len;
	} strlist_ty;
	/* Iterate through all the potential sysfs files. */
	const strlist_ty tables[] = {
		(strlist_ty) { c_table_fans,        LEN(c_table_fans)        },
		(strlist_ty) { c_table_fans_enable, LEN(c_table_fans_enable) },
		(strlist_ty) { c_table_temps,       LEN(c_table_temps)       },
	};
	/* Update hwmon/hwmon[0-9]* and thermal/thermal_zone[0-9]* to point to
	 * the real file, given that the number may change between reboots. */
	for (unsigned int i = 0; i < LEN(tables); ++i) {
		for (unsigned int j = 0; j < tables[i].len; ++j) {
			if (strstr(tables[i].data[j], "/sys/")) {
				char *p = path_sysfs_resolve(tables[i].data[j]);
				if (unlikely(p == NULL))
					DIE();
				if (p != tables[i].data[j]) {
					/* Set new path. */
					tables[i].data[j] = p;
					DBG(fprintf(stderr, "%s:%d:%s: %s doesn't exist, resolved to %s (which is malloc'd).\n", __FILE__, __LINE__, ASSERT_FUNC, tables[i].data[j], p));
				} else {
					DBG(fprintf(stderr, "%s:%d:%s: %s exists.\n", __FILE__, __LINE__, ASSERT_FUNC, p));
				}
			}
		}
	}
}

static void
c_fans_enable(void)
{
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i)
		if (unlikely(c_putchar(c_table_fans_enable[i], 0, PWM_ENABLE_MANUAL)))
			DIE_GRACEFUL();
}

void
c_init(void)
{
	c_paths_sysfs_resolve();
	for (unsigned int i = 0; i < LEN(c_table_temps); ++i)
		for (unsigned int retry = 5; retry; --retry)
			if (unlikely((c_temp_fds[i] = open(c_table_temps[i], O_RDONLY)) == -1)) {
				if (retry != 0)
					nanosleep(&c_sleeptime, NULL);
				DIE_GRACEFUL();
			}
	for (unsigned int i = 0; i < LEN(c_table_fans); ++i) {
		c_fan_fds[i] = open(c_table_fans[i], O_WRONLY);
		if (unlikely(c_fan_fds[i] == -1))
			DIE_GRACEFUL();
	}
	c_fans_enable();
}

static ATTR_INLINE unsigned int
c_speed_get(unsigned int temp)
{
	const unsigned int next_speed = temptospeed[temp];
	DBG(fprintf(stderr, "%s:%d:%s: geting curr_speed: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, next_speed));
	/* Avoid updating if curr_speed has not changed. */
	return next_speed;
}

static int
c_temp_write(int fd, unsigned int temp, unsigned int *old_temp_len)
{
	char buf[8];
	unsigned int size = c_utoa_lt3_p(temp, buf) - buf;
	/* Print milidegrees, like sysfs. */
	buf[size] = '0';
	++size;
	buf[size] = '0';
	++size;
	buf[size] = '0';
	++size;
	buf[size] = '\n';
	++size;
	buf[size] = '\0';
	if (unlikely(size != *old_temp_len)) {
		*old_temp_len = size;
		ftruncate(fd, size);
	}
	if (unlikely(pwrite(fd, buf, size, 0) < 0)) {
		DIE_GRACEFUL();
		return -1;
	}
	return 0;
}

static int
c_temp_cpu_init(void)
{
	int fd = open(CFAN_PATH "/" CFAN_FILE_TEMP_CPU, O_CREAT | O_WRONLY);
	if (unlikely(fd < 0)) {
		DIE_GRACEFUL();
		return -1;
	}
	if (unlikely(chmod(CFAN_PATH "/" CFAN_FILE_TEMP_CPU, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)) {
		fprintf(stderr, "nvspeed: can't chmod %s.\n", CFAN_PATH "/" CFAN_FILE_TEMP_CPU);
		DIE_GRACEFUL();
	}
	return fd;
}

static void
c_mainloop(void)
{
#if CFAN_PRINT_TEMP_CPU
	global_fd_temp_cpu = c_temp_cpu_init();
#endif
	/* Avoid underflow. */
	if (unlikely(STEPDOWN_MAX > temptospeed[0])) {
		fprintf(stderr, "%s:%d:%s: STEPDOWN_MAX (%d) must not be greater than the minimum fan curr_speed (%d).\n", __FILE__, __LINE__, ASSERT_FUNC, STEPDOWN_MAX, temptospeed[0]);
		DIE_GRACEFUL();
	}
	unsigned int last_speed = c_fanspeed_max_get();
	unsigned int curr_speed;
	unsigned int temp;
	unsigned int hot_secs = 0;
	unsigned int last_max_cpu = 0;
	unsigned int max_cpu = 0;
	for (; !c_sig_caught; ) {
		temp = c_temp_max_get();
		if (unlikely(temp == (unsigned int)-1))
			DIE_GRACEFUL();
#if CFAN_PRINT_TEMP_CPU
		max_cpu = global_temp_cpu_max;
		/* Write to tmpfs, if temp has changed */
		if (max_cpu != last_max_cpu) {
			c_temp_write(global_fd_temp_cpu, max_cpu, &global_temp_cpu_old_sz);
			last_max_cpu = max_cpu;
		}
#endif
		curr_speed = c_speed_get(temp);
		/* Avoid updating when not necessary. */
		if (curr_speed == last_speed)
			goto sleep;
		/* Get next step. */
		hot_secs = c_step_get(&curr_speed, last_speed, temp, hot_secs);
		last_speed = curr_speed;
		if (unlikely(c_speeds_set(curr_speed) == -1))
			DIE_GRACEFUL();
sleep:
		nanosleep(&c_sleeptime, NULL);
	}
}

static void
c_inits(void)
{
	for (unsigned int i = 0; i < LEN(c_table_fn_init); ++i)
		c_table_fn_init[i]();
}

/* clang-format off */

const char *usage = _("Usage: cfan [OPTIONS]...\n")
                    _("Options:\n")
                    _("  --help\n")
                    _("    Show this help.\n")
                    _("  --medium\n")
                    _("    Medium fan speed.\n")
                    _("  --high\n")
                    _("    High fan speed.\n");

/* clang-format on */

int
main(int argc, char **argv)
{
	if (argc == 2) {
		if (!strcmp(argv[1], "--help")) {
			printf("%s", usage);
			exit(EXIT_SUCCESS);
		} else if (!strcmp(argv[1], "--medium")) {
			printf("cfan: using medium fan speed.\n");
			temptospeed = c_table_temptospeed_med;
		} else if (!strcmp(argv[1], "--high")) {
			printf("cfan: using high fan speed.\n");
			temptospeed = c_table_temptospeed_high;
		} else {
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}
	c_sig_setup();
	c_mode_setup();
	c_inits();
	c_mainloop();
	c_cleanup();
	return EXIT_SUCCESS;
}

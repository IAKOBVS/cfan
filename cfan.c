/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 James Tirta Halim <tirtajames45 at gmail dot com>
 * This file is part of cfan.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <signal.h>
#if 0
#	include <ctype.h>
#endif

#include "path.h"
#include "cfan.h"
#include "macros.h"
#include "config.h"
#include "table-temp.h"

#include "table-fans.generated.h"
#include "cpu.generated.h"

#define STEPUP_SPIKE 4

static unsigned int c_hot_secs;

static unsigned int
c_atou_lt3(const char *buf, int len)
{
	if (len == 2)
		return (unsigned int)((*(buf + 0) - '0') * 10 + (*(buf + 1) - '0'));
	if (len == 3)
		return (unsigned int)((*(buf + 0) - '0') * 100 + (*(buf + 1) - '0') * 10 + (*(buf + 2) - '0'));
	/* len == 1 */
	return (unsigned int)(*(buf + 0) - '0');
}

static char *
c_utoa_lt3_p(unsigned int num, char *buf)
{
	/* digits == 2 */
	if (likely((unsigned int)(num - 10) < 90)) {
		*(buf + 0) = (num / 10) + '0';
		*(buf + 1) = (num % 10) + '0';
		*(buf + 2) = '\0';
		return buf + 2;
	}
	/* digits == 3 */
	if (num > 99) {
		*(buf + 0) = (num / 100) + '0';
		*(buf + 1) = ((num / 10) % 10) + '0';
		*(buf + 2) = (num % 10) + '0';
		*(buf + 3) = '\0';
		return buf + 3;
	}
	/* digits == 1 */
	*(buf + 0) = num + '0';
	*(buf + 1) = '\0';
	return buf + 1;
}

static unsigned int
c_temp_get(const char *temp_file)
{
	int fd = open(temp_file, O_RDONLY);
	if (unlikely(fd == -1))
		return (unsigned int)-1;
	/* Milidegrees = degrees * 1000 */
	char buf[S_LEN("100") + S_LEN("000") + S_LEN("\n")];
	int read_sz = read(fd, buf, sizeof(buf));
	if (unlikely(close(fd) == -1))
		return (unsigned int)-1;
	if (unlikely(read_sz == -1))
		return (unsigned int)-1;
	/* Don't read the newline. */
	if (*(buf + read_sz - 1) == '\n')
		--read_sz;
	/* Don't read the milidegrees. */
	read_sz -= S_LEN("000");
	*(buf + read_sz) = '\0';
	return c_atou_lt3(buf, read_sz);
}

unsigned int
c_temp_sysfs_max_get(void)
{
	unsigned int max = 0;
	for (unsigned int i = 0, curr; i < LEN(c_table_temps); ++i) {
		curr = c_temp_get(c_table_temps[i]);
		if (unlikely(curr == (unsigned int)-1))
			return (unsigned int)-1;
		if (curr > max)
			max = curr;
	}
	return max;
}

static ATTR_INLINE unsigned int
c_temp_max_get(void)
{
	unsigned int curr;
	unsigned int max = 0;
	for (unsigned int i = 0; i < LEN(c_table_fn_temps); ++i) {
		curr = c_table_fn_temps[i]();
		fprintf(stderr, "%s:%d:%s: getting temperature: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, curr);
		if (unlikely(curr == (unsigned int)-1))
			return (unsigned int)-1;
		if (curr > max)
			max = curr;
	}
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
c_puts_len(const char *filename, const char *buf, unsigned int len)
{
	int fd = open(filename, O_WRONLY);
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
c_speed_set(const char *fan_file, const char *buf, unsigned int len)
{
	return c_puts_len(fan_file, buf, len);
}

static ATTR_INLINE int
c_putchar(const char *filename, char c)
{
	return c_puts_len(filename, &c, 1);
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
		if (unlikely(c_speed_set(c_table_fans[i], speeds, speeds_len) == -1))
			DIE_GRACEFUL(return -1);
	}
	return 0;
}

static void
c_cleanup(void)
{
	setbuf(stdout, NULL);
	if (unlikely(c_speeds_set(FANSPEED_DEFAULT) == -1))
		DIE_GRACEFUL();
#if 0
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i) {
		printf("Setting auto mode to fan %s.\n", c_table_fans[i]);
		/* Restore mode to auto. */
		if (unlikely(c_putchar(c_table_fans_enable[i], PWM_ENABLE_AUTO) == -1))
			DIE_GRACEFUL();
	}
#endif
}

#ifdef EXIT_SLOW
static void
c_cleanup_slow(void)
{
	setbuf(stdout, NULL);
	for (unsigned int speed = c_fanspeed_get(c_table_fans[0]); speed > FANSPEED_DEFAULT; speed -= 10) {
		if (unlikely(c_speeds_set(speed) == -1))
			DIE_GRACEFUL();
		if (unlikely(sleep(1)))
			DIE_GRACEFUL();
	}
}
#endif

void
c_exit(int status)
{
#ifdef EXIT_SLOW
	c_cleanup_slow();
#else
	c_cleanup();
#endif
	_Exit(status);
}

static void
c_sig_handler(int signum)
{
	c_exit(EXIT_SUCCESS);
	(void)signum;
}

static void
c_sig_setup(void)
{
	if (unlikely(signal(SIGTERM, c_sig_handler) == SIG_ERR))
		DIE();
	if (unlikely(signal(SIGINT, c_sig_handler) == SIG_ERR))
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
			char *p = path_sysfs_resolve(tables[i].data[j]);
			if (unlikely(p == NULL))
				DIE();
			if (p != tables[i].data[j]) {
				DBG(fprintf(stderr, "%s:%d:%s: %s doesn't exist, resolved to %s (which is malloc'd).\n", __FILE__, __LINE__, ASSERT_FUNC, tables[i].data[j], p));
				/* Set new path. */
				tables[i].data[j] = p;
			} else {
				DBG(fprintf(stderr, "%s:%d:%s: %s exists.\n", __FILE__, __LINE__, ASSERT_FUNC, p));
			}
		}
	}
}

static void
c_fans_enable(void)
{
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i)
		if (unlikely(c_putchar(c_table_fans_enable[i], PWM_ENABLE_MANUAL)))
			DIE_GRACEFUL();
}

void
c_init(void)
{
	c_paths_sysfs_resolve();
	c_fans_enable();
}

static ATTR_INLINE unsigned int
c_step_need(unsigned int *speed, unsigned int speed_last, unsigned int temp)
{
	*speed = c_table_temptospeed[temp];
	DBG(fprintf(stderr, "%s:%d:%s: geting speed: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, *speed));
	/* Avoid updating if speed has not changed. */
	return (*speed != speed_last);
}

static ATTR_INLINE unsigned int
c_step(unsigned int speed, unsigned int *speed_last, unsigned int temp)
{
	if (speed > *speed_last) {
		/* Maybe ramp up slower. */
		if (speed > *speed_last - STEPDOWN_MAX)
			/* This avoids unwanted ramping up for short spikes
			 * as in opening a browser. */
			if (c_hot_secs <= SPIKE_MAX && likely(temp < SPIKE_TEMP_MAX)) {
				++c_hot_secs;
				DBG(fprintf(stderr, "%s:%d:%s: getting step: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, speed));
				return *speed_last += STEPUP_SPIKE;
			}
	} else { /* speed < *speed_last */
		/* Ramp down slower. */
		speed = MAX(speed, *speed_last - STEPDOWN_MAX);
	}
	*speed_last = speed;
	c_hot_secs = 0;
	DBG(fprintf(stderr, "%s:%d:%s: getting step: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, speed));
	return speed;
}

static void
c_mainloop(void)
{
	/* Avoid underflow. */
	if (unlikely(STEPDOWN_MAX > c_table_temptospeed[0])) {
		fprintf(stderr, "%s:%d:%s: STEPDOWN_MAX (%d) must not be greater than the minimum fan speed (%d).\n", __FILE__, __LINE__, ASSERT_FUNC, STEPDOWN_MAX, c_table_temptospeed[0]);
		DIE_GRACEFUL();
	}
	unsigned int speed_last = c_fanspeed_max_get();
	unsigned int speed;
	unsigned int temp;
	for (;;) {
		temp = c_temp_max_get();
		if (unlikely(temp == (unsigned int)-1))
			DIE_GRACEFUL();
		/* Avoid updating when not necessary. */
		if (!c_step_need(&speed, speed_last, temp))
			goto sleep;
		/* Get next step. */
		speed = c_step(speed, &speed_last, temp);
		if (unlikely(c_speeds_set(speed) == -1))
			DIE_GRACEFUL();
sleep:
		if (unlikely(sleep(INTERVAL_UPDATE)))
			DIE_GRACEFUL();
	}
}

static void
c_inits(void)
{
	for (unsigned int i = 0; i < LEN(c_table_fn_init); ++i)
		c_table_fn_init[i]();
}

#if 0

static char *
c_aredigits(const char *s)
{
	for (; isdigit(*s); ++s) {}
	return *s == '\0' ? (char *)s : NULL;
}

static char *
c_startswith(const char *s1, const char *s2)
{
	for (; *s1 == *s2 && *s2; ++s1, ++s2)
		;
	return *s2 == '\0' ? (char *)s1 : NULL;
}

#	define _(x) x

/* clang-format off */

const char *c_usage = 
	_("Usage: cfan [OPTIONS]...\n")
	_("Options:\n")
	_("  --set-speed-pwm SPEED\n")
	_("    Set all fan speeds to SPEED (0-255).\n")
	_("  --set-speed-percent SPEED\n")
	_("    Set all fan speeds to SPEED (0-100).\n")
	_("\n")
	_("Otherwise, it will use the configuration in config.h");

/* clang-format on */

void
c_args_handle(int argc, const char **argv)
{
	for (unsigned int i = 1; i < (unsigned int)argc; ++i) {
		if (c_startswith(argv[i], "--")) {
			if (c_startswith(argv[i], "--set-speed")) {
				if (unlikely(argv[i + 1] == NULL))
					DIE_GRACEFUL();
				char *end = c_aredigits(argv[i + 1]);
				if (unlikely(end == NULL))
					DIE_GRACEFUL();
				if (unlikely(end - argv[i + 1] < 1))
					DIE_GRACEFUL();
				if (unlikely(end - argv[i + 1] > 3))
					DIE_GRACEFUL();
				int min;
				int max;
				enum {
					MODE_PWM = 1,
					MODE_PERCENT = 2,
				};
				int speed_mode;
				if (!strcmp(argv[i] + S_LEN("--set-speed"), "-pwm")) {
					min = 51;
					max = 255;
					speed_mode = MODE_PWM;
				} else if (!strcmp(argv[i] + S_LEN("--set-speed"), "-percent")) {
					min = 20;
					max = 100;
					speed_mode = MODE_PERCENT;
				} else {
					fprintf(stderr, "%s\n", c_usage);
					DIE_GRACEFUL();
					speed_mode = 0;
					min = 0;
					max = 0;
				}
				int speed = atoi(argv[i + 1]);
				if (unlikely(speed < min))
					exit(EXIT_SUCCESS);
				if (unlikely(speed > max)) {
					fprintf(stderr, "%s:%d:%s: trying to set speed (%d) above maximum (255).\n", __FILE__, __LINE__, ASSERT_FUNC, speed);
					DIE_GRACEFUL();
				}
				if (speed_mode == MODE_PERCENT)
					speed = (int)((float)speed * 2.55f);
				if (unlikely(c_speeds_set((unsigned int)speed) == -1))
					DIE_GRACEFUL();
				exit(EXIT_SUCCESS);
			}
		} else if (c_startswith(argv[i], "-")) {
			if (c_startswith(argv[i], "-h")) {
				printf("%s\n", c_usage);
				exit(EXIT_SUCCESS);
			} else {
				fprintf(stderr, "%s\n", c_usage);
				exit(EXIT_FAILURE);
			}
		} else {
			fprintf(stderr, "%s\n", c_usage);
			DIE_GRACEFUL();
		}
	}
}

/* clang-format on */

#endif

int
main(int argc, const char **argv)
{
	c_sig_setup();
	c_inits();
	c_mainloop();
	return EXIT_SUCCESS;
}

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
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "macros.h"
#include "config.h"
#include "temp.h"

#include "fans.generated.h"
#include "cpu.generated.h"

#define STEPUP_SPIKE 4

static unsigned int c_hot_secs;

void
c_exit(int status);

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

static ATTR_INLINE unsigned int
c_temp_max_get()
{
	unsigned int max = 0;
	for (unsigned int i = 0, curr; i < LEN(c_table_temps); ++i) {
		curr = c_temp_get(c_table_temps[i]);
		if (curr == (unsigned int)-1)
			DIE_GRACEFUL(return (unsigned int)-1);
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
		DBG(fprintf(stderr, "Getting fanspeed %d for fan %s.\n", curr, c_table_fans[i]));
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

static void
c_cleanup()
{
	setbuf(stdout, NULL);
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i) {
		char min_speed[4];
		sprintf(min_speed, "%d", 80);
		const unsigned int min_speed_len = strlen(min_speed);
		printf("Setting speed %s to fan %s.\n", min_speed, c_table_fans[i]);
		/* Set fan to minimum speed. */
		if (unlikely(c_puts_len(c_table_fans[i], min_speed, min_speed_len) == -1))
			DIE();
#if 0
		printf("Setting auto mode to fan %s.\n", c_table_fans[i]);
		/* Restore mode to auto. */
		if (unlikely(c_putchar(c_table_fans_enable[i], PWM_ENABLE_AUTO) == -1))
			DIE_GRACEFUL();
#endif
	}
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
	c_exit(EXIT_SUCCESS);
	(void)signum;
}

static void
c_sig_setup()
{
	if (unlikely(signal(SIGTERM, c_sig_handler) == SIG_ERR))
		DIE();
	if (unlikely(signal(SIGINT, c_sig_handler) == SIG_ERR))
		DIE();
}

static void
c_init()
{
	c_sig_setup();
	for (unsigned int i = 0; i < LEN(c_table_fans_enable); ++i)
		if (unlikely(c_putchar(c_table_fans_enable[i], PWM_ENABLE_MANUAL)))
			DIE_GRACEFUL();
}

static ATTR_INLINE unsigned int
c_step_need(unsigned int *speed, unsigned int speed_last, unsigned int temp)
{
	*speed = c_table_temptospeed[temp];
	DBG(fprintf(stderr, "Geting speed: %d.\n", *speed));
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
			if (c_hot_secs <= SPIKE_MAX && likely(temp < 83)) {
				++c_hot_secs;
				DBG(fprintf(stderr, "Getting step: %d.\n", speed));
				return *speed_last + STEPUP_SPIKE;
			}
	} else { /* speed < *speed_last */
		/* Ramp down slower. */
		speed = MAX(speed, *speed_last - STEPDOWN_MAX);
	}
	*speed_last = speed;
	c_hot_secs = 0;
	DBG(fprintf(stderr, "Getting step: %d.\n", speed));
	return speed;
}

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
		DBG(fprintf(stderr, "Setting speed: %s to fan %s.\n", speeds, c_table_fans[i]));
		if (unlikely(c_speed_set(c_table_fans[i], speeds, speeds_len) == -1))
			DIE_GRACEFUL(return -1);
	}
	return 0;
}

static void
c_mainloop(void)
{
	/* Avoid underflow. */
	if (unlikely(STEPDOWN_MAX > c_table_temptospeed[0])) {
		fprintf(stderr, "cfan: STEPDOWN_MAX (%d) must not be greater than the minimum fan speed (%d).\n", STEPDOWN_MAX, c_table_temptospeed[0]);
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

int
main(void)
{
	c_init();
	c_mainloop();
}

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

#include "macros.h"
#include "config.h"

#include "pwm.generated.h"
#include "cpu.generated.h"

#define STEPUP_SPIKE 4

static unsigned int c_speed_min;
static unsigned int c_hot_secs;

static int
c_atoi_lt3(const char *buf, int len)
{
	if (len == 2)
		return (*(buf + 0) - '0') * 10 + (*(buf + 1) - '0');
	if (len == 3)
		return (*(buf + 0) - '0') * 100 + (*(buf + 1) - '0') * 10 + (*(buf + 2) - '0');
	/* len == 1 */
	return *(buf + 0) - '0';
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

static unsigned char
c_temp_get(const char *temp_file)
{
	int fd = open(temp_file, O_RDONLY);
	if (unlikely(fd == -1))
		DIE_GRACEFUL(return (unsigned char)-1);
	/* Milidegrees = degrees * 1000 */
	char buf[S_LEN("100") + S_LEN("000") + S_LEN("\n")];
	int read_sz = read(fd, buf, sizeof(buf));
	if (unlikely(close(fd) == -1))
		DIE_GRACEFUL(return (unsigned char)-1);
	if (unlikely(read_sz == -1))
		DIE_GRACEFUL(return (unsigned char)-1);
	/* Don't read the newline. */
	if (*(buf + read_sz - 1) == '\n')
		--read_sz;
	/* Don't read the milidegrees. */
	read_sz -= S_LEN("000");
	*(buf + read_sz) = '\0';
	return c_atoi_lt3(buf, read_sz);
}

static int
c_puts_len(const char *filename, const char *buf, unsigned int len)
{
	int fd = open(filename, O_WRONLY);
	if (unlikely(fd == -1))
		DIE_GRACEFUL(return -1);
	int write_sz = write(fd, buf, len);
	if (unlikely(write_sz == -1)) {
		close(fd);
		DIE_GRACEFUL(return -1);
	}
	if (unlikely(close(fd) == -1))
		DIE_GRACEFUL(return -1);
	return 0;
}

static ATTR_INLINE int
c_fanspeed_set(const char *fan_file, const char *buf, unsigned int len)
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
c_init()
{
	c_speed_min = table_pwm[0];
	for (unsigned int i = 0; i < LEN(c_pwms_enable); ++i)
		if (unlikely(c_putchar(c_pwms_enable[i], PWM_ENABLE_MANUAL)))
			DIE_GRACEFUL();
}

static void
c_cleanup()
{
	for (unsigned int i = 0; i < LEN(c_pwms_enable); ++i) {
		char buf[4];
		const unsigned int buf_len = c_utoa_lt3_p(table_pwm[0], buf) - buf;
		printf("Setting fan %s to %s.\n", c_pwms[i], buf);
		/* Set fan to minimum speed. */
		if (unlikely(c_puts_len(c_pwms_enable[i], buf, buf_len) == -1))
			DIE_GRACEFUL();
		printf("Setting auto mode to fan %s.\n", c_pwms[i]);
		/* Restore mode to auto. */
		if (unlikely(c_putchar(c_pwms_enable[i], PWM_ENABLE_AUTO) == -1))
			DIE_GRACEFUL();
	}
}

static ATTR_INLINE unsigned int
c_step(unsigned int speed, unsigned int last_speed, unsigned int temp)
{
	if (speed > last_speed) {
		/* Maybe ramp up slower. */
		if (speed > last_speed - STEPDOWN_MAX)
			/* This avoids unwanted ramping up for short spikes
			 * as in opening a browser. */
			if (c_hot_secs <= 3 && likely(temp < 83)) {
				++c_hot_secs;
				return last_speed + STEPUP_SPIKE;
			}
	} else { /* speed < last_speed */
		/* Ramp down slower. */
		speed = MAX(speed, last_speed - STEPDOWN_MAX);
	}
	c_hot_secs = 0;
	return speed;
}

static void
c_mainloop(void)
{
	if (unlikely(STEPDOWN_MAX > table_pwm[0])) {
		fprintf(stderr, "cfan: STEPDOWN_MAX (%d) must not be greater than the minimum fan speed (%d).\n", STEPDOWN_MAX, table_pwm[0]);
		DIE_GRACEFUL();
	}
	unsigned int last_speed = table_pwm[0];
	unsigned int speed;
	unsigned int temp;
	for (;;) {
		for (;;) {
			temp = c_temp_get(CPU_TEMP_FILE);
			if (unlikely(temp == (unsigned char)-1))
				DIE_GRACEFUL();
			DBG(fprintf(stderr, "Getting temp: %d.\n", temp));
			speed = table_pwm[temp];
			DBG(fprintf(stderr, "Geting speed: %d.\n", speed));
			/* Avoid updating if speed has not changed. */
			if (speed == last_speed)
				break;
			speed = c_step(speed, last_speed, temp);
			last_speed = speed;
			DBG(fprintf(stderr, "Getting step: %d.\n", speed));
			/* speed: 0-255 */
			char buf[4];
			const unsigned int buf_len = c_utoa_lt3_p(speed, buf) - buf;
#ifdef DEBUG
			if (unlikely((int)speed != atoi(buf)))
				DIE_GRACEFUL();
#endif
			for (unsigned int i = 0; i < LEN(c_pwms); ++i) {
				DBG(fprintf(stderr, "Setting speed: %s.\n", buf));
				if (unlikely(c_fanspeed_set(c_pwms[i], buf, buf_len) == -1))
					DIE_GRACEFUL();
			}
			break;
		}
		if (unlikely(sleep(INTERVAL_UPDATE)))
			DIE_GRACEFUL();
	}
}

int
main(void)
{
	atexit(c_cleanup);
	c_init();
	c_mainloop();
}

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

/* TODO: use graphs to generate fan speed */
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "macros.h"
#include "pwm.generated.h"
#include "cpu.generated.h"

#ifdef DEBUG
#	define D(x) (x)
#else
#	define D(x)
#endif

#ifdef __ASSERT_FUNCTION
#	define ASSERT_FUNC __ASSERT_FUNCTION
#else
#	define ASSERT_FUNC __func__
#endif

#define S_LEN(s)     (sizeof(s) - 1)
#define S_LITERAL(s) s, S_LEN(s)
#define LEN(X)       (sizeof(X) / sizeof(X[0]))

#define DIE_GRACEFUL(x)                                                         \
	do {                                                                    \
		if (errno)                                                      \
			perror("");                                             \
		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, ASSERT_FUNC); \
		exit(EXIT_FAILURE);                                             \
		x;                                                              \
	} while (0)

#define DIE(x)             \
	do {               \
		assert(0); \
		x;         \
	} while (0)

static int
u_atoi_lt3(const char *buf, int len)
{
	if (len == 2)
		return (*(buf + 0) - '0') * 10 + (*(buf + 1) - '0');
	if (len == 3)
		return (*(buf + 0) - '0') * 100 + (*(buf + 1) - '0') * 10 + (*(buf + 2) - '0');
	/* if (len == 1) */
	return *(buf + 0) - '0';
}

static char *
u_utoa_lt3(unsigned int number, char *buf)
{
	if (number > 99) {
		*(buf + 0) = (number / 100) + '0';
		*(buf + 1) = ((number / 10) % 10) + '0';
		*(buf + 2) = (number % 10) + '0';
		*(buf + 3) = '\0';
		return buf + 3;
	}
	if (number > 9) {
		*(buf + 0) = (number / 10) + '0';
		*(buf + 1) = (number % 10) + '0';
		*(buf + 2) = '\0';
		return buf + 2;
	}
	*(buf + 0) = number + '0';
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
	return u_atoi_lt3(buf, read_sz);
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

static int
c_putchar(const char *filename, char c)
{
	return c_puts_len(filename, &c, 1);
}

enum {
	C_PWM_ENABLE_FULLSPEED = '0',
	C_PWM_ENABLE_MANUAL = '1',
	C_PWM_ENABLE_AUTO = '2',
};

static void
c_init()
{
	for (unsigned int i = 0; i < LEN(c_pwms_enable); ++i)
		if (unlikely(c_putchar(c_pwms_enable[i], C_PWM_ENABLE_MANUAL)))
			DIE_GRACEFUL();
}

static void
c_cleanup()
{
	for (unsigned int i = 0; i < LEN(c_pwms_enable); ++i) {
		if (unlikely(c_puts_len(c_pwms_enable[i], S_LITERAL(SPEED_MIN_DEF)) == -1))
			DIE_GRACEFUL();
		if (unlikely(c_putchar(c_pwms_enable[i], C_PWM_ENABLE_AUTO) == -1))
			DIE_GRACEFUL();
	}
}

static unsigned int
speed_change(unsigned int speed, unsigned int last_speed)
{
	if (speed < last_speed)
		return last_speed - ((last_speed - speed) / 2);
	else
		return speed;
}

static void
c_mainloop(void)
{
	unsigned int last_temp = 0;
	unsigned int last_speed = 0;
	unsigned int speed;
	unsigned int temp;
	for (;;) {
		for (;;) {
			temp = c_temp_get(CPU_TEMP_FILE);
			if (unlikely(temp == (unsigned char)-1))
				DIE_GRACEFUL();
			/* */
			D(fprintf(stderr, "cfan: temp:%d.\n", temp));
			D(fprintf(stderr, "cfan: last temp:%d.\n", last_temp));
			/* Avoid updating if temperature has not changed. */
			if (temp == last_temp)
				break;
			last_temp = temp;
			speed = table_pwm[temp];
			/* */
			D(fprintf(stderr, "cfan: temp:%d speed:%d.\n", temp, speed));
			/* Avoid updating if speed has not changed. */
			if (speed == last_speed)
				break;
			speed = speed_change(speed, last_speed);
			last_speed = speed;
			/* speed: 0-255 */
			char buf[4];
			const unsigned int buf_len = u_utoa_lt3(speed, buf) - buf;
			D(if (atoi(buf) != (int)speed)
			  DIE_GRACEFUL());
			for (unsigned int i = 0; i < LEN(c_pwms); ++i) {
				if (unlikely(c_puts_len(c_pwms[i], buf, buf_len) == -1))
					DIE_GRACEFUL();
				D(fprintf(stderr, "========================="));
				D(fprintf(stderr, "cfan: setting speed:%d.\n", speed));
				D(fprintf(stderr, "cfan: speedstr:%s.\n", buf));
				D(fprintf(stderr, "cfan: last speed:%d.\n", last_speed));
			}
			break;
		}
		if (unlikely(sleep(INTERVAL)))
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

#ifndef UTIL_H
#define UTIL_H 1

#include "macros.h"

static unsigned int
c_atou_lt3(const char *buf, int len)
{
	if (len == 2)
		return (unsigned int)((*(buf + 0) - '0') * 10
		                      + (*(buf + 1) - '0'));
	if (len == 3)
		return (unsigned int)((*(buf + 0) - '0') * 100
		                      + (*(buf + 1) - '0') * 10
		                      + (*(buf + 2) - '0'));
	return (unsigned int)(*(buf + 0) - '0');
}

static char *
c_utoa_lt3_p(unsigned int num, char *buf)
{
	if (likely((unsigned int)(num - 10) < 90)) {
		*(buf + 0) = (num / 10) + '0';
		*(buf + 1) = (num % 10) + '0';
		*(buf + 2) = '\0';
		return buf + 2;
	}
	if (num > 99) {
		*(buf + 0) = (num / 100) + '0';
		*(buf + 1) = ((num / 10) % 10) + '0';
		*(buf + 2) = (num % 10) + '0';
		*(buf + 3) = '\0';
		return buf + 3;
	}
	*(buf + 0) = num + '0';
	*(buf + 1) = '\0';
	return buf + 1;
}

#endif /* UTIL_H */

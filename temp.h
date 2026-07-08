#ifndef TEMP_H
#define TEMP_H 1

#include "macros.h"
#include "util.h"
#include <unistd.h>

static unsigned int
c_temp_fd_get(int fd)
{
	char buf[S_LEN("100") + S_LEN("000") + S_LEN("\n")];
	int read_sz;
	do {
		read_sz = pread(fd, buf, sizeof(buf), 0);
	} while (unlikely(read_sz == -1) && errno == EINTR);
	if (unlikely(read_sz == -1))
		return (unsigned int)-1;
	if (*(buf + read_sz - 1) == '\n')
		--read_sz;
	read_sz -= (int)S_LEN("000");
	*(buf + read_sz) = '\0';
	return c_atou_le3(buf, read_sz);
}

#endif /* TEMP_H */


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

#ifndef PATH_H
#	define PATH_H 1

#	include <glob.h>
#	include <string.h>
#	include <unistd.h>
#	include <assert.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <limits.h>
#	include <regex.h>

#	include "macros.h"

static ATTR_INLINE char *
u_stpcpy_len(char *dst, const char *src, size_t n)
{
	dst = (char *)memcpy(dst, src, n) + n;
	*dst = '\0';
	return dst;
}

/* Update hwmon/hwmon[0-9]* and thermal/thermal_zone[0-9]* to point to
 * the real file, given that the number may change between reboots.
 *
 * Return pointer to malloc'd resolved filename.
 * If file already exists, then return original filename
 *
 * Filename should start with /sys/devices/platform.
 *
 * Example filename: /sys/devices/platform/coretemp.0/hwmon/hwmon2/temp1_input */
static char *
path_sysfs_resolve(const char *filename)
{
#	if !defined NAME_MAX || !defined PATH_MAX
	enum {
#		ifndef NAME_MAX
		NAME_MAX = 256,
#		endif
#		ifndef PATH_MAX
		PATH_MAX = 4096,
#		endif
	};
#	endif
	/* No need to continue if file exists. */
	if (access(filename, F_OK) == 0)
		return (char *)filename;
	regex_t r;
	regmatch_t rm[4];
	/* Convert the filename into a glob. */
	const char *pattern = "\\(\\/sys\\/devices.*\\)\\/\\([^/0-9]*\\)[0-9]*\\/\\([^/]*\\)$";
	DBG(fprintf(stderr, "%s:%d:%s: pattern: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, pattern));
	int ret = regcomp(&r, pattern, 0);
	if (unlikely(ret))
		return NULL;
	ret = regexec(&r, filename, 4, rm, 0);
	regfree(&r);
	if (unlikely(ret != REG_NOERROR))
		return NULL;
	char glob_pattern[PATH_MAX + NAME_MAX];
	char *glob_end = glob_pattern;
	/* Construct the glob pattern. */
	/* /sys/devices/.* */
	glob_end = (char *)u_stpcpy_len(glob_end, filename + rm[1].rm_so, (size_t)(rm[1].rm_eo - rm[1].rm_so));
	glob_end = (char *)u_stpcpy_len(glob_end, S_LITERAL("/"));
	/* hwmon[0-9]* */
	glob_end = (char *)u_stpcpy_len(glob_end, filename + rm[2].rm_so, (size_t)(rm[2].rm_eo - rm[2].rm_so));
	glob_end = (char *)u_stpcpy_len(glob_end, S_LITERAL("[0-9]*/"));
	/* tail */
	glob_end = (char *)u_stpcpy_len(glob_end, filename + rm[3].rm_so, (size_t)(rm[3].rm_eo - rm[3].rm_so));
	DBG(fprintf(stderr, "%s:%d:%s: glob_pattern: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, glob_pattern));
	glob_t g;
	/* Expand the glob into the real file. */
	ret = glob(glob_pattern, 0, NULL, &g);
	if (ret == 0) {
		const size_t len = strlen(g.gl_pathv[0]);
		char *heap = (char *)malloc(len + 1);
		if (heap == NULL)
			return NULL;
		u_stpcpy_len(heap, g.gl_pathv[0], len);
		globfree(&g);
		DBG(fprintf(stderr, "%s:%d:%s: heap (malloc'd): %s.\n", __FILE__, __LINE__, ASSERT_FUNC, heap));
		return heap;
	} else if (unlikely(ret == GLOB_NOMATCH)) {
		globfree(&g);
	} else { /* glob error */
		return NULL;
	}
	return NULL;
}

#endif /* PATH_H */

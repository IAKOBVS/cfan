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

#	include "macros.h"

/* Update hwmon/hwmon[0-9]* and thermal/thermal_zone[0-9]* to point to
 * the real file, given that the number may change between reboots.
 *
 * Return pointer to malloc'd resolved filename.
 * If file already exists, then return original filename
 *
 * Filename should start with /sys/devices/platform.
 *
 * Example filename: /sys/devices/platform/coretemp.0/hwmon/hwmon2/temp1_input
 * Example pattern: hwmon/hwmon and thermal/thermal_zone
 * Example pattern_glob: hwmon/hwmon[0-9]* and thermal/thermal_zone[0-9]* */
static char *
path_sysfs_resolve(const char *filename)
{
	enum n {
#	ifndef NAME_MAX
		NAME_MAX = 256,
#	endif
#	ifndef PATH_MAX
		PATH_MAX = 4096,
#	endif
	};
	if (access(filename, F_OK) == 0)
		return (char *)filename;
	char platform[NAME_MAX];
	char monitor_dir[NAME_MAX];
	char monitor_subdir[NAME_MAX];
	char tail[PATH_MAX];
	/* %[^/]: match non slash. */
	if (unlikely(sscanf(filename, "/sys/devices/platform/%[^/]/%[^/]/%[^/0-9]%*[0-9]/%s", platform, monitor_dir, monitor_subdir, tail) < 0))
		return NULL;
	char glob_pattern[PATH_MAX];
	const char pat[] = "[0-9]*";
	/* Construct the glob pattern. */
	int len = sprintf(glob_pattern,
	                  "/sys/devices/platform/%s/%s/%s%s/%.*s",
	                  platform,
	                  monitor_dir,
	                  monitor_subdir,
	                  pat,
	                  (int)(sizeof(glob_pattern) - 1 - strlen(platform) - strlen(monitor_dir) - strlen(monitor_subdir) - strlen(pat)),
	                  tail);
	if (unlikely(len < 0))
		return NULL;
	DBG(fprintf(stderr, "%s:%d:%s: platform: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, platform));
	DBG(fprintf(stderr, "%s:%d:%s: monitor_dir: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, monitor_dir));
	DBG(fprintf(stderr, "%s:%d:%s: monitor_subdir: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, monitor_subdir));
	DBG(fprintf(stderr, "%s:%d:%s: tail: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, tail));
	DBG(fprintf(stderr, "%s:%d:%s: glob_pattern: %s.\n", __FILE__, __LINE__, ASSERT_FUNC, glob_pattern));
	glob_t g;
	int ret = glob(glob_pattern, 0, NULL, &g);
	/* Match */
	if (ret == 0) {
		if (access(g.gl_pathv[0], F_OK) == -1)
			return NULL;
		len += strlen(g.gl_pathv[0] + len - S_LEN(pat));
		char *tmp = (char *)malloc((size_t)len + 1);
		if (unlikely(tmp == NULL))
			return NULL;
		memcpy(tmp, g.gl_pathv[0], (size_t)len);
		*(tmp + len) = '\0';
		DBG(fprintf(stderr, "%s:%d:%s: tmp (malloc'd): %s.\n", __FILE__, __LINE__, ASSERT_FUNC, tmp));
		globfree(&g);
		return tmp;
	}
	if (ret == GLOB_NOMATCH)
		globfree(&g);
	else
		return NULL;
	return NULL;
}

#endif /* PATH_H */

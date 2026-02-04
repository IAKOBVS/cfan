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

#include "config.h"

#ifndef GPU_NVIDIA_H
#	define GPU_NVIDIA_H 1

#	ifdef USE_CUDA

#		define NVML_HEADER "/opt/cuda/include/nvml.h"
#		include NVML_HEADER

#		include <stdio.h>
#		include <stdlib.h>
#		include <unistd.h>
#		include <assert.h>
#		include <time.h>
#		include <errno.h>

#		include "cfan.h"
#		include "macros.h"

#		define NV_DIE_GRACEFUL(nv_ret)                                                                                      \
			do {                                                                                                         \
				if (nv_ret != NVML_SUCCESS)                                                                          \
					fprintf(stderr, "%s:%d:%s: %s\n", __FILE__, __LINE__, ASSERT_FUNC, nvmlErrorString(nv_ret)); \
				nv_cleanup();                                                                                        \
				c_exit(EXIT_FAILURE);                                                                                \
			} while (0)

/* May not work for older versions of CUDA, in which case, comment it out. */
#		define USE_NVML_DEVICEGETTEMPERATUREV 1

typedef enum {
	NV_RET_SUCC = 0,
	NV_RET_ERR
} nv_ret_ty;

static nvmlReturn_t
nv_nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensorType, unsigned int *temp)
{
#		if USE_NVML_DEVICEGETTEMPERATUREV
	nvmlTemperature_t tmp;
	tmp.sensorType = sensorType;
	tmp.version = nvmlTemperature_v1;
	const nvmlReturn_t ret = nvmlDeviceGetTemperatureV(device, &tmp);
	*temp = (unsigned int)tmp.temperature;
	return ret;
#		else
	return nvmlDeviceGetTemperature(device, sensorType, temp);
#		endif
}

/* Global variables */
static nvmlDevice_t *nv_device;
static unsigned int nv_device_count;
static int nv_inited;
static nvmlReturn_t nv_ret;

static void
nv_cleanup()
{
	if (nv_inited)
		nvmlShutdown();
	free(nv_device);
}

static void
nv_init()
{
	nv_ret = nvmlInit();
	if (unlikely(nv_ret != NVML_SUCCESS))
		NV_DIE_GRACEFUL(nv_ret);
	nv_inited = 1;
	nv_ret = nvmlDeviceGetCount(&nv_device_count);
	if (unlikely(nv_ret != NVML_SUCCESS))
		NV_DIE_GRACEFUL(nv_ret);
	nv_device = (nvmlDevice_t *)calloc(nv_device_count, sizeof(nvmlDevice_t));
	if (unlikely(nv_device == NULL))
		NV_DIE_GRACEFUL(nv_ret);
	for (unsigned int i = 0; i < nv_device_count; ++i) {
		nv_ret = nvmlDeviceGetHandleByIndex(i, nv_device + i);
		if (unlikely(nv_ret != NVML_SUCCESS))
			NV_DIE_GRACEFUL(nv_ret);
	}
}

static unsigned int
nv_temp_gpu_get_max()
{
	unsigned int max = 0;
	unsigned int temp;
	for (unsigned int i = 0; i < nv_device_count; ++i) {
		nv_ret = nv_nvmlDeviceGetTemperature(nv_device[i], NVML_TEMPERATURE_GPU, &temp);
		if (unlikely(nv_ret != NVML_SUCCESS))
			NV_DIE_GRACEFUL(nv_ret);
		DBG(fprintf(stderr, "%s:%d:%s: getting temperature: %d from GPU%d.\n", __FILE__, __LINE__, ASSERT_FUNC, temp, i));
		if (temp > max)
			max = temp;
	}
	return max;
}

#	endif

#endif /* GPU_NVIDIA_H */

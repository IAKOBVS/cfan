#include "cpu.generated.h"

#include "gpu-nvidia.h"
#include "cfan.h"

/* Add sysfs temperature files, or any file, provided that
 * it uses the same format, and is continuously updated. */

static const char *c_table_temps[] = {
	/*
	 * Temperature files from sysfs MUST start with /sys/devices/platform/,
	 * not /sys/class/, to allow the program to find the correct file
	 * across reboots. These files will then be passed to path_sysfs_resolve
	 * which finds the actual file.
	 *
	 * For example:
	 * "/sys/devices/platform/coretemp.0/hwmon/hwmon2/temp1_input",
	 * "/sys/devices/platform/coretemp.0/hwmon/hwmon2/temp2_input",
	 * */
	CPU_TEMP_FILE
};

typedef unsigned int (*fn_temp)(void);

static const fn_temp c_table_fn_temps[] = {
	c_temp_sysfs_max_get,
	nv_temp_gpu_get_max
};

typedef void (*fn_temp_init)(void);

static const fn_temp_init c_table_fn_init[] = {
	c_init,
	nv_init
};

#include "cpu.generated.h"

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

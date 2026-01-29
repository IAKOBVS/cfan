#include "cpu.generated.h"

/* Add sysfs temperature files, or any file, provided that
 * it uses the same format, and is continuously updated. */

static const char *c_table_temps[] = {
	/* For example:
	"/sys/class/hwmon/hwmon2/temp2_name",
	"/sys/class/hwmon/hwmon2/temp3_name",
	"/sys/class/hwmon/hwmon2/temp4_name",
	"/sys/class/hwmon/hwmon2/temp5_name",
	"/sys/class/hwmon/hwmon2/temp6_name",
	"/sys/class/hwmon/hwmon2/temp7_name", */
	CPU_TEMP_FILE
};

#include "cpu.generated.h"

/* Add sysfs temperature files, or any file, provided that
 * it uses the same format, and is continuously updated. */

static const char *c_table_temps[] = {
	/* For example:
	"/sys/class/hwmon/hwmon2/temp2_input",
	"/sys/class/hwmon/hwmon2/temp3_input",
	"/sys/class/hwmon/hwmon2/temp4_input",
	"/sys/class/hwmon/hwmon2/temp5_input",
	"/sys/class/hwmon/hwmon2/temp6_input",
	"/sys/class/hwmon/hwmon2/temp7_input", */
	CPU_TEMP_FILE
};

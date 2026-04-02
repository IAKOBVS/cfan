#include "config.h"
#include <stdio.h>
int
main()
{
#define table c_table_temptospeed_med
/* #define table c_table_temptospeed_high */
	for (unsigned int i = 0; i < sizeof(table); ++i) {
		const char *space;
		if (i <= 9)
			space = "  ";
		else if (i <= 99)
			space = " ";
		else
			space = "";
		printf("%s%dc ", space, i);
		for (unsigned int j = 0; j <= 100; ++j) {
			const char *c;
			if (j == (unsigned int)(table[i] / 2.55)) {
				c = "x";
				printf("%s ", c);
				printf("%d%%", j);
				if (j <= 9)
					j += 3;
				else if (j <= 99)
					j += 4;
				else
					j += 5;
			} else {
				c = "-";
				printf("%s", c);
			}
		}
		putchar('\n');
	}
	printf("%s", "     ");
	for (unsigned int i = 0; i <= 100; ++i) {
		if (i % 5 == 0) {
			const char *space;
			if (i <= 9)
				space = "   ";
			else if (i <= 99)
				space = "  ";
			else
				space = " ";
			printf("%d%s ", i, space);
			if (i <= 9)
				i += 3;
			else if (i <= 99)
				i += 4;
			else
				i += 5;
		}
	}
	putchar('\n');
	return 0;
}

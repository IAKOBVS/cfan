#include "util.h"
#include "config.h"
#include "step.h"
#include "temp.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static unsigned int tests_run;
static unsigned int tests_failed;

#define TEST(name)                                                          \
	do {                                                                \
		++tests_run;                                                \
		name();                                                         \
		printf("  PASS %s\n", #name);                                   \
	} while (0)

static void
fail(const char *msg)
{
	(void)msg;
	++tests_failed;
}

static void
test_atou_lt3_1digit(void)
{
	if (c_atou_lt3("0", 1) != 0) fail("0");
	if (c_atou_lt3("5", 1) != 5) fail("5");
	if (c_atou_lt3("9", 1) != 9) fail("9");
}

static void
test_atou_lt3_2digit(void)
{
	if (c_atou_lt3("10", 2) != 10) fail("10");
	if (c_atou_lt3("51", 2) != 51) fail("51");
	if (c_atou_lt3("99", 2) != 99) fail("99");
}

static void
test_atou_lt3_3digit(void)
{
	if (c_atou_lt3("100", 3) != 100) fail("100");
	if (c_atou_lt3("200", 3) != 200) fail("200");
	if (c_atou_lt3("255", 3) != 255) fail("255");
}

static void
test_utoa_lt3_1digit(void)
{
	char buf[4];
	char buf2[4];
	char buf3[4];
	if (c_utoa_lt3_p(0, buf) != buf + 1
	    || strcmp(buf, "0") != 0)
		fail("0");
	if (c_utoa_lt3_p(5, buf2) != buf2 + 1
	    || strcmp(buf2, "5") != 0)
		fail("5");
	if (c_utoa_lt3_p(9, buf3) != buf3 + 1
	    || strcmp(buf3, "9") != 0)
		fail("9");
}

static void
test_utoa_lt3_2digit(void)
{
	char buf[4];
	char buf2[4];
	char buf3[4];
	if (c_utoa_lt3_p(10, buf) != buf + 2
	    || strcmp(buf, "10") != 0)
		fail("10");
	if (c_utoa_lt3_p(51, buf2) != buf2 + 2
	    || strcmp(buf2, "51") != 0)
		fail("51");
	if (c_utoa_lt3_p(99, buf3) != buf3 + 2
	    || strcmp(buf3, "99") != 0)
		fail("99");
}

static void
test_utoa_lt3_3digit(void)
{
	char buf[4];
	char buf2[4];
	char buf3[4];
	if (c_utoa_lt3_p(100, buf) != buf + 3
	    || strcmp(buf, "100") != 0)
		fail("100");
	if (c_utoa_lt3_p(200, buf2) != buf2 + 3
	    || strcmp(buf2, "200") != 0)
		fail("200");
	if (c_utoa_lt3_p(255, buf3) != buf3 + 3
	    || strcmp(buf3, "255") != 0)
		fail("255");
}

static void
test_temp_fd_get_basic(void)
{
	FILE *f = tmpfile();
	if (!f) { fail("tmpfile"); return; }
	fwrite("55000\n", 1, 6, f);
	fflush(f);
	unsigned int result = c_temp_fd_get(fileno(f));
	fclose(f);
	if (result != 55)
		fail("55000 -> 55");
}

static void
test_temp_fd_get_nonl(void)
{
	FILE *f = tmpfile();
	if (!f) { fail("tmpfile"); return; }
	fwrite("55000", 1, 5, f);
	fflush(f);
	unsigned int result = c_temp_fd_get(fileno(f));
	fclose(f);
	if (result != 55)
		fail("55000 no newline -> 55");
}

static void
test_temp_fd_get_hundred(void)
{
	FILE *f = tmpfile();
	if (!f) { fail("tmpfile"); return; }
	fwrite("100000\n", 1, 7, f);
	fflush(f);
	unsigned int result = c_temp_fd_get(fileno(f));
	fclose(f);
	if (result != 100)
		fail("100000 -> 100");
}

static void
test_temp_fd_get_zero(void)
{
	FILE *f = tmpfile();
	if (!f) { fail("tmpfile"); return; }
	fwrite("00000\n", 1, 6, f);
	fflush(f);
	unsigned int result = c_temp_fd_get(fileno(f));
	fclose(f);
	if (result != 0)
		fail("00000 -> 0");
}

static void
test_step_get_spike(void)
{
	unsigned int curr = 100;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, 0);
	if (curr != 54) fail("spike curr");
	if (hot != 1)   fail("spike hot");
}

static void
test_step_get_hot_exceeded(void)
{
	unsigned int curr = 100;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, SPIKE_MAX + 1);
	if (curr != 100) fail("hotx curr");
	if (hot != 0)    fail("hotx hot");
}

static void
test_step_get_hightemp(void)
{
	unsigned int curr = 100;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, SPIKE_TEMP_MAX, 0);
	if (curr != 100) fail("hightemp curr");
	if (hot != 0)    fail("hightemp hot");
}

static void
test_step_get_small_rise(void)
{
	unsigned int curr = 55;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, 0);
	if (curr != 55) fail("smallrise curr");
	if (hot != 0)   fail("smallrise hot");
}

static void
test_step_get_rampdown_sharp(void)
{
	unsigned int curr = 30;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, 0);
	if (curr != 42) fail("sharpdown curr");
	if (hot != 0)   fail("sharpdown hot");
}

static void
test_step_get_rampdown_gentle(void)
{
	unsigned int curr = 44;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, 0);
	if (curr != 44) fail("gentledown curr");
	if (hot != 0)   fail("gentledown hot");
}

static void
test_step_get_rampdown_boundary(void)
{
	unsigned int curr = 42;
	unsigned int last = 50;
	unsigned int hot = c_step_get(&curr, last, 60, 0);
	if (curr != 42) fail("boundary curr");
	if (hot != 0)   fail("boundary hot");
}

int
main(void)
{
	tests_run = 0;
	tests_failed = 0;
	printf("Running tests...\n");
	TEST(test_atou_lt3_1digit);
	TEST(test_atou_lt3_2digit);
	TEST(test_atou_lt3_3digit);
	TEST(test_utoa_lt3_1digit);
	TEST(test_utoa_lt3_2digit);
	TEST(test_utoa_lt3_3digit);
	TEST(test_temp_fd_get_basic);
	TEST(test_temp_fd_get_nonl);
	TEST(test_temp_fd_get_hundred);
	TEST(test_temp_fd_get_zero);
	TEST(test_step_get_spike);
	TEST(test_step_get_hot_exceeded);
	TEST(test_step_get_hightemp);
	TEST(test_step_get_small_rise);
	TEST(test_step_get_rampdown_sharp);
	TEST(test_step_get_rampdown_gentle);
	TEST(test_step_get_rampdown_boundary);
	printf("\nResult: %u/%u passed.\n",
	       tests_run - tests_failed, tests_run);
	return tests_failed ? 1 : 0;
}

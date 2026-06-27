#ifndef STEP_H
#define STEP_H 1

#include "macros.h"

/* Spike step speed change per update (0-255). */
#define STEPUP_SPIKE 4

static ATTR_INLINE unsigned int
c_step_get(unsigned int *curr_speed, unsigned int last_speed, unsigned int temp, unsigned int hot_secs)
{
	unsigned int hot = 0;
	if (*curr_speed > last_speed) {
		/* Ramp up slower for short spikes.
		 * This avoids fans ramping up
		 * when opening a browser. */
		if (*curr_speed > last_speed + STEPDOWN_MAX
		    && hot_secs <= SPIKE_MAX
		    && likely(temp < SPIKE_TEMP_MAX)) {
			*curr_speed = last_speed + STEPUP_SPIKE;
			hot = hot_secs + 1;
		}
	} else { /* *curr_speed < last_speed */
		/* Always ramp down slower. */
		*curr_speed = MAX(*curr_speed, last_speed - STEPDOWN_MAX);
	}
	DBG(fprintf(stderr, "%s:%d:%s: getting step: %d.\n", __FILE__, __LINE__, ASSERT_FUNC, *curr_speed));
	return hot;
}

#endif /* STEP_H */

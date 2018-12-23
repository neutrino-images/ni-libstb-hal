#include <unistd.h>

#include "init.h"

#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_INIT, NULL, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_INIT, NULL, args)

static bool initialized = false;

void init_td_api()
{
	if (!initialized)
		lt_debug_init();
	lt_info("%s begin, initialized=%d, debug=0x%02x\n", __func__, (int)initialized, debuglevel);
	initialized = true;
}

void shutdown_td_api()
{
	lt_info("%s, initialized = %d\n", __func__, (int)initialized);
	initialized = false;
}

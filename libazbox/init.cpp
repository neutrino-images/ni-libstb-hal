#include <unistd.h>

#include "init.h"

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)

static bool initialized = false;

void init_td_api()
{
	if (!initialized)
		hal_debug_init();
	hal_info("%s begin, initialized=%d, debug=0x%02x\n", __func__, (int)initialized, debuglevel);
	initialized = true;
}

void shutdown_td_api()
{
	hal_info("%s, initialized = %d\n", __func__, (int)initialized);
	initialized = false;
}

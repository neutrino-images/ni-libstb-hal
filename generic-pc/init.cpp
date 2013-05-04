#include <unistd.h>

#include "init_lib.h"
#include "lt_debug.h"
#include "glfb.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_INIT, NULL, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_INIT, NULL, args)

static bool initialized = false;
GLFramebuffer *glfb = NULL;

void init_td_api()
{
	if (!initialized)
		lt_debug_init();
	lt_info("%s begin, initialized=%d, debug=0x%02x\n", __func__, (int)initialized, debuglevel);
	if (! glfb)
		glfb = new GLFramebuffer(720, 576); /* hard coded to PAL resolution for now */
	initialized = true;
}

void shutdown_td_api()
{
	lt_info("%s, initialized = %d\n", __func__, (int)initialized);
	if (glfb)
		delete glfb;
	initialized = false;
}

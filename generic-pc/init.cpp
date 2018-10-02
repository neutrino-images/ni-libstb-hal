#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "init_td.h"
#include "glfb.h"

#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_INIT, NULL, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_INIT, NULL, args)

static bool initialized = false;
GLFramebuffer *glfb = NULL;
bool HAL_nodec = false;

void init_td_api()
{
	if (!initialized)
		lt_debug_init();
	lt_info("%s begin, initialized=%d, debug=0x%02x\n", __func__, (int)initialized, debuglevel);
	if (! glfb) {
		int x = 1280, y = 720; /* default OSD FB resolution */
		/*
		 * export GLFB_RESOLUTION=720,576
		 * to restore old default behviour
		 */
		const char *tmp = getenv("GLFB_RESOLUTION");
		const char *p = NULL;
		if (tmp)
			p = strchr(tmp, ',');
		if (p) {
			x = atoi(tmp);
			y = atoi(p + 1);
		}
		lt_info("%s: setting GL Framebuffer size to %dx%d\n", __func__, x, y);
		if (!p)
			lt_info("%s: export GLFB_RESOLUTION=\"<w>,<h>\" to set another resolution\n", __func__);

		glfb = new GLFramebuffer(x, y); /* hard coded to PAL resolution for now */
	}
	/* allow disabling of Audio/video decoders in case we just want to
	 * valgrind-check other parts... export HAL_NOAVDEC=1 */
	if (getenv("HAL_NOAVDEC"))
		HAL_nodec = true;
	/* hack, this triggers that the simple_display thread does not blit() once per second... */
	setenv("SPARK_NOBLIT", "1", 1);
	initialized = true;
}

void shutdown_td_api()
{
	lt_info("%s, initialized = %d\n", __func__, (int)initialized);
	if (glfb)
		delete glfb;
	glfb = NULL;
	initialized = false;
}

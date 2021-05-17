#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "init.h"
#include "glfb.h"

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)

static bool initialized = false;
GLFramebuffer *glfb = NULL;
bool HAL_nodec = false;

void hal_api_init()
{
	if (!initialized)
		hal_debug_init();
	hal_info("%s begin, initialized=%d, debug=0x%02x\n", __func__, (int)initialized, debuglevel);
	if (! glfb)
	{
		int x = 1280, y = 720; /* default OSD FB resolution */
		/*
		 * export GLFB_RESOLUTION=720,576
		 * to restore old default behviour
		 */
		const char *tmp = getenv("GLFB_RESOLUTION");
		const char *p = NULL;
		if (tmp)
			p = strchr(tmp, ',');
		if (p)
		{
			x = atoi(tmp);
			y = atoi(p + 1);
		}
		hal_info("%s: setting GL Framebuffer size to %dx%d\n", __func__, x, y);
		if (!p)
			hal_info("%s: export GLFB_RESOLUTION=\"<w>,<h>\" to set another resolution\n", __func__);

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

void hal_api_exit()
{
	hal_info("%s, initialized = %d\n", __func__, (int)initialized);
	if (glfb)
		delete glfb;
	glfb = NULL;
	initialized = false;
}

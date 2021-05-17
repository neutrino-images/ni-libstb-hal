/* minimal test program for libstb-hal
 * (C) 2012-2013 Stefan Seyfried
 * License: GPL v2 or later
 *
 * this does just test the input converter thread for now...
 */

#include <config.h>
#include <stdint.h>
#include <unistd.h>
#include <include/init.h>
#if HAVE_GENERIC_HARDWARE
#include <include/glfb.h>

extern GLFramebuffer *glfb;
#define fb_pixel_t uint32_t
#endif

int main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	hal_api_init();
#if HAVE_GENERIC_HARDWARE
	int available = glfb->getOSDBuffer()->size(); /* allocated in glfb constructor */
	fb_pixel_t *lfb = reinterpret_cast<fb_pixel_t *>(glfb->getOSDBuffer()->data());

	int x = 0;
#endif
	while (1)
	{
#if HAVE_GENERIC_HARDWARE
		fb_pixel_t c = (0xff << (8 * x)) | 0xff000000;
		x++;
		if (x > 3) x = 0;
		for (int i = 0; i < available / 4; i++)
			*(lfb + i) = c;
		glfb->blit();
#endif
		sleep(1);
		if (! access("/tmp/endtest", R_OK))
		{
			unlink("/tmp/endtest");
			break;
		}
	};
	hal_api_exit();
	return 0;
}

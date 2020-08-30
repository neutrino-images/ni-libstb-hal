/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
 *
 * License: GPL v2 or later
 */

#include "hardware_caps.h"

static hw_caps_t caps = {
	.has_fan = 0,
	.has_SCART = 1,
	.has_SCART_input = 1,
	.has_HDMI = 0,
	.has_YUV_cinch = 0,
	.can_cpufreq = 1, /* see "elegant" hack in pwrmngr.cpp */
	.can_shutdown = 0,
	.can_cec = 0,
	.display_type = HW_DISPLAY_GFX,
	.display_xres = 128,
	.display_yres = 64,
	.display_can_deepstandby = 0;
	.display_has_statusline = 0;
	.boxvendor = "Armas",
	.boxname = "TripleDragon"
};

hw_caps_t *get_hwcaps(void)
{
	return &caps;
}

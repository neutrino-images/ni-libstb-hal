/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2013 Stefan Seyfried
 *
 * License: GPL v2 or later
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <hardware_caps.h>

static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{
	if (initialized)
		return &caps;

	memset(&caps, 0, sizeof(hw_caps_t));

	if (access("/dev/dvb/adapter0/video1", F_OK) != -1)
		caps.can_pip = 1;

	caps.can_cpufreq = 0;
	caps.can_shutdown = 1; /* for testing */
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.has_HDMI = 1;
	caps.display_xres = 8;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	strcpy(caps.startup_file, "");
	strcpy(caps.model, "raspi");
	strcpy(caps.boxvendor, "Raspberry");
	strcpy(caps.boxname, "Pi");

	initialized = 1;
	return &caps;
}

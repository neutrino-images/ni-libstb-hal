/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
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

	initialized = 1;
	caps.can_shutdown = 1;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.has_HDMI = 1;
	caps.display_xres = 8;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 0;
	strcpy(caps.boxvendor, "AZBox");
	const char *tmp;
	char buf[64];
	int len = -1;
	int fd = open("/proc/stb/info/model", O_RDONLY);
	if (fd != -1) {
		len = read(fd, buf, sizeof(buf) - 1);
		close(fd);
	}
	if (len > 0) {
		buf[len] = 0;
		strcpy(caps.boxname, buf);
	}
	else
		strcpy(caps.boxname, "(unknown model)");

	return &caps;
}

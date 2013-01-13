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
#include <sys/ioctl.h>
#include <aotom_main.h>

#include <hardware_caps.h>

#define FP_DEV "/dev/vfd"
static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{
	if (initialized)
		return &caps;

	memset(&caps, 0, sizeof(hw_caps_t));

	initialized = 1;
	caps.can_shutdown = 1;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.has_HDMI = 1;
	caps.display_xres = 4;
	strcpy(caps.boxvendor, "SPARK");
	const char *tmp;
	char buf[1024];
	int len = -1, ret, val;
	int fd = open("/proc/cmdline", O_RDONLY);
	if (fd != -1) {
		len = read(fd, buf, sizeof(buf) - 1);
		close(fd);
	}
	if (len > 0) {
		buf[len] = 0;
		char *p = strstr(buf, "STB_ID=");
		int h0, h1, h2;
		if (p && sscanf(p, "STB_ID=%x:%x:%x:", &h0, &h1, &h2) == 3) {
			int sys_id = (h0 << 16) | (h1 << 8) | h2;
			switch (sys_id) {
				case 0x090007:
					tmp = "GoldenMedia GM990";
					caps.has_SCART = 1;
					break;
				case 0x090008:
					tmp = "Edision Pingulux";
					break;
				case 0x09000a:
					tmp = "Amiko Alien SDH8900";
					caps.has_SCART = 1;
					break;
				case 0x09000b:
					tmp = "GalaxyInnovations S8120";
					caps.has_SCART = 1;
					break;
				case 0x0c0007:
					tmp = "GoldenMedia Triplex";
					caps.has_SCART = 1;
#ifdef MARTII
					caps.fe_offset = 1; // first sat tuner
#endif
					break;
				default:
					tmp = p;
			}
		} else
			tmp = "(NO STB_ID FOUND)";
		strcpy(caps.boxname, tmp);
	}
#ifdef MARTII
	char *fe_off = getenv("FE_OFFSET");
	if (fe_off)
		sscanf(fe_off, "%d", &caps.fe_offset);
#endif
	fd = open (FP_DEV, O_RDWR);
	if (fd != -1) {
		ret = ioctl(fd, VFDGETVERSION, &val);
		if (ret < 0)
			fprintf(stderr, "[hardware_caps] %s: VFDGETVERSION %m\n", __func__);
		else if (val == 1) { /* VFD, others not yet seen in the wild */
			caps.display_type = HW_DISPLAY_LINE_TEXT;
			caps.display_xres = 8;
		}
		close(fd);
	}
	return &caps;
}

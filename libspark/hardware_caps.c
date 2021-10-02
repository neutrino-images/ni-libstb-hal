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

	caps.pip_devs = 0;
	if (access("/dev/dvb/adapter0/video1", F_OK) != -1) caps.pip_devs = 1;
	if (access("/dev/dvb/adapter0/video2", F_OK) != -1) caps.pip_devs = 2;
	if (access("/dev/dvb/adapter0/video3", F_OK) != -1) caps.pip_devs = 3;
	if (caps.pip_devs > 0) caps.can_pip = 1;

	caps.has_CI = 0;
	caps.can_cec = 1;
	caps.can_cpufreq = 1;
	caps.can_shutdown = 1;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 0;
	caps.display_can_umlauts = 0;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.display_xres = 4;
	strcpy(caps.boxvendor, "SPARK");
	const char *tmp;
	char buf[1024];
	int len = -1, ret, val;
	int fd = open(FP_DEV, O_RDWR);
	if (fd != -1)
	{
		ret = ioctl(fd, VFDGETVERSION, &val);
		if (ret < 0)
			fprintf(stderr, "[hardware_caps] %s: VFDGETVERSION %m\n", __func__);
		else if (val & 1)   /* VFD = 1, DVFD = 3 */
		{
			caps.display_type = HW_DISPLAY_LINE_TEXT;
			caps.display_can_umlauts = 0; /* need test */
			caps.display_xres = 8;
			caps.display_can_set_brightness = 1;
		}
		close(fd);
	}
	fd = open("/proc/cmdline", O_RDONLY);
	if (fd != -1)
	{
		len = read(fd, buf, sizeof(buf) - 1);
		close(fd);
	}
	if (len > 0)
	{
		buf[len] = 0;
		char *p = strstr(buf, "STB_ID=");
		int h0, h1, h2;
		if (p && sscanf(p, "STB_ID=%x:%x:%x:", &h0, &h1, &h2) == 3)
		{
			int sys_id = (h0 << 16) | (h1 << 8) | h2;
			switch (sys_id)
			{
				case 0x090003:
					tmp = "Truman Premier 1+";
					break;
				case 0x090007:
					tmp = "GoldenMedia GM990";
					break;
				case 0x090008:
					tmp = "Edision Pingulux";
					if (caps.display_type == HW_DISPLAY_LINE_TEXT)
						tmp = "Edision Pingulux Plus";
					break;
				case 0x09000a:
					tmp = "Amiko Alien SDH8900";
					break;
				case 0x09000b:
					tmp = "GalaxyInnovations S8120";
					break;
				case 0x09000d:
					tmp = "Dynavision Spark";
					break;
				case 0x09000e:
					tmp = "SAB Unix F+ Solo (S902)";
					break;
				case 0x090015:
					tmp = "Superbox S 750 HD";
					break;
				case 0x09001d:
					tmp = "Fulan Spark I+";
					break;
				case 0x090020:
					tmp = "SAMSAT LINUX 1";
					break;
				case 0x090021:
					tmp = "Visionnet Hammer 5400"; // or Startrack SRT 2020 HD, or Visionnet Fireball 101
					break;
				case 0x090043:
					tmp = "Sogno Spark Revolution";
					break;
				case 0x0c0003:
					tmp = "Truman Top Box 2";
					break;
				case 0x0c0004:
					tmp = "Delta";
					break;
				case 0x0c0007:
					tmp = "GoldenMedia Triplex";
					break;
				case 0x0c000a:
					tmp = "Amiko Alien 2";
					break;
				case 0x0c000b:
					tmp = "GalaxyInnovations Avatar 3 (8820)";
					break;
				case 0x0c000d:
					tmp = "Dynavision 7162";
					break;
				case 0x0c000e:
					tmp = "SAB Unix Triple HD (S903)";
					break;
				case 0x0c0015:
					tmp = "Superbox Z500";
					break;
				case 0x0c001d:
					tmp = "Satcom 7162";
					break;
				case 0x0c0020:
					tmp = "Samsat 7162";
					break;
				case 0x0c0021:
					tmp = "Visionnet Falcon";
					break;
				case 0x0c002b:
					tmp = "Icecrypt S3700 CHD";
					break;
				case 0x0c0043:
					tmp = "Sogno Spark Triple";
					break;
				case 0x0c0045:
					tmp = "Interstar";
					break;
				default:
					tmp = p;
			}
			if ((sys_id & 0xff0000) == 0x090000) {
				caps.boxtype = 7111;
				strcpy(caps.boxarch, "STX7111");
			} else {
				caps.boxtype = 7162;
				strcpy(caps.boxarch, "STX7105");
			}
		}
		else
			tmp = "(NO STB_ID FOUND)";
		strcpy(caps.boxname, tmp);
	}

	initialized = 1;
	return &caps;
}

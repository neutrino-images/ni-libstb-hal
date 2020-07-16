/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
 *
 * License: GPL v2 or later
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <hardware_caps.h>

#define FP_DEV "/dev/dbox/oled0"
static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{
	if (initialized)
		return &caps;

	memset(&caps, 0, sizeof(hw_caps_t));

#if BOXMODEL_VUSOLO4K
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 480;
	caps.display_yres = 320;
	caps.display_type = HW_DISPLAY_GFX;
	caps.display_can_deepstandby = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_can_set_brightness = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_statusline = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "SOLO4K");
	strcpy(caps.boxarch, "BCM7376");
#endif
#if BOXMODEL_VUDUO4K
	initialized = 1;
	caps.has_CI = 2;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 480;
	caps.display_yres = 320;
	caps.display_type = HW_DISPLAY_GFX;
	caps.display_can_deepstandby = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_can_set_brightness = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_statusline = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 2;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "DUO4K");
	strcpy(caps.boxarch, "BCM7278");
#endif
#if BOXMODEL_VUULTIMO4K
	initialized = 1;
	caps.has_CI = 2;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 800;
	caps.display_yres = 480;
	caps.display_type = HW_DISPLAY_GFX;
	caps.display_can_deepstandby = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_can_set_brightness = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_statusline = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 2;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "ULTIMO4K");
	strcpy(caps.boxarch, "BCM7444S");
#endif
#if BOXMODEL_VUZERO4K
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_type = HW_DISPLAY_LED_ONLY;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "ZERO4K");
	strcpy(caps.boxarch, "BCM72604");
#endif
#if BOXMODEL_VUUNO4KSE
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 400;
	caps.display_yres = 240;
	caps.display_type = HW_DISPLAY_GFX;
	caps.display_can_deepstandby = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_can_set_brightness = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_statusline = 0;	// 0 because we use graphlcd/lcd4linux
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 2;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "UNO4KSE");
	strcpy(caps.boxarch, "BCM7252S");
#endif
#if BOXMODEL_VUUNO4K
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_type = HW_DISPLAY_LED_ONLY;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "VU+");
	strcpy(caps.boxname, "UNO4K");
	strcpy(caps.boxarch, "BCM7252S");
#endif
#if BOXMODEL_HD51
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 16;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "AX-Technologies");
	strcpy(caps.boxname, "HD51");
	strcpy(caps.boxarch, "BCM7251S");
#endif
#if BOXMODEL_HD60
	initialized = 1;
	caps.has_CI = 0;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 4;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 1;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "AX-Technologies");
	strcpy(caps.boxname, "HD60");
	strcpy(caps.boxarch, "HI3798M");
#endif
#if BOXMODEL_BRE2ZE4K
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 4;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 1;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 1;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "WWIO");
	strcpy(caps.boxname, "BRE2ZE4K");
	strcpy(caps.boxarch, "BCM7251S");
#endif
#if BOXMODEL_H7
	initialized = 1;
	caps.has_CI = 1;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 4;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 0;
	caps.display_has_colon = 1;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 0;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "AirDigital");
	strcpy(caps.boxname, "Zgemma H7C/S");
	strcpy(caps.boxarch, "BCM7251S");
#endif
#if BOXMODEL_OSMIO4K
	initialized = 1;
	caps.has_CI = 0;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 4;
	caps.display_type = HW_DISPLAY_LED_NUM;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 1;
	caps.display_has_colon = 1;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 1;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "Edision");
	strcpy(caps.boxname, "OS mio 4K");
	strcpy(caps.boxarch, "BCM72604");
#endif
#if BOXMODEL_OSMIO4KPLUS
	initialized = 1;
	caps.has_CI = 0;
	caps.can_cec = 1;
	caps.can_shutdown = 1;
	caps.display_xres = 5;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_can_deepstandby = 0;
	caps.display_can_set_brightness = 1;
	caps.display_has_statusline = 1;
	caps.display_has_colon = 0;
	caps.has_button_timer = 1;
	caps.has_button_vformat = 1;
	caps.has_HDMI = 1;
	strcpy(caps.boxvendor, "Edision");
	strcpy(caps.boxname, "OS mio+ 4K");
	strcpy(caps.boxarch, "BCM72604");
#endif
	return &caps;
}

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
#include <hardware_caps.h>

static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{
	if (initialized)
		return &caps;

	memset(&caps, 0, sizeof(hw_caps_t));

#if BOXMODEL_UFS910
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "UFS910");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
//	caps.has_SCART_input = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 16;
#endif
#if BOXMODEL_UFS912
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "UFS912");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
//	caps.has_SCART_input = 1;
	caps.can_cec = 1;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 16;
#endif
#if BOXMODEL_UFS913
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "UFS913");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
//	caps.has_SCART_input = 1;
	caps.can_cec = 1;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 16;
#endif
#if BOXMODEL_UFS922
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "UFS922");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 16;
#endif
#if BOXMODEL_ATEVIO7500
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "ATEVIO7500");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
//	caps.has_SCART_input = 1;
	caps.can_cec = 1;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 12;
#endif
#if BOXMODEL_FORTIS_HDBOX
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "HDBOX");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
//	caps.has_SCART_input = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 12;
#endif
#if BOXMODEL_OCTAGON1008
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "OCTAGON1008");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 8;
#endif
#if BOXMODEL_CUBEREVO
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 2;
#endif
#if BOXMODEL_CUBEREVO_MINI
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO-MINI");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 2;
#endif
#if BOXMODEL_CUBEREVO_MINI2
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO-MINI2");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 2;
	caps.display_can_set_brightness = 1;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 14;
#endif
#if BOXMODEL_CUBEREVO_250HD
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO-250HD");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 0;
#endif
#if BOXMODEL_CUBEREVO_2000HD
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO-2000HD");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 0;
#endif
#if BOXMODEL_CUBEREVO_3000HD
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "CUBEREVO-3000HD");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 2;
#endif
#if BOXMODEL_IPBOX9900
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "IPBOX9900");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 2;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 2;
#endif
#if BOXMODEL_IPBOX99
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "IPBOX99");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 1;
	caps.has_CI = 0;
#endif
#if BOXMODEL_IPBOX55
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "IPBOX55");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 0;
#endif
#if BOXMODEL_TF7700
	initialized = 1;
	strcpy(caps.boxvendor, "DUCKBOX");
	strcpy(caps.boxname, "TF7700");
	strcpy(caps.boxarch, caps.boxname);
	caps.can_shutdown = 1;
	caps.display_can_set_brightness = 0;
	caps.display_can_deepstandby = 0;
	caps.display_has_statusline = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = 1;
	caps.can_cec = 0;
	caps.has_fan = 0;
	caps.has_CI = 2;
#endif

	return &caps;
}

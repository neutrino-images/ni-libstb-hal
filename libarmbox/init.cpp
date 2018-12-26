#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "init.h"
#include "pwrmngr.h"
#include <proc_tools.h>

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)

static bool initialized = false;

void hal_api_init()
{
	if (!initialized)
		hal_debug_init();
	hal_info("%s begin, initialized=%d, debug=0x%02x\n", __FUNCTION__, (int)initialized, debuglevel);
	if (!initialized)
	{
		cCpuFreqManager f;
		f.SetCpuFreq(0);	/* CPUFREQ == 0 is the trigger for leaving standby */
		char buffer[64];
		sprintf(buffer, "%x", 0);
		proc_put("/proc/stb/fb/dst_top", buffer, strlen(buffer));
		proc_put("/proc/stb/fb/dst_left", buffer, strlen(buffer));
		sprintf(buffer, "%x", 576);
		proc_put("/proc/stb/fb/dst_height", buffer, strlen(buffer));
		sprintf(buffer, "%x", 720);
		proc_put("/proc/stb/fb/dst_width", buffer, strlen(buffer));
		sprintf(buffer, "%x", 1);
		proc_put("/proc/stb/fb/dst_apply", buffer, strlen(buffer));
	}
	initialized = true;
	hal_info("%s end\n", __FUNCTION__);
}

void hal_api_exit()
{
	hal_info("%s, initialized = %d\n", __FUNCTION__, (int)initialized);
	initialized = false;
}

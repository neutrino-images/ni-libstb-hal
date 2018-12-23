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

#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_INIT, NULL, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_INIT, NULL, args)

static bool initialized = false;

void init_td_api()
{
	if (!initialized)
		lt_debug_init();
	lt_info("%s begin, initialized=%d, debug=0x%02x\n", __FUNCTION__, (int)initialized, debuglevel);
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
	lt_info("%s end\n", __FUNCTION__);
}

void shutdown_td_api()
{
	lt_info("%s, initialized = %d\n", __FUNCTION__, (int)initialized);
	initialized = false;
}

#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/dvb/dmx.h>

#include "init.h"
#include "pwrmngr.h"

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
		/* this is a strange hack: the drivers seem to only work correctly after
		 * demux0 has been used once. After that, we can use demux1,2,... */
		struct dmx_pes_filter_params p;
		int dmx = open("/dev/dvb/adapter0/demux0", O_RDWR | O_CLOEXEC);
		if (dmx < 0)
			hal_info("%s: ERROR open /dev/dvb/adapter0/demux0 (%m)\n", __func__);
		else
		{
			memset(&p, 0, sizeof(p));
			p.output = DMX_OUT_DECODER;
			p.input  = DMX_IN_FRONTEND;
			p.flags  = DMX_IMMEDIATE_START;
			p.pes_type = DMX_PES_VIDEO;
			ioctl(dmx, DMX_SET_PES_FILTER, &p);
			ioctl(dmx, DMX_STOP);
			close(dmx);
		}
	}
	initialized = true;
	hal_info("%s end\n", __FUNCTION__);
}

void hal_api_exit()
{
	hal_info("%s, initialized = %d\n", __FUNCTION__, (int)initialized);
	initialized = false;
}

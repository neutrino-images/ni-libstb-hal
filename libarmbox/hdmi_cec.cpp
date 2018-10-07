/*
	Copyright (C) 2018 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include <ctype.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "linux-uapi-cec.h"
#include "hdmi_cec.h"
#include "lt_debug.h"

#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_debug_c(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, NULL, args)
#define lt_info_c(args...) _lt_info(TRIPLE_DEBUG_VIDEO, NULL, args)

#define fop(cmd, args...) ({				\
	int _r;						\
	if (fd >= 0) { 					\
		if ((_r = ::cmd(fd, args)) < 0)		\
			lt_info(#cmd"(fd, "#args")\n");	\
		else					\
			lt_debug(#cmd"(fd, "#args")\n");\
	}						\
	else { _r = fd; } 				\
	_r;						\
})

#define CEC_DEVICE "/dev/cec0"

hdmi_cec * hdmi_cec::hdmi_cec_instance = NULL;

hdmi_cec::hdmi_cec()
{
	standby_cec_activ = autoview_cec_activ = false;
	hdmiFd = -1;
	if (hdmiFd == -1)
		hdmiFd = open(CEC_DEVICE, O_RDWR | O_CLOEXEC);
	Start();
}

hdmi_cec::~hdmi_cec()
{
	if (hdmiFd >= 0)
	{
		close(hdmiFd);
		hdmiFd = -1;
	}
}

hdmi_cec* hdmi_cec::getInstance()
{
	if (hdmi_cec_instance == NULL)
		hdmi_cec_instance = new hdmi_cec();
	return hdmi_cec_instance;
}

bool hdmi_cec::SetCECMode(VIDEO_HDMI_CEC_MODE _deviceType)
{
	physicalAddress[0] = 0x10;
	physicalAddress[1] = 0x00;
	logicalAddress = 1;

	if (_deviceType == VIDEO_HDMI_CEC_MODE_OFF)
	{
		if (hdmiFd >= 0)
		{
			close(hdmiFd);
			hdmiFd = -1;
			Stop();
		}
		return false;
	}
	else
		deviceType = _deviceType;

	if (hdmiFd == -1)
	{
		hdmiFd = open(CEC_DEVICE, O_RDWR | O_CLOEXEC);
		Start();
	}

	if (hdmiFd >= 0)
	{
		__u32 monitor = CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER;
		struct cec_caps caps = {};

		if (ioctl(hdmiFd, CEC_ADAP_G_CAPS, &caps) < 0)
			lt_info("%s: CEC get caps failed (%m)\n", __func__);

		if (caps.capabilities & CEC_CAP_LOG_ADDRS)
		{
			struct cec_log_addrs laddrs = {};

			if (ioctl(hdmiFd, CEC_ADAP_S_LOG_ADDRS, &laddrs) < 0)
				lt_info("%s: CEC reset log addr failed (%m)\n", __func__);

			memset(&laddrs, 0, sizeof(laddrs));

			/*
			 * NOTE: cec_version, osd_name and deviceType should be made configurable,
			 * CEC_ADAP_S_LOG_ADDRS delayed till the desired values are available
			 * (saves us some startup speed as well, polling for a free logical address
			 * takes some time)
			 */
			laddrs.cec_version = CEC_OP_CEC_VERSION_2_0;
			strcpy(laddrs.osd_name, "neutrino");
			laddrs.vendor_id = CEC_VENDOR_ID_NONE;

			switch (deviceType)
			{
			case CEC_LOG_ADDR_TV:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_TV;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_TV;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_TV;
				break;
			case CEC_LOG_ADDR_RECORD_1:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_RECORD;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_RECORD;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_RECORD;
				break;
			case CEC_LOG_ADDR_TUNER_1:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_TUNER;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_TUNER;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_TUNER;
				break;
			case CEC_LOG_ADDR_PLAYBACK_1:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_PLAYBACK;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_PLAYBACK;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
				break;
			case CEC_LOG_ADDR_AUDIOSYSTEM:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_AUDIOSYSTEM;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM;
				break;
			default:
				laddrs.log_addr_type[laddrs.num_log_addrs] = CEC_LOG_ADDR_TYPE_UNREGISTERED;
				laddrs.all_device_types[laddrs.num_log_addrs] = CEC_OP_ALL_DEVTYPE_SWITCH;
				laddrs.primary_device_type[laddrs.num_log_addrs] = CEC_OP_PRIM_DEVTYPE_SWITCH;
				break;
			}
			laddrs.num_log_addrs++;

			if (ioctl(hdmiFd, CEC_ADAP_S_LOG_ADDRS, &laddrs) < 0)
				lt_info("%s: CEC set log addr failed (%m)\n", __func__);
		}

		if (ioctl(hdmiFd, CEC_S_MODE, &monitor) < 0)
			lt_info("%s: CEC monitor failed (%m)\n", __func__);

		GetCECAddressInfo();

		if(autoview_cec_activ)
			SetCECState(false);

		return true;

	}
	return false;
}

void hdmi_cec::GetCECAddressInfo()
{
	if (hdmiFd >= 0)
	{
		struct addressinfo addressinfo;

		__u16 phys_addr;
		struct cec_log_addrs laddrs = {};

		::ioctl(hdmiFd, CEC_ADAP_G_PHYS_ADDR, &phys_addr);
		addressinfo.physical[0] = (phys_addr >> 8) & 0xff;
		addressinfo.physical[1] = phys_addr & 0xff;

		::ioctl(hdmiFd, CEC_ADAP_G_LOG_ADDRS, &laddrs);
		addressinfo.logical = laddrs.log_addr[0];

		switch (laddrs.log_addr_type[0])
		{
		case CEC_LOG_ADDR_TYPE_TV:
			addressinfo.type = CEC_LOG_ADDR_TV;
			break;
		case CEC_LOG_ADDR_TYPE_RECORD:
			addressinfo.type = CEC_LOG_ADDR_RECORD_1;
			break;
		case CEC_LOG_ADDR_TYPE_TUNER:
			addressinfo.type = CEC_LOG_ADDR_TUNER_1;
			break;
		case CEC_LOG_ADDR_TYPE_PLAYBACK:
			addressinfo.type = CEC_LOG_ADDR_PLAYBACK_1;
			break;
		case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM:
			addressinfo.type = CEC_LOG_ADDR_AUDIOSYSTEM;
			break;
		case CEC_LOG_ADDR_TYPE_UNREGISTERED:
		default:
			addressinfo.type = CEC_LOG_ADDR_UNREGISTERED;
			break;
		}

		deviceType = addressinfo.type;
		logicalAddress = addressinfo.logical;
		if (memcmp(physicalAddress, addressinfo.physical, sizeof(physicalAddress)))
		{
			lt_info("%s: detected physical address change: %02X%02X --> %02X%02X\n", __func__, physicalAddress[0], physicalAddress[1], addressinfo.physical[0], addressinfo.physical[1]);
			memcpy(physicalAddress, addressinfo.physical, sizeof(physicalAddress));
			ReportPhysicalAddress();
		}
	}
}

void hdmi_cec::ReportPhysicalAddress()
{
	struct cec_message txmessage;
	txmessage.address = 0x0f; /* broadcast */
	txmessage.data[0] = CEC_MSG_REPORT_PHYSICAL_ADDR;
	txmessage.data[1] = physicalAddress[0];
	txmessage.data[2] = physicalAddress[1];
	txmessage.data[3] = deviceType;
	txmessage.length = 4;
	SendCECMessage(txmessage);
}

void hdmi_cec::SendCECMessage(struct cec_message &txmessage)
{
	if (hdmiFd >= 0)
	{
		char str[txmessage.length*6];
		for (int i = 0; i < txmessage.length; i++)
		{
			sprintf(str+(i*6),"(0x%02X)", txmessage.data[i]);
		}
		lt_info("[CEC] send message %s\n",str);
		struct cec_msg msg;
		cec_msg_init(&msg, logicalAddress, txmessage.address);
		memcpy(&msg.msg[1], txmessage.data, txmessage.length);
		msg.len = txmessage.length + 1;
		ioctl(hdmiFd, CEC_TRANSMIT, &msg);
	}
}

void hdmi_cec::SetCECAutoStandby(bool state)
{
	standby_cec_activ = state;
}

void hdmi_cec::SetCECAutoView(bool state)
{
	autoview_cec_activ = state;
}

void hdmi_cec::SetCECState(bool state)
{
	struct cec_message message;

	if ((standby_cec_activ) && state)
	{
		message.address = CEC_OP_PRIM_DEVTYPE_TV;
		message.data[0] = CEC_MSG_STANDBY;
		message.length = 1;
		SendCECMessage(message);
	}

	if ((autoview_cec_activ) && !state)
	{
		message.address = CEC_OP_PRIM_DEVTYPE_TV;
		message.data[0] = CEC_MSG_IMAGE_VIEW_ON;
		message.length = 1;
		SendCECMessage(message);
		usleep(10000);
		message.address = 0x0f; /* broadcast */
		message.data[0] = CEC_MSG_ACTIVE_SOURCE;
		message.data[1] = physicalAddress[0];
		message.data[2] = physicalAddress[1];
		message.length = 3;
		SendCECMessage(message);
	}

}

long hdmi_cec::translateKey(unsigned char code)
{
	long key = 0;
	switch (code)
	{
	case 0x32:
		key = 0x8b;
		break;
	case 0x20:
		key = 0x0b;
		break;
	case 0x21:
		key = 0x02;
		break;
	case 0x22:
		key = 0x03;
		break;
	case 0x23:
		key = 0x04;
		break;
	case 0x24:
		key = 0x05;
		break;
	case 0x25:
		key = 0x06;
		break;
	case 0x26:
		key = 0x07;
		break;
	case 0x27:
		key = 0x08;
		break;
	case 0x28:
		key = 0x09;
		break;
	case 0x29:
		key = 0x0a;
		break;
	case 0x30:
		key = 0x192;
		break;
	case 0x31:
		key = 0x193;
		break;
	case 0x44:
		key = 0xcf;
		break;
	case 0x45:
		key = 0x80;
		break;
	case 0x46:
		key = 0x77;
		break;
	case 0x47:
		key = 0xa7;
		break;
	case 0x48:
		key = 0xa8;
		break;
	case 0x49:
		key = 0xd0;
		break;
	case 0x53:
		key = 0x166;
		break;
	case 0x54:
		key = 0x16a;
		break;
	case 0x60:
		key = 0xcf;
		break;
	case 0x61:
		key = 0xa4;
		break;
	case 0x62:
		key = 0xa7;
		break;
	case 0x64:
		key = 0x80;
		break;
	case 0x00:
		key = 0x160;
		break;
	case 0x03:
		key = 0x69;
		break;
	case 0x04:
		key = 0x6a;
		break;
	case 0x01:
		key = 0x67;
		break;
	case 0x02:
		key = 0x6c;
		break;
	case 0x0d:
		key = 0xae;
		break;
	case 0x72:
		key = 0x18e;
		break;
	case 0x71:
		key = 0x191;
		break;
	case 0x73:
		key = 0x18f;
		break;
	case 0x74:
		key = 0x190;
		break;
	default:
		key = 0x8b;
		break;
	}
	return key;
}

bool hdmi_cec::Start()
{
	if (running)
		return false;

	if (hdmiFd == -1)
		return false;

	running = true;
	return (OpenThreads::Thread::start() == 0);
}

bool hdmi_cec::Stop()
{
	if (!running)
		return false;

	running = false;

	return (OpenThreads::Thread::join() == 0);
}

void hdmi_cec::run()
{
	while (running)
	{
		Receive();
	}
}

void hdmi_cec::Receive()
{
	bool hasdata = false;
	struct cec_message rxmessage;

	struct cec_msg msg;
	if (::ioctl(hdmiFd, CEC_RECEIVE, &msg) >= 0)
	{
		rxmessage.length = msg.len - 1;
		memcpy(&rxmessage.data, &msg.msg[1], rxmessage.length);
		hasdata = true;
	}

	if (hasdata)
	{
		bool keypressed = false;
		static unsigned char pressedkey = 0;

		char str[rxmessage.length*6];
		for (int i = 0; i < rxmessage.length; i++)
		{
			sprintf(str+(i*6),"(0x%02X)", rxmessage.data[i]);
		}
		lt_info("[CEC] received message %s\n", str);

		switch (rxmessage.data[0])
		{
		case 0x44: /* key pressed */
			keypressed = true;
			pressedkey = rxmessage.data[1];
		case 0x45: /* key released */
		{
			long code = translateKey(pressedkey);
			if (keypressed)
				code |= 0x80000000;
			lt_info("[CEC] received key %ld\n",code);
			break;
		}
		}
	}
}

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

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include "video_lib.h"

#ifndef KEY_OK
#define KEY_OK           0x160
#endif

#ifndef KEY_RED
#define KEY_RED          0x18e
#endif

#ifndef KEY_GREEN
#define KEY_GREEN        0x18f
#endif

#ifndef KEY_YELLOW
#define KEY_YELLOW       0x190
#endif

#ifndef KEY_BLUE
#define KEY_BLUE         0x191
#endif

struct cec_message
{
	unsigned char address;
	unsigned char length;
	unsigned char data[256];
} __attribute__((packed));

struct addressinfo
{
	unsigned char logical;
	unsigned char physical[2];
	unsigned char type;
};

class hdmi_cec : public OpenThreads::Thread
{
private:
	hdmi_cec();
	static hdmi_cec *hdmi_cec_instance;
	void run();
	bool Start();
	bool Stop();
	void Receive();
	unsigned char physicalAddress[2];
	bool autoview_cec_activ;
	unsigned char deviceType, logicalAddress;
	int hdmiFd;
	long translateKey(unsigned char code);
protected:
	bool running;
public:
	~hdmi_cec();
	static hdmi_cec* getInstance();
	bool SetCECMode(VIDEO_HDMI_CEC_MODE);
	void SetCECAutoView(bool);
	void SetCECAutoStandby(bool);
	void GetCECAddressInfo();
	void SendCECMessage(struct cec_message &message);
	void SetCECState(bool state);
	void ReportPhysicalAddress();
	bool standby_cec_activ;
};

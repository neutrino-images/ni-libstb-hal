/* dummy cAudio implementation that does nothing for now */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/dvb/audio.h>

#include "audio_lib.h"
#include "lt_debug.h"

#define AUDIO_DEVICE	"/dev/dvb/adapter0/audio0"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_AUDIO, this, args)

#include <linux/soundcard.h>

cAudio * audioDecoder = NULL;

cAudio::cAudio(void *, void *, void *)
{
}

cAudio::~cAudio(void)
{
	closeDevice();
}

void cAudio::openDevice(void)
{
	lt_debug("%s\n", __func__);
}

void cAudio::closeDevice(void)
{
	lt_debug("%s\n", __func__);
}

int cAudio::do_mute(bool enable, bool remember)
{
	lt_debug("%s(%d, %d)\n", __func__, enable, remember);
	return 0;
}

int cAudio::setVolume(unsigned int left, unsigned int right)
{
	lt_debug("%s(%d, %d)\n", __func__, left, right);
	return 0;
}

int cAudio::Start(void)
{
	lt_debug("%s\n", __func__);
	return 0;
}

int cAudio::Stop(void)
{
	lt_debug("%s\n", __func__);
	return 0;
}

bool cAudio::Pause(bool /*Pcm*/)
{
	return true;
};

void cAudio::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __func__, Mode);
};

void cAudio::SetStreamType(AUDIO_FORMAT type)
{
	lt_debug("%s %d\n", __func__, type);
};

int cAudio::setChannel(int channel)
{
	return 0;
};

int cAudio::PrepareClipPlay(int ch, int srate, int bits, int little_endian)
{
	lt_debug("%s ch %d srate %d bits %d le %d\n", __func__, ch, srate, bits, little_endian);
	return 0;
};

int cAudio::WriteClip(unsigned char * /*buffer*/, int /*size*/)
{
	lt_debug("cAudio::%s\n", __func__);
	return 0;
};

int cAudio::StopClip()
{
	lt_debug("%s\n", __func__);
	return 0;
};

void cAudio::getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode)
{
	lt_debug("%s\n", __func__);
	type = 0;
	layer = 0;
	freq = 0;
	bitrate = 0;
	mode = 0;
};

void cAudio::SetSRS(int /*iq_enable*/, int /*nmgr_enable*/, int /*iq_mode*/, int /*iq_level*/)
{
	lt_debug("%s\n", __func__);
};

void cAudio::SetHdmiDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::SetSpdifDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::ScheduleMute(bool On)
{
	lt_debug("%s %d\n", __func__, On);
};

void cAudio::EnableAnalogOut(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::setBypassMode(bool disable)
{
	lt_debug("%s %d\n", __func__, disable);
}

/*
 * (C) 2010-2013 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * cAudio dummy implementation
 */

#include <cstdio>
#include <cstdlib>

#include "audio_lib.h"
#include "dmx_hal.h"
#include "lt_debug.h"

#define lt_debug(args...) _lt_debug(HAL_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(HAL_DEBUG_AUDIO, this, args)

cAudio * audioDecoder = NULL;

cAudio::cAudio(void *, void *, void *)
{
	lt_debug("%s\n", __func__);
}

cAudio::~cAudio(void)
{
	lt_debug("%s\n", __func__);
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
	lt_debug("%s >\n", __func__);
	return 0;
}

int cAudio::Stop(void)
{
	lt_debug("%s >\n", __func__);
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

int cAudio::setChannel(int /*channel*/)
{
	return 0;
};

int cAudio::PrepareClipPlay(int ch, int srate, int bits, int le)
{
	lt_debug("%s ch %d srate %d bits %d le %d\n", __func__, ch, srate, bits, le);;
	return 0;
};

int cAudio::WriteClip(unsigned char *buffer, int size)
{
	lt_debug("cAudio::%s buf 0x%p size %d\n", __func__, buffer, size);
	return size;
};

int cAudio::StopClip()
{
	lt_debug("%s\n", __func__);
	return 0;
};

void cAudio::getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode)
{
	type = 0;
	layer = 0;	/* not used */
	freq = 0;
	bitrate = 0;	/* not used, but easy to get :-) */
	mode = 0;	/* default: stereo */
	lt_debug("%s t: %d l: %d f: %d b: %d m: %d\n",
		  __func__, type, layer, freq, bitrate, mode);
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

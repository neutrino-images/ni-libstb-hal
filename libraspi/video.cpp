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
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Suite 500 Boston, MA 02110-1335 USA
 *
 * cVideo dummy implementation
 */

#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "video_lib.h"
#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_VIDEO, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_VIDEO, this, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_VIDEO, NULL, args)

cVideo *videoDecoder = NULL;
int system_rev = 0;

cVideo::cVideo(int, void *, void *, unsigned int)
{
	hal_debug("%s\n", __func__);
	display_aspect = DISPLAY_AR_16_9;
	display_crop = DISPLAY_AR_MODE_LETTERBOX;
	v_format = VIDEO_FORMAT_MPEG2;
}

cVideo::~cVideo(void)
{
}

int cVideo::setAspectRatio(int vformat, int cropping)
{
	hal_info("%s(%d, %d)\n", __func__, vformat, cropping);
	return 0;
}

int cVideo::getAspectRatio(void)
{
	int ret = 0;
	return ret;
}

int cVideo::setCroppingMode(int)
{
	return 0;
}

int cVideo::Start(void *, unsigned short, unsigned short, void *)
{
	hal_debug("%s running %d >\n", __func__, thread_running);
	return 0;
}

int cVideo::Stop(bool)
{
	hal_debug("%s running %d >\n", __func__, thread_running);
	return 0;
}

int cVideo::setBlank(int)
{
	return 1;
}

int cVideo::SetVideoSystem(int system, bool)
{
	int h;
	switch (system)
	{
		case VIDEO_STD_NTSC:
		case VIDEO_STD_480P:
			h = 480;
			break;
		case VIDEO_STD_1080I60:
		case VIDEO_STD_1080I50:
		case VIDEO_STD_1080P30:
		case VIDEO_STD_1080P24:
		case VIDEO_STD_1080P25:
		case VIDEO_STD_1080P50:
			h = 1080;
			break;
		case VIDEO_STD_720P50:
		case VIDEO_STD_720P60:
			h = 720;
			break;
		case VIDEO_STD_AUTO:
			hal_info("%s: VIDEO_STD_AUTO not implemented\n", __func__);
		// fallthrough
		case VIDEO_STD_SECAM:
		case VIDEO_STD_PAL:
		case VIDEO_STD_576P:
			h = 576;
			break;
		default:
			hal_info("%s: unhandled value %d\n", __func__, system);
			return 0;
	}
	v_std = (VIDEO_STD) system;
	output_h = h;
	return 0;
}

int cVideo::getPlayState(void)
{
	return VIDEO_PLAYING;
}

void cVideo::SetVideoMode(analog_mode_t)
{
}

bool cVideo::ShowPicture(const char *fname)
{
	hal_info("%s(%s)\n", __func__, fname);
	if (access(fname, R_OK))
		return true;
}

void cVideo::StopPicture()
{
}

void cVideo::Standby(unsigned int)
{
}

int cVideo::getBlank(void)
{
	return 0;
}

void cVideo::Pig(int x, int y, int w, int h, int, int)
{
	pig_x = x;
	pig_y = y;
	pig_w = w;
	pig_h = h;
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	width = dec_w;
	height = dec_h;
	rate = dec_r;
}

void cVideo::SetSyncMode(AVSYNC_TYPE)
{
};

int cVideo::SetStreamType(VIDEO_FORMAT v)
{
	v_format = v;
	return 0;
}

bool cVideo::GetScreenImage(unsigned char *&data, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	hal_info("%s: data 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, data, xres, yres, get_video, get_osd, scale_to_video);
	return false;
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	return pts;
}

void cVideo::SetDemux(cDemux *)
{
	hal_debug("%s: not implemented yet\n", __func__);
}

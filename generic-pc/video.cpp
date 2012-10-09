/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 * (C) 2010-2012 Stefan Seyfried
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
 */

/* this is a dummy implementation with no functionality at all */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include <ctype.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#include <linux/types.h>
#include <linux/dvb/video.h>
#include "video_lib.h"
#define VIDEO_DEVICE "/dev/dvb/adapter0/video0"
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

cVideo * videoDecoder = NULL;
int system_rev = 0;

cVideo::cVideo(int, void *, void *)
{
	lt_debug("%s\n", __FUNCTION__);
}

cVideo::~cVideo(void)
{
}

int cVideo::setAspectRatio(int, int)
{
	return 0;
}

int cVideo::getAspectRatio(void)
{
	return 1;
}

int cVideo::setCroppingMode(int)
{
	return 0;
}

int cVideo::Start(void *, unsigned short, unsigned short, void *)
{
	return 0;
}

int cVideo::Stop(bool)
{
	return 0;
}

int cVideo::setBlank(int)
{
	return 1;
}

int cVideo::SetVideoSystem(int, bool)
{
	return 0;
}

int cVideo::getPlayState(void)
{
	return VIDEO_PLAYING;
}

void cVideo::SetVideoMode(analog_mode_t)
{
}

void cVideo::ShowPicture(const char *)
{
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

void cVideo::Pig(int, int, int, int, int, int)
{
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	width = 720;
	height = 576;
	rate = 50;
}

void cVideo::SetSyncMode(AVSYNC_TYPE)
{
};

int cVideo::SetStreamType(VIDEO_FORMAT)
{
	return 0;
}

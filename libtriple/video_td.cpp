/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#include <avs/avs_inf.h>
#include <clip/clipinfo.h>
#include <hw/hardware.h>
#include "video_td.h"
#include <hardware/tddevices.h>
#define VIDEO_DEVICE "/dev/" DEVICE_NAME_VIDEO
#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_VIDEO, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_VIDEO, this, args)

#define fop(cmd, args...) ({				\
	int _r;						\
	if (fd >= 0) { 					\
		if ((_r = ::cmd(fd, args)) < 0)		\
			hal_info(#cmd"(fd, "#args")\n");	\
		else					\
			hal_debug(#cmd"(fd, "#args")\n");\
	}						\
	else { _r = fd; } 				\
	_r;						\
})

cVideo * videoDecoder = NULL;
int system_rev = 0;

#if 0
/* this would be necessary for the DirectFB implementation of ShowPicture */
#include <directfb.h>
#include <tdgfx/stb04gfx.h>
extern IDirectFB *dfb;
extern IDirectFBSurface *dfbdest;
#endif

extern struct Ssettings settings;
static pthread_mutex_t stillp_mutex = PTHREAD_MUTEX_INITIALIZER;

/* debugging hacks */
static bool noscart = false;

cVideo::cVideo(int, void *, void *, unsigned int)
{
	hal_debug("%s\n", __FUNCTION__);
	if ((fd = open(VIDEO_DEVICE, O_RDWR)) < 0)
		hal_info("%s cannot open %s: %m\n", __FUNCTION__, VIDEO_DEVICE);
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	playstate = VIDEO_STOPPED;
	croppingMode = VID_DISPMODE_NORM;
	outputformat = VID_OUTFMT_RGBC_SVIDEO;
	scartvoltage = -1;
	z[0] = 100;
	z[1] = 100;
	zoomvalue = &z[0];
	const char *blanknames[2] = { "/share/tuxbox/blank_576.mpg", "/share/tuxbox/blank_480.mpg" };
	int blankfd;
	struct stat st;

	for (int i = 0; i < 2; i++)
	{
		blank_data[i] = NULL; /* initialize */
		blank_size[i] = 0;
		blankfd = open(blanknames[i], O_RDONLY);
		if (blankfd < 0)
		{
			hal_info("%s cannot open %s: %m", __FUNCTION__, blanknames[i]);
			continue;
		}
		if (fstat(blankfd, &st) != -1 && st.st_size > 0)
		{
			blank_size[i] = st.st_size;
			blank_data[i] = malloc(blank_size[i]);
			if (! blank_data[i])
				hal_info("%s malloc failed (%m)\n", __FUNCTION__);
			else if (read(blankfd, blank_data[i], blank_size[i]) != blank_size[i])
			{
				hal_info("%s short read (%m)\n", __FUNCTION__);
				free(blank_data[i]); /* don't leak... */
				blank_data[i] = NULL;
			}
			else
			{	/* set framerate to 24fps... see getBlank() */
				((char *)blank_data[i])[7] &= 0xF0;
				((char *)blank_data[i])[7] += 2;
			}
		}
		close(blankfd);
	}
	video_standby = 0;
	noscart = (getenv("TRIPLE_NOSCART") != NULL);
	if (noscart)
		hal_info("%s TRIPLE_NOSCART variable prevents SCART switching\n", __FUNCTION__);
}

cVideo::~cVideo(void)
{
	playstate = VIDEO_STOPPED;
	for (int i = 0; i < 2; i++)
	{
		if (blank_data[i])
			free(blank_data[i]);
		blank_data[i] = NULL;
	}
	/* disable DACs and SCART voltage */
	Standby(true);
	if (fd >= 0)
		close(fd);
}

int cVideo::setAspectRatio(int aspect, int mode)
{
	static int _mode = -1;
	static int _aspect = -1;
	vidDispSize_t dsize = VID_DISPSIZE_UNKNOWN;
	vidDispMode_t dmode = VID_DISPMODE_NORM;
	/* 1 = 4:3, 3 = 16:9, 4 = 2.21:1, 0 = unknown */
	int v_ar = getAspectRatio();

	if (aspect != -1)
		_aspect = aspect;
	if (mode != -1)
		_mode = mode;
	hal_info("%s(%d, %d)_(%d, %d) v_ar %d\n", __FUNCTION__, aspect, mode, _aspect, _mode, v_ar);

	/* values are hardcoded in neutrino_menue.cpp, "2" is 14:9 -> not used */
	if (_aspect != -1)
	{
		switch(_aspect)
		{
		case 1:
			dsize = VID_DISPSIZE_4x3;
			scartvoltage = 12;
			break;
		case 3:
			dsize = VID_DISPSIZE_16x9;
			scartvoltage = 6;
			break;
		default:
			break;
		}
	}
	if (_mode != -1)
	{
		int zoom = 100 * 16 / 14; /* 16:9 vs 14:9 */
		switch(_mode)
		{
		case DISPLAY_AR_MODE_NONE:
			if (v_ar < 3)
				dsize = VID_DISPSIZE_4x3;
			else
				dsize = VID_DISPSIZE_16x9;
			break;
		case DISPLAY_AR_MODE_LETTERBOX:
			dmode = VID_DISPMODE_LETTERBOX;
			break;
		case DISPLAY_AR_MODE_PANSCAN:
			zoom = 100 * 5 / 4;
		case DISPLAY_AR_MODE_PANSCAN2:
			if ((v_ar < 3 && _aspect == 3) || (v_ar >= 3 && _aspect == 1))
			{
				/* unfortunately, this partly reimplements the setZoom code... */
				dsize = VID_DISPSIZE_UNKNOWN;
				dmode = VID_DISPMODE_SCALE;
				SCALEINFO s;
				memset(&s, 0, sizeof(s));
				if (v_ar < 3) { /* 4:3 */
					s.src.hori_size = 720;
					s.src.vert_size = 2 * 576 - 576 * zoom / 100;
					s.des.hori_size = zoom * 720 * 3/4 / 100;
					s.des.vert_size = 576;
				} else {
					s.src.hori_size = 2 * 720 - 720 * zoom / 100;
					s.src.vert_size = 576;
					s.des.hori_size = 720;
					s.des.vert_size = zoom * 576 * 3/4 / 100;
				}
				s.des.vert_off = (576 - s.des.vert_size) / 2;
				s.des.hori_off = (720 - s.des.hori_size) / 2;
				hal_debug("PANSCAN2: %d%% src: %d:%d:%d:%d dst: %d:%d:%d:%d\n", zoom,
					s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
					s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
				fop(ioctl, MPEG_VID_SCALE_ON);
				fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
			}
		default:
			break;
		}
		if (dmode != VID_DISPMODE_SCALE)
			fop(ioctl, MPEG_VID_SCALE_OFF);
		setCroppingMode(dmode);
	}
	const char *ds[] = { "4x3", "16x9", "2.21", "unknown" };
	const char *d;
	if (dsize >=0 && dsize < 4)
		d = ds[dsize];
	else
		d = "invalid!";
	hal_debug("%s dispsize(%d) (%s)\n", __FUNCTION__, dsize, d);
	fop(ioctl, MPEG_VID_SET_DISPSIZE, dsize);

	int avsfd = open("/dev/stb/tdsystem", O_RDONLY);
	if (avsfd < 0)
	{
		perror("open tdsystem");
		return 0;
	}
	if (!noscart && scartvoltage > 0 && video_standby == 0)
	{
		hal_info("%s set SCART_PIN_8 to %dV\n", __FUNCTION__, scartvoltage);
		if (ioctl(avsfd, IOC_AVS_SCART_PIN8_SET, scartvoltage) < 0)
			perror("IOC_AVS_SCART_PIN8_SET");
	}
	close(avsfd);
	return 0;
}

int cVideo::getAspectRatio(void)
{
	VIDEOINFO v;
	/* this memset silences *TONS* of valgrind warnings */
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);
	if (v.pel_aspect_ratio < VID_DISPSIZE_4x3 || v.pel_aspect_ratio > VID_DISPSIZE_UNKNOWN)
	{
		hal_info("%s invalid value %d, returning 0/unknown fd: %d", __FUNCTION__, v.pel_aspect_ratio, fd);
		return 0;
	}
	/* convert to Coolstream api values. Taken from streaminfo2.cpp */
	switch (v.pel_aspect_ratio)
	{
		case VID_DISPSIZE_4x3:
			return 1;
		case VID_DISPSIZE_16x9:
			return 3;
		case VID_DISPSIZE_221x100:
			return 4;
		default:
			return 0;
	}
}

int cVideo::setCroppingMode(vidDispMode_t format)
{
	croppingMode = format;
	const char *format_string[] = { "norm", "letterbox", "unknown", "mode_1_2", "mode_1_4", "mode_2x", "scale", "disexp" };
	const char *f;
	if (format >= VID_DISPMODE_NORM && format <= VID_DISPMODE_DISEXP)
		f = format_string[format];
	else
		f = "ILLEGAL format!";
	hal_debug("%s(%d) => %s\n", __FUNCTION__, format, f);
	return fop(ioctl, MPEG_VID_SET_DISPMODE, format);
}

int cVideo::Start(void * /*PcrChannel*/, unsigned short /*PcrPid*/, unsigned short /*VideoPid*/, void * /*hChannel*/)
{
	hal_debug("%s playstate=%d\n", __FUNCTION__, playstate);
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
	playstate = VIDEO_PLAYING;
	fop(ioctl, MPEG_VID_PLAY);
	return fop(ioctl, MPEG_VID_SYNC_ON, VID_SYNC_AUD);
}

int cVideo::Stop(bool blank)
{
	hal_debug("%s(%d)\n", __FUNCTION__, blank);
	if (blank)
	{
		playstate = VIDEO_STOPPED;
		fop(ioctl, MPEG_VID_STOP);
		return setBlank(1);
	}
	playstate = VIDEO_FREEZED;
	return fop(ioctl, MPEG_VID_FREEZE);
}

int cVideo::setBlank(int)
{
	hal_debug("%s\n", __FUNCTION__);
	/* The TripleDragon has no VIDEO_SET_BLANK ioctl.
	   instead, you write a black still-MPEG Iframe into the decoder.
	   The original software uses different files for 4:3 and 16:9 and
	   for PAL and NTSC. I optimized that a little bit
	 */
	int index = 0; /* default PAL */
	int ret = 0;
	VIDEOINFO v;
	BUFINFO buf;
	pthread_mutex_lock(&stillp_mutex);
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);

	if ((v.v_size % 240) == 0) /* NTSC */
	{
		hal_info("%s NTSC format detected", __FUNCTION__);
		index = 1;
	}

	if (blank_data[index] == NULL) /* no MPEG found */
	{
		ret = -1;
		goto out;
	}
	/* hack: this might work only on those two still-MPEG files!
	   I diff'ed the 4:3 and the 16:9 still mpeg from the original
	   soft and spotted the single bit difference, so there is no
	   need to keep two different MPEGs in memory
	   If we would read them from disk all the time it would be
	   slower and it might wake up the drive occasionally */
	if (v.pel_aspect_ratio == VID_DISPSIZE_4x3)
		((char *)blank_data[index])[7] &= ~0x10; // clear the bit
	else
		((char *)blank_data[index])[7] |=  0x10; // set the bit

	//WARN("blank[7] == 0x%02x", ((char *)blank_data[index])[7]);

	buf.ulLen = blank_size[index];
	buf.ulStartAdrOff = (int)blank_data[index];
	fop(ioctl, MPEG_VID_STILLP_WRITE, &buf);
	ret = fop(ioctl, MPEG_VID_SELECT_SOURCE, VID_SOURCE_DEMUX);
 out:
	pthread_mutex_unlock(&stillp_mutex);
	return ret;
}

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	hal_info("%s(%d, %d)\n", __FUNCTION__, video_system, remember);
	if (video_system > VID_DISPFMT_SECAM || video_system < 0)
		video_system = VID_DISPFMT_PAL;
        return fop(ioctl, MPEG_VID_SET_DISPFMT, video_system);
}

int cVideo::getPlayState(void)
{
	return playstate;
}

void cVideo::SetVideoMode(analog_mode_t mode)
{
	hal_debug("%s(%d)\n", __FUNCTION__, mode);
	switch(mode)
	{
		case ANALOG_SD_YPRPB_SCART:
			outputformat = VID_OUTFMT_YBR_SVIDEO;
			break;
		case ANALOG_SD_RGB_SCART:
			outputformat = VID_OUTFMT_RGBC_SVIDEO;
			break;
		default:
			hal_info("%s unknown mode %d\n", __FUNCTION__, mode);
			return;
	}
	fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
}

bool cVideo::ShowPicture(const char * fname)
{
	bool ret = false;
	hal_debug("%s(%s)\n", __FUNCTION__, fname);
	char destname[512];
	char cmd[512];
	char *p;
	void *data;
	int mfd;
	struct stat st, st2;
	strcpy(destname, "/var/cache");
	if (stat(fname, &st2))
	{
		hal_info("%s: could not stat %s (%m)\n", __func__, fname);
		return ret;
	}
	mkdir(destname, 0755);
	/* the cache filename is (example for /share/tuxbox/neutrino/icons/radiomode.jpg):
	   /var/cache/share.tuxbox.neutrino.icons.radiomode.jpg.m2v
	   build that filename first...
	   TODO: this could cause name clashes, use a hashing function instead... */
	strcat(destname, fname);
	p = &destname[strlen("/var/cache/")];
	while ((p = strchr(p, '/')) != NULL)
		*p = '.';
	strcat(destname, ".m2v");
	/* ...then check if it exists already... */
	if (stat(destname, &st) || (st.st_mtime != st2.st_mtime) || (st.st_size == 0))
	{
		struct utimbuf u;
		u.actime = time(NULL);
		u.modtime = st2.st_mtime;
		/* it does not exist or has a different date, so call ffmpeg... */
		sprintf(cmd, "ffmpeg -y -f mjpeg -i '%s' -s 704x576 '%s' </dev/null",
							fname, destname);
		system(cmd); /* TODO: use libavcodec to directly convert it */
		utime(destname, &u);
	}
	/* the mutex is a workaround: setBlank is apparently called from
	   a differnt thread and takes slightly longer, so that the decoder
	   was blanked immediately after displaying the image, which is not
	   what we want. the mutex ensures proper ordering. */
	pthread_mutex_lock(&stillp_mutex);
	mfd = open(destname, O_RDONLY);
	if (mfd < 0)
	{
		hal_info("%s cannot open %s: %m", __FUNCTION__, destname);
		goto out;
	}
	if (fstat(mfd, &st) != -1 && st.st_size > 0)
	{
		data = malloc(st.st_size);
		if (! data)
			hal_info("%s malloc failed (%m)\n", __FUNCTION__);
		else if (read(mfd, data, st.st_size) != st.st_size)
			hal_info("%s short read (%m)\n", __FUNCTION__);
		else
		{
			BUFINFO buf;
			buf.ulLen = st.st_size;
			buf.ulStartAdrOff = (int)data;
			Stop(false);
			fop(ioctl, MPEG_VID_STILLP_WRITE, &buf);
			ret = true;
		}
		free(data);
	}
	close(mfd);
 out:
	pthread_mutex_unlock(&stillp_mutex);
	return ret;
#if 0
	/* DirectFB based picviewer: works, but is slow and the infobar
	   draws in the same plane */
	int width;
	int height;
	if (!fname)
		return;

	IDirectFBImageProvider *provider;
	DFBResult err = dfb->CreateImageProvider(dfb, fname, &provider);
	if (err)
	{
		fprintf(stderr, "cVideo::ShowPicture: CreateImageProvider error!\n");
		return;
	}

	DFBSurfaceDescription desc;
	provider->GetSurfaceDescription (provider, &desc);
	width = desc.width;
	height = desc.height;
	provider->RenderTo(provider, dfbdest, NULL);
	provider->Release(provider);
#endif
}

void cVideo::StopPicture()
{
	hal_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_SELECT_SOURCE, VID_SOURCE_DEMUX);
}

void cVideo::Standby(unsigned int bOn)
{
	hal_debug("%s(%d)\n", __FUNCTION__, bOn);
	if (bOn)
	{
		setBlank(1);
		fop(ioctl, MPEG_VID_SET_OUTFMT, VID_OUTFMT_DISABLE_DACS);
	} else
		fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
	routeVideo(bOn);
	video_standby = bOn;
}

int cVideo::getBlank(void)
{
	VIDEOINFO v;
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);
	/* HACK HACK HACK :-)
	 * setBlank() puts a 24fps black mpeg into the decoder...
	 * regular broadcast does not have 24fps, so if it is still
	 * there, video did not decode... */
	hal_debug("%s: %hu (blank = 2)\n", __func__, v.frame_rate);
	return (v.frame_rate == 2);
}

/* set zoom in percent (100% == 1:1) */
int cVideo::setZoom(int zoom)
{
	if (zoom == -1) // "auto" reset
		zoom = *zoomvalue;

	if (zoom > 150 || zoom < 100)
		return -1;

	*zoomvalue = zoom;

	if (zoom == 100)
	{
		setCroppingMode(croppingMode);
		return fop(ioctl, MPEG_VID_SCALE_OFF);
	}

	/* the SCALEINFO describes the source and destination of the scaled
	   video. "src" is the part of the source picture that gets scaled,
	   "dst" is the area on the screen where this part is displayed
	   Messing around with MPEG_VID_SET_SCALE_POS disables the automatic
	   letterboxing, which, as I guess, is only a special case of
	   MPEG_VID_SET_SCALE_POS. Therefor we need to care for letterboxing
	   etc here, which is probably not yet totally correct */
	SCALEINFO s;
	memset(&s, 0, sizeof(s));
	if (zoom > 100)
	{
		/* 1 = 4:3, 3 = 16:9, 4 = 2.21:1, 0 = unknown */
		int x = getAspectRatio();
		if (x < 3 && croppingMode == VID_DISPMODE_NORM)
		{
			s.src.hori_size = 720;
			s.des.hori_size = 720 * 3/4 * zoom / 100;
			if (s.des.hori_size > 720)
			{
				/* the destination exceeds the screen size.
				   TODO: decrease source size to allow higher
				   zoom factors (is this useful ?) */
				s.des.hori_size = 720;
				zoom = 133; // (720*4*100)/(720*3)
				*zoomvalue = zoom;
			}
		}
		else
		{
			s.src.hori_size = 2 * 720 - 720 * zoom / 100;
			s.des.hori_size = 720;
		}
		s.src.vert_size = 2 * 576 - 576 * zoom / 100;
		s.des.hori_off = (720 - s.des.hori_size) / 2;
		s.des.vert_size = 576;
	}
/* not working correctly (wrong formula) and does not make sense IMHO
	else
	{
		s.src.hori_size = 720;
		s.src.vert_size = 576;
		s.des.hori_size = 720 * zoom / 100;
		s.des.vert_size = 576 * zoom / 100;
		s.des.hori_off = (720 - s.des.hori_size) / 2;
		s.des.vert_off = (576 - s.des.vert_size) / 2;
	}
 */
	hal_debug("%s %d%% src: %d:%d:%d:%d dst: %d:%d:%d:%d\n", __FUNCTION__, zoom,
		s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
		s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
	fop(ioctl, MPEG_VID_SET_DISPMODE, VID_DISPMODE_SCALE);
	fop(ioctl, MPEG_VID_SCALE_ON);
	return fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
}

#if 0
int cVideo::getZoom(void)
{
	return *zoomvalue;
}

void cVideo::setZoomAspect(int index)
{
	if (index < 0 || index > 1)
		WARN("index out of range");
	else
		zoomvalue = &z[index];
}
#endif

/* this function is regularly called, checks if video parameters
   changed and triggers appropriate actions */
void cVideo::VideoParamWatchdog(void)
{
	static unsigned int _v_info = (unsigned int) -1;
	unsigned int v_info;
	if (fd == -1)
		return;
	ioctl(fd, MPEG_VID_GET_V_INFO_RAW, &v_info);
	if (_v_info != v_info)
	{
		hal_debug("%s params changed. old: %08x new: %08x\n", __FUNCTION__, _v_info, v_info);
		setAspectRatio(-1, -1);
	}
	_v_info = v_info;
}

void cVideo::Pig(int x, int y, int w, int h, int /*osd_w*/, int /*osd_h*/)
{
	/* x = y = w = h = -1 -> reset / "hide" PIG */
	if (x == -1 && y == -1 && w == -1 && h == -1)
	{
		setZoom(-1);
		setAspectRatio(-1, -1);
		return;
	}
	SCALEINFO s;
	memset(&s, 0, sizeof(s));
	s.src.hori_size = 720;
	s.src.vert_size = 576;
	s.des.hori_off = x;
	s.des.vert_off = y;
	s.des.hori_size = w;
	s.des.vert_size = h;
	hal_debug("%s src: %d:%d:%d:%d dst: %d:%d:%d:%d", __FUNCTION__,
		s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
		s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
	fop(ioctl, MPEG_VID_SET_DISPMODE, VID_DISPMODE_SCALE);
	fop(ioctl, MPEG_VID_SCALE_ON);
	fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	VIDEOINFO v;
	/* this memset silences *TONS* of valgrind warnings */
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);
	/* convert to Coolstream API */
	rate = (int)v.frame_rate - 1;
	width = (int)v.h_size;
	height = (int)v.v_size;
}

void cVideo::SetSyncMode(AVSYNC_TYPE Mode)
{
	hal_debug("%s %d\n", __FUNCTION__, Mode);
	/*
	 * { 0, LOCALE_OPTIONS_OFF },
	 * { 1, LOCALE_OPTIONS_ON  },
	 * { 2, LOCALE_AUDIOMENU_AVSYNC_AM }
	 */
	switch(Mode)
	{
		case 0:
			ioctl(fd, MPEG_VID_SYNC_OFF);
			break;
		case 1:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_VID);
			break;
		default:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_AUD);
			break;
	}
};

int cVideo::SetStreamType(VIDEO_FORMAT type)
{
	static const char *VF[] = {
		"VIDEO_FORMAT_MPEG2",
		"VIDEO_FORMAT_MPEG4",
		"VIDEO_FORMAT_VC1",
		"VIDEO_FORMAT_JPEG",
		"VIDEO_FORMAT_GIF",
		"VIDEO_FORMAT_PNG"
	};

	hal_debug("%s type=%s\n", __FUNCTION__, VF[type]);
	return 0;
}

void cVideo::routeVideo(int standby)
{
	hal_debug("%s(%d)\n", __FUNCTION__, standby);

	int avsfd = open("/dev/stb/tdsystem", O_RDONLY);
	if (avsfd < 0)
	{
		perror("open tdsystem");
		return;
	}

	/* in standby, we always route VCR scart to the TV. Once there is a UI
	   to configure this, we can think more about this... */
	if (standby)
	{
		hal_info("%s set fastblank and pin8 to follow VCR SCART, route VCR to TV\n", __FUNCTION__);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, (unsigned char)3) < 0)
			perror("IOC_AVS_FASTBLANK_SET, 3");
		/* TODO: should probably depend on aspect ratio setting */
		if (ioctl(avsfd, IOC_AVS_SCART_PIN8_FOLLOW_VCR) < 0)
			perror("IOC_AVS_SCART_PIN8_FOLLOW_VCR");
		if (ioctl(avsfd, IOC_AVS_ROUTE_VCR2TV) < 0)
			perror("IOC_AVS_ROUTE_VCR2TV");
	} else {
		unsigned char fblk = 1;
		hal_info("%s set fastblank=%d pin8=%dV, route encoder to TV\n", __FUNCTION__, fblk, scartvoltage);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, fblk) < 0)
			perror("IOC_AVS_FASTBLANK_SET, fblk");
		if (!noscart && ioctl(avsfd, IOC_AVS_SCART_PIN8_SET, scartvoltage) < 0)
			perror("IOC_AVS_SCART_PIN8_SET");
		if (ioctl(avsfd, IOC_AVS_ROUTE_ENC2TV) < 0)
			perror("IOC_AVS_ROUTE_ENC2TV");
	}
	close(avsfd);
}

void cVideo::FastForwardMode(int mode)
{
	hal_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_FASTFORWARD, mode);
}

/* get an image of the video screen
 * this code is inspired by dreambox AIO-grab,
 * git://schwerkraft.elitedvb.net/aio-grab/aio-grab.git */
/* static lookup tables for faster yuv2rgb conversion */
static const int yuv2rgbtable_y[256] = {
	0xFFED5EA0, 0xFFEE88B6, 0xFFEFB2CC, 0xFFF0DCE2, 0xFFF206F8, 0xFFF3310E, 0xFFF45B24, 0xFFF5853A,
	0xFFF6AF50, 0xFFF7D966, 0xFFF9037C, 0xFFFA2D92, 0xFFFB57A8, 0xFFFC81BE, 0xFFFDABD4, 0xFFFED5EA,
	0x00000000, 0x00012A16, 0x0002542C, 0x00037E42, 0x0004A858, 0x0005D26E, 0x0006FC84, 0x0008269A,
	0x000950B0, 0x000A7AC6, 0x000BA4DC, 0x000CCEF2, 0x000DF908, 0x000F231E, 0x00104D34, 0x0011774A,
	0x0012A160, 0x0013CB76, 0x0014F58C, 0x00161FA2, 0x001749B8, 0x001873CE, 0x00199DE4, 0x001AC7FA,
	0x001BF210, 0x001D1C26, 0x001E463C, 0x001F7052, 0x00209A68, 0x0021C47E, 0x0022EE94, 0x002418AA,
	0x002542C0, 0x00266CD6, 0x002796EC, 0x0028C102, 0x0029EB18, 0x002B152E, 0x002C3F44, 0x002D695A,
	0x002E9370, 0x002FBD86, 0x0030E79C, 0x003211B2, 0x00333BC8, 0x003465DE, 0x00358FF4, 0x0036BA0A,
	0x0037E420, 0x00390E36, 0x003A384C, 0x003B6262, 0x003C8C78, 0x003DB68E, 0x003EE0A4, 0x00400ABA,
	0x004134D0, 0x00425EE6, 0x004388FC, 0x0044B312, 0x0045DD28, 0x0047073E, 0x00483154, 0x00495B6A,
	0x004A8580, 0x004BAF96, 0x004CD9AC, 0x004E03C2, 0x004F2DD8, 0x005057EE, 0x00518204, 0x0052AC1A,
	0x0053D630, 0x00550046, 0x00562A5C, 0x00575472, 0x00587E88, 0x0059A89E, 0x005AD2B4, 0x005BFCCA,
	0x005D26E0, 0x005E50F6, 0x005F7B0C, 0x0060A522, 0x0061CF38, 0x0062F94E, 0x00642364, 0x00654D7A,
	0x00667790, 0x0067A1A6, 0x0068CBBC, 0x0069F5D2, 0x006B1FE8, 0x006C49FE, 0x006D7414, 0x006E9E2A,
	0x006FC840, 0x0070F256, 0x00721C6C, 0x00734682, 0x00747098, 0x00759AAE, 0x0076C4C4, 0x0077EEDA,
	0x007918F0, 0x007A4306, 0x007B6D1C, 0x007C9732, 0x007DC148, 0x007EEB5E, 0x00801574, 0x00813F8A,
	0x008269A0, 0x008393B6, 0x0084BDCC, 0x0085E7E2, 0x008711F8, 0x00883C0E, 0x00896624, 0x008A903A,
	0x008BBA50, 0x008CE466, 0x008E0E7C, 0x008F3892, 0x009062A8, 0x00918CBE, 0x0092B6D4, 0x0093E0EA,
	0x00950B00, 0x00963516, 0x00975F2C, 0x00988942, 0x0099B358, 0x009ADD6E, 0x009C0784, 0x009D319A,
	0x009E5BB0, 0x009F85C6, 0x00A0AFDC, 0x00A1D9F2, 0x00A30408, 0x00A42E1E, 0x00A55834, 0x00A6824A,
	0x00A7AC60, 0x00A8D676, 0x00AA008C, 0x00AB2AA2, 0x00AC54B8, 0x00AD7ECE, 0x00AEA8E4, 0x00AFD2FA,
	0x00B0FD10, 0x00B22726, 0x00B3513C, 0x00B47B52, 0x00B5A568, 0x00B6CF7E, 0x00B7F994, 0x00B923AA,
	0x00BA4DC0, 0x00BB77D6, 0x00BCA1EC, 0x00BDCC02, 0x00BEF618, 0x00C0202E, 0x00C14A44, 0x00C2745A,
	0x00C39E70, 0x00C4C886, 0x00C5F29C, 0x00C71CB2, 0x00C846C8, 0x00C970DE, 0x00CA9AF4, 0x00CBC50A,
	0x00CCEF20, 0x00CE1936, 0x00CF434C, 0x00D06D62, 0x00D19778, 0x00D2C18E, 0x00D3EBA4, 0x00D515BA,
	0x00D63FD0, 0x00D769E6, 0x00D893FC, 0x00D9BE12, 0x00DAE828, 0x00DC123E, 0x00DD3C54, 0x00DE666A,
	0x00DF9080, 0x00E0BA96, 0x00E1E4AC, 0x00E30EC2, 0x00E438D8, 0x00E562EE, 0x00E68D04, 0x00E7B71A,
	0x00E8E130, 0x00EA0B46, 0x00EB355C, 0x00EC5F72, 0x00ED8988, 0x00EEB39E, 0x00EFDDB4, 0x00F107CA,
	0x00F231E0, 0x00F35BF6, 0x00F4860C, 0x00F5B022, 0x00F6DA38, 0x00F8044E, 0x00F92E64, 0x00FA587A,
	0x00FB8290, 0x00FCACA6, 0x00FDD6BC, 0x00FF00D2, 0x01002AE8, 0x010154FE, 0x01027F14, 0x0103A92A,
	0x0104D340, 0x0105FD56, 0x0107276C, 0x01085182, 0x01097B98, 0x010AA5AE, 0x010BCFC4, 0x010CF9DA,
	0x010E23F0, 0x010F4E06, 0x0110781C, 0x0111A232, 0x0112CC48, 0x0113F65E, 0x01152074, 0x01164A8A
};
static const int yuv2rgbtable_ru[256] = {
	0xFEFDA500, 0xFEFFA9B6, 0xFF01AE6C, 0xFF03B322, 0xFF05B7D8, 0xFF07BC8E, 0xFF09C144, 0xFF0BC5FA,
	0xFF0DCAB0, 0xFF0FCF66, 0xFF11D41C, 0xFF13D8D2, 0xFF15DD88, 0xFF17E23E, 0xFF19E6F4, 0xFF1BEBAA,
	0xFF1DF060, 0xFF1FF516, 0xFF21F9CC, 0xFF23FE82, 0xFF260338, 0xFF2807EE, 0xFF2A0CA4, 0xFF2C115A,
	0xFF2E1610, 0xFF301AC6, 0xFF321F7C, 0xFF342432, 0xFF3628E8, 0xFF382D9E, 0xFF3A3254, 0xFF3C370A,
	0xFF3E3BC0, 0xFF404076, 0xFF42452C, 0xFF4449E2, 0xFF464E98, 0xFF48534E, 0xFF4A5804, 0xFF4C5CBA,
	0xFF4E6170, 0xFF506626, 0xFF526ADC, 0xFF546F92, 0xFF567448, 0xFF5878FE, 0xFF5A7DB4, 0xFF5C826A,
	0xFF5E8720, 0xFF608BD6, 0xFF62908C, 0xFF649542, 0xFF6699F8, 0xFF689EAE, 0xFF6AA364, 0xFF6CA81A,
	0xFF6EACD0, 0xFF70B186, 0xFF72B63C, 0xFF74BAF2, 0xFF76BFA8, 0xFF78C45E, 0xFF7AC914, 0xFF7CCDCA,
	0xFF7ED280, 0xFF80D736, 0xFF82DBEC, 0xFF84E0A2, 0xFF86E558, 0xFF88EA0E, 0xFF8AEEC4, 0xFF8CF37A,
	0xFF8EF830, 0xFF90FCE6, 0xFF93019C, 0xFF950652, 0xFF970B08, 0xFF990FBE, 0xFF9B1474, 0xFF9D192A,
	0xFF9F1DE0, 0xFFA12296, 0xFFA3274C, 0xFFA52C02, 0xFFA730B8, 0xFFA9356E, 0xFFAB3A24, 0xFFAD3EDA,
	0xFFAF4390, 0xFFB14846, 0xFFB34CFC, 0xFFB551B2, 0xFFB75668, 0xFFB95B1E, 0xFFBB5FD4, 0xFFBD648A,
	0xFFBF6940, 0xFFC16DF6, 0xFFC372AC, 0xFFC57762, 0xFFC77C18, 0xFFC980CE, 0xFFCB8584, 0xFFCD8A3A,
	0xFFCF8EF0, 0xFFD193A6, 0xFFD3985C, 0xFFD59D12, 0xFFD7A1C8, 0xFFD9A67E, 0xFFDBAB34, 0xFFDDAFEA,
	0xFFDFB4A0, 0xFFE1B956, 0xFFE3BE0C, 0xFFE5C2C2, 0xFFE7C778, 0xFFE9CC2E, 0xFFEBD0E4, 0xFFEDD59A,
	0xFFEFDA50, 0xFFF1DF06, 0xFFF3E3BC, 0xFFF5E872, 0xFFF7ED28, 0xFFF9F1DE, 0xFFFBF694, 0xFFFDFB4A,
	0x00000000, 0x000204B6, 0x0004096C, 0x00060E22, 0x000812D8, 0x000A178E, 0x000C1C44, 0x000E20FA,
	0x001025B0, 0x00122A66, 0x00142F1C, 0x001633D2, 0x00183888, 0x001A3D3E, 0x001C41F4, 0x001E46AA,
	0x00204B60, 0x00225016, 0x002454CC, 0x00265982, 0x00285E38, 0x002A62EE, 0x002C67A4, 0x002E6C5A,
	0x00307110, 0x003275C6, 0x00347A7C, 0x00367F32, 0x003883E8, 0x003A889E, 0x003C8D54, 0x003E920A,
	0x004096C0, 0x00429B76, 0x0044A02C, 0x0046A4E2, 0x0048A998, 0x004AAE4E, 0x004CB304, 0x004EB7BA,
	0x0050BC70, 0x0052C126, 0x0054C5DC, 0x0056CA92, 0x0058CF48, 0x005AD3FE, 0x005CD8B4, 0x005EDD6A,
	0x0060E220, 0x0062E6D6, 0x0064EB8C, 0x0066F042, 0x0068F4F8, 0x006AF9AE, 0x006CFE64, 0x006F031A,
	0x007107D0, 0x00730C86, 0x0075113C, 0x007715F2, 0x00791AA8, 0x007B1F5E, 0x007D2414, 0x007F28CA,
	0x00812D80, 0x00833236, 0x008536EC, 0x00873BA2, 0x00894058, 0x008B450E, 0x008D49C4, 0x008F4E7A,
	0x00915330, 0x009357E6, 0x00955C9C, 0x00976152, 0x00996608, 0x009B6ABE, 0x009D6F74, 0x009F742A,
	0x00A178E0, 0x00A37D96, 0x00A5824C, 0x00A78702, 0x00A98BB8, 0x00AB906E, 0x00AD9524, 0x00AF99DA,
	0x00B19E90, 0x00B3A346, 0x00B5A7FC, 0x00B7ACB2, 0x00B9B168, 0x00BBB61E, 0x00BDBAD4, 0x00BFBF8A,
	0x00C1C440, 0x00C3C8F6, 0x00C5CDAC, 0x00C7D262, 0x00C9D718, 0x00CBDBCE, 0x00CDE084, 0x00CFE53A,
	0x00D1E9F0, 0x00D3EEA6, 0x00D5F35C, 0x00D7F812, 0x00D9FCC8, 0x00DC017E, 0x00DE0634, 0x00E00AEA,
	0x00E20FA0, 0x00E41456, 0x00E6190C, 0x00E81DC2, 0x00EA2278, 0x00EC272E, 0x00EE2BE4, 0x00F0309A,
	0x00F23550, 0x00F43A06, 0x00F63EBC, 0x00F84372, 0x00FA4828, 0x00FC4CDE, 0x00FE5194, 0x00100564A
};
static const int yuv2rgbtable_gu[256] = {
	0xFFCDD300, 0xFFCE375A, 0xFFCE9BB4, 0xFFCF000E, 0xFFCF6468, 0xFFCFC8C2, 0xFFD02D1C, 0xFFD09176,
	0xFFD0F5D0, 0xFFD15A2A, 0xFFD1BE84, 0xFFD222DE, 0xFFD28738, 0xFFD2EB92, 0xFFD34FEC, 0xFFD3B446,
	0xFFD418A0, 0xFFD47CFA, 0xFFD4E154, 0xFFD545AE, 0xFFD5AA08, 0xFFD60E62, 0xFFD672BC, 0xFFD6D716,
	0xFFD73B70, 0xFFD79FCA, 0xFFD80424, 0xFFD8687E, 0xFFD8CCD8, 0xFFD93132, 0xFFD9958C, 0xFFD9F9E6,
	0xFFDA5E40, 0xFFDAC29A, 0xFFDB26F4, 0xFFDB8B4E, 0xFFDBEFA8, 0xFFDC5402, 0xFFDCB85C, 0xFFDD1CB6,
	0xFFDD8110, 0xFFDDE56A, 0xFFDE49C4, 0xFFDEAE1E, 0xFFDF1278, 0xFFDF76D2, 0xFFDFDB2C, 0xFFE03F86,
	0xFFE0A3E0, 0xFFE1083A, 0xFFE16C94, 0xFFE1D0EE, 0xFFE23548, 0xFFE299A2, 0xFFE2FDFC, 0xFFE36256,
	0xFFE3C6B0, 0xFFE42B0A, 0xFFE48F64, 0xFFE4F3BE, 0xFFE55818, 0xFFE5BC72, 0xFFE620CC, 0xFFE68526,
	0xFFE6E980, 0xFFE74DDA, 0xFFE7B234, 0xFFE8168E, 0xFFE87AE8, 0xFFE8DF42, 0xFFE9439C, 0xFFE9A7F6,
	0xFFEA0C50, 0xFFEA70AA, 0xFFEAD504, 0xFFEB395E, 0xFFEB9DB8, 0xFFEC0212, 0xFFEC666C, 0xFFECCAC6,
	0xFFED2F20, 0xFFED937A, 0xFFEDF7D4, 0xFFEE5C2E, 0xFFEEC088, 0xFFEF24E2, 0xFFEF893C, 0xFFEFED96,
	0xFFF051F0, 0xFFF0B64A, 0xFFF11AA4, 0xFFF17EFE, 0xFFF1E358, 0xFFF247B2, 0xFFF2AC0C, 0xFFF31066,
	0xFFF374C0, 0xFFF3D91A, 0xFFF43D74, 0xFFF4A1CE, 0xFFF50628, 0xFFF56A82, 0xFFF5CEDC, 0xFFF63336,
	0xFFF69790, 0xFFF6FBEA, 0xFFF76044, 0xFFF7C49E, 0xFFF828F8, 0xFFF88D52, 0xFFF8F1AC, 0xFFF95606,
	0xFFF9BA60, 0xFFFA1EBA, 0xFFFA8314, 0xFFFAE76E, 0xFFFB4BC8, 0xFFFBB022, 0xFFFC147C, 0xFFFC78D6,
	0xFFFCDD30, 0xFFFD418A, 0xFFFDA5E4, 0xFFFE0A3E, 0xFFFE6E98, 0xFFFED2F2, 0xFFFF374C, 0xFFFF9BA6,
	0x00000000, 0x0000645A, 0x0000C8B4, 0x00012D0E, 0x00019168, 0x0001F5C2, 0x00025A1C, 0x0002BE76,
	0x000322D0, 0x0003872A, 0x0003EB84, 0x00044FDE, 0x0004B438, 0x00051892, 0x00057CEC, 0x0005E146,
	0x000645A0, 0x0006A9FA, 0x00070E54, 0x000772AE, 0x0007D708, 0x00083B62, 0x00089FBC, 0x00090416,
	0x00096870, 0x0009CCCA, 0x000A3124, 0x000A957E, 0x000AF9D8, 0x000B5E32, 0x000BC28C, 0x000C26E6,
	0x000C8B40, 0x000CEF9A, 0x000D53F4, 0x000DB84E, 0x000E1CA8, 0x000E8102, 0x000EE55C, 0x000F49B6,
	0x000FAE10, 0x0010126A, 0x001076C4, 0x0010DB1E, 0x00113F78, 0x0011A3D2, 0x0012082C, 0x00126C86,
	0x0012D0E0, 0x0013353A, 0x00139994, 0x0013FDEE, 0x00146248, 0x0014C6A2, 0x00152AFC, 0x00158F56,
	0x0015F3B0, 0x0016580A, 0x0016BC64, 0x001720BE, 0x00178518, 0x0017E972, 0x00184DCC, 0x0018B226,
	0x00191680, 0x00197ADA, 0x0019DF34, 0x001A438E, 0x001AA7E8, 0x001B0C42, 0x001B709C, 0x001BD4F6,
	0x001C3950, 0x001C9DAA, 0x001D0204, 0x001D665E, 0x001DCAB8, 0x001E2F12, 0x001E936C, 0x001EF7C6,
	0x001F5C20, 0x001FC07A, 0x002024D4, 0x0020892E, 0x0020ED88, 0x002151E2, 0x0021B63C, 0x00221A96,
	0x00227EF0, 0x0022E34A, 0x002347A4, 0x0023ABFE, 0x00241058, 0x002474B2, 0x0024D90C, 0x00253D66,
	0x0025A1C0, 0x0026061A, 0x00266A74, 0x0026CECE, 0x00273328, 0x00279782, 0x0027FBDC, 0x00286036,
	0x0028C490, 0x002928EA, 0x00298D44, 0x0029F19E, 0x002A55F8, 0x002ABA52, 0x002B1EAC, 0x002B8306,
	0x002BE760, 0x002C4BBA, 0x002CB014, 0x002D146E, 0x002D78C8, 0x002DDD22, 0x002E417C, 0x002EA5D6,
	0x002F0A30, 0x002F6E8A, 0x002FD2E4, 0x0030373E, 0x00309B98, 0x0030FFF2, 0x0031644C, 0x0031C8A6
};
static const int yuv2rgbtable_gv[256] = {
	0xFF97E900, 0xFF98B92E, 0xFF99895C, 0xFF9A598A, 0xFF9B29B8, 0xFF9BF9E6, 0xFF9CCA14, 0xFF9D9A42,
	0xFF9E6A70, 0xFF9F3A9E, 0xFFA00ACC, 0xFFA0DAFA, 0xFFA1AB28, 0xFFA27B56, 0xFFA34B84, 0xFFA41BB2,
	0xFFA4EBE0, 0xFFA5BC0E, 0xFFA68C3C, 0xFFA75C6A, 0xFFA82C98, 0xFFA8FCC6, 0xFFA9CCF4, 0xFFAA9D22,
	0xFFAB6D50, 0xFFAC3D7E, 0xFFAD0DAC, 0xFFADDDDA, 0xFFAEAE08, 0xFFAF7E36, 0xFFB04E64, 0xFFB11E92,
	0xFFB1EEC0, 0xFFB2BEEE, 0xFFB38F1C, 0xFFB45F4A, 0xFFB52F78, 0xFFB5FFA6, 0xFFB6CFD4, 0xFFB7A002,
	0xFFB87030, 0xFFB9405E, 0xFFBA108C, 0xFFBAE0BA, 0xFFBBB0E8, 0xFFBC8116, 0xFFBD5144, 0xFFBE2172,
	0xFFBEF1A0, 0xFFBFC1CE, 0xFFC091FC, 0xFFC1622A, 0xFFC23258, 0xFFC30286, 0xFFC3D2B4, 0xFFC4A2E2,
	0xFFC57310, 0xFFC6433E, 0xFFC7136C, 0xFFC7E39A, 0xFFC8B3C8, 0xFFC983F6, 0xFFCA5424, 0xFFCB2452,
	0xFFCBF480, 0xFFCCC4AE, 0xFFCD94DC, 0xFFCE650A, 0xFFCF3538, 0xFFD00566, 0xFFD0D594, 0xFFD1A5C2,
	0xFFD275F0, 0xFFD3461E, 0xFFD4164C, 0xFFD4E67A, 0xFFD5B6A8, 0xFFD686D6, 0xFFD75704, 0xFFD82732,
	0xFFD8F760, 0xFFD9C78E, 0xFFDA97BC, 0xFFDB67EA, 0xFFDC3818, 0xFFDD0846, 0xFFDDD874, 0xFFDEA8A2,
	0xFFDF78D0, 0xFFE048FE, 0xFFE1192C, 0xFFE1E95A, 0xFFE2B988, 0xFFE389B6, 0xFFE459E4, 0xFFE52A12,
	0xFFE5FA40, 0xFFE6CA6E, 0xFFE79A9C, 0xFFE86ACA, 0xFFE93AF8, 0xFFEA0B26, 0xFFEADB54, 0xFFEBAB82,
	0xFFEC7BB0, 0xFFED4BDE, 0xFFEE1C0C, 0xFFEEEC3A, 0xFFEFBC68, 0xFFF08C96, 0xFFF15CC4, 0xFFF22CF2,
	0xFFF2FD20, 0xFFF3CD4E, 0xFFF49D7C, 0xFFF56DAA, 0xFFF63DD8, 0xFFF70E06, 0xFFF7DE34, 0xFFF8AE62,
	0xFFF97E90, 0xFFFA4EBE, 0xFFFB1EEC, 0xFFFBEF1A, 0xFFFCBF48, 0xFFFD8F76, 0xFFFE5FA4, 0xFFFF2FD2,
	0x00000000, 0x0000D02E, 0x0001A05C, 0x0002708A, 0x000340B8, 0x000410E6, 0x0004E114, 0x0005B142,
	0x00068170, 0x0007519E, 0x000821CC, 0x0008F1FA, 0x0009C228, 0x000A9256, 0x000B6284, 0x000C32B2,
	0x000D02E0, 0x000DD30E, 0x000EA33C, 0x000F736A, 0x00104398, 0x001113C6, 0x0011E3F4, 0x0012B422,
	0x00138450, 0x0014547E, 0x001524AC, 0x0015F4DA, 0x0016C508, 0x00179536, 0x00186564, 0x00193592,
	0x001A05C0, 0x001AD5EE, 0x001BA61C, 0x001C764A, 0x001D4678, 0x001E16A6, 0x001EE6D4, 0x001FB702,
	0x00208730, 0x0021575E, 0x0022278C, 0x0022F7BA, 0x0023C7E8, 0x00249816, 0x00256844, 0x00263872,
	0x002708A0, 0x0027D8CE, 0x0028A8FC, 0x0029792A, 0x002A4958, 0x002B1986, 0x002BE9B4, 0x002CB9E2,
	0x002D8A10, 0x002E5A3E, 0x002F2A6C, 0x002FFA9A, 0x0030CAC8, 0x00319AF6, 0x00326B24, 0x00333B52,
	0x00340B80, 0x0034DBAE, 0x0035ABDC, 0x00367C0A, 0x00374C38, 0x00381C66, 0x0038EC94, 0x0039BCC2,
	0x003A8CF0, 0x003B5D1E, 0x003C2D4C, 0x003CFD7A, 0x003DCDA8, 0x003E9DD6, 0x003F6E04, 0x00403E32,
	0x00410E60, 0x0041DE8E, 0x0042AEBC, 0x00437EEA, 0x00444F18, 0x00451F46, 0x0045EF74, 0x0046BFA2,
	0x00478FD0, 0x00485FFE, 0x0049302C, 0x004A005A, 0x004AD088, 0x004BA0B6, 0x004C70E4, 0x004D4112,
	0x004E1140, 0x004EE16E, 0x004FB19C, 0x005081CA, 0x005151F8, 0x00522226, 0x0052F254, 0x0053C282,
	0x005492B0, 0x005562DE, 0x0056330C, 0x0057033A, 0x0057D368, 0x0058A396, 0x005973C4, 0x005A43F2,
	0x005B1420, 0x005BE44E, 0x005CB47C, 0x005D84AA, 0x005E54D8, 0x005F2506, 0x005FF534, 0x0060C562,
	0x00619590, 0x006265BE, 0x006335EC, 0x0064061A, 0x0064D648, 0x0065A676, 0x006676A4, 0x006746D2
};
static const int yuv2rgbtable_bv[256] = {
	0xFF33A280, 0xFF353B3B, 0xFF36D3F6, 0xFF386CB1, 0xFF3A056C, 0xFF3B9E27, 0xFF3D36E2, 0xFF3ECF9D,
	0xFF406858, 0xFF420113, 0xFF4399CE, 0xFF453289, 0xFF46CB44, 0xFF4863FF, 0xFF49FCBA, 0xFF4B9575,
	0xFF4D2E30, 0xFF4EC6EB, 0xFF505FA6, 0xFF51F861, 0xFF53911C, 0xFF5529D7, 0xFF56C292, 0xFF585B4D,
	0xFF59F408, 0xFF5B8CC3, 0xFF5D257E, 0xFF5EBE39, 0xFF6056F4, 0xFF61EFAF, 0xFF63886A, 0xFF652125,
	0xFF66B9E0, 0xFF68529B, 0xFF69EB56, 0xFF6B8411, 0xFF6D1CCC, 0xFF6EB587, 0xFF704E42, 0xFF71E6FD,
	0xFF737FB8, 0xFF751873, 0xFF76B12E, 0xFF7849E9, 0xFF79E2A4, 0xFF7B7B5F, 0xFF7D141A, 0xFF7EACD5,
	0xFF804590, 0xFF81DE4B, 0xFF837706, 0xFF850FC1, 0xFF86A87C, 0xFF884137, 0xFF89D9F2, 0xFF8B72AD,
	0xFF8D0B68, 0xFF8EA423, 0xFF903CDE, 0xFF91D599, 0xFF936E54, 0xFF95070F, 0xFF969FCA, 0xFF983885,
	0xFF99D140, 0xFF9B69FB, 0xFF9D02B6, 0xFF9E9B71, 0xFFA0342C, 0xFFA1CCE7, 0xFFA365A2, 0xFFA4FE5D,
	0xFFA69718, 0xFFA82FD3, 0xFFA9C88E, 0xFFAB6149, 0xFFACFA04, 0xFFAE92BF, 0xFFB02B7A, 0xFFB1C435,
	0xFFB35CF0, 0xFFB4F5AB, 0xFFB68E66, 0xFFB82721, 0xFFB9BFDC, 0xFFBB5897, 0xFFBCF152, 0xFFBE8A0D,
	0xFFC022C8, 0xFFC1BB83, 0xFFC3543E, 0xFFC4ECF9, 0xFFC685B4, 0xFFC81E6F, 0xFFC9B72A, 0xFFCB4FE5,
	0xFFCCE8A0, 0xFFCE815B, 0xFFD01A16, 0xFFD1B2D1, 0xFFD34B8C, 0xFFD4E447, 0xFFD67D02, 0xFFD815BD,
	0xFFD9AE78, 0xFFDB4733, 0xFFDCDFEE, 0xFFDE78A9, 0xFFE01164, 0xFFE1AA1F, 0xFFE342DA, 0xFFE4DB95,
	0xFFE67450, 0xFFE80D0B, 0xFFE9A5C6, 0xFFEB3E81, 0xFFECD73C, 0xFFEE6FF7, 0xFFF008B2, 0xFFF1A16D,
	0xFFF33A28, 0xFFF4D2E3, 0xFFF66B9E, 0xFFF80459, 0xFFF99D14, 0xFFFB35CF, 0xFFFCCE8A, 0xFFFE6745,
	0x00000000, 0x000198BB, 0x00033176, 0x0004CA31, 0x000662EC, 0x0007FBA7, 0x00099462, 0x000B2D1D,
	0x000CC5D8, 0x000E5E93, 0x000FF74E, 0x00119009, 0x001328C4, 0x0014C17F, 0x00165A3A, 0x0017F2F5,
	0x00198BB0, 0x001B246B, 0x001CBD26, 0x001E55E1, 0x001FEE9C, 0x00218757, 0x00232012, 0x0024B8CD,
	0x00265188, 0x0027EA43, 0x002982FE, 0x002B1BB9, 0x002CB474, 0x002E4D2F, 0x002FE5EA, 0x00317EA5,
	0x00331760, 0x0034B01B, 0x003648D6, 0x0037E191, 0x00397A4C, 0x003B1307, 0x003CABC2, 0x003E447D,
	0x003FDD38, 0x004175F3, 0x00430EAE, 0x0044A769, 0x00464024, 0x0047D8DF, 0x0049719A, 0x004B0A55,
	0x004CA310, 0x004E3BCB, 0x004FD486, 0x00516D41, 0x005305FC, 0x00549EB7, 0x00563772, 0x0057D02D,
	0x005968E8, 0x005B01A3, 0x005C9A5E, 0x005E3319, 0x005FCBD4, 0x0061648F, 0x0062FD4A, 0x00649605,
	0x00662EC0, 0x0067C77B, 0x00696036, 0x006AF8F1, 0x006C91AC, 0x006E2A67, 0x006FC322, 0x00715BDD,
	0x0072F498, 0x00748D53, 0x0076260E, 0x0077BEC9, 0x00795784, 0x007AF03F, 0x007C88FA, 0x007E21B5,
	0x007FBA70, 0x0081532B, 0x0082EBE6, 0x008484A1, 0x00861D5C, 0x0087B617, 0x00894ED2, 0x008AE78D,
	0x008C8048, 0x008E1903, 0x008FB1BE, 0x00914A79, 0x0092E334, 0x00947BEF, 0x009614AA, 0x0097AD65,
	0x00994620, 0x009ADEDB, 0x009C7796, 0x009E1051, 0x009FA90C, 0x00A141C7, 0x00A2DA82, 0x00A4733D,
	0x00A60BF8, 0x00A7A4B3, 0x00A93D6E, 0x00AAD629, 0x00AC6EE4, 0x00AE079F, 0x00AFA05A, 0x00B13915,
	0x00B2D1D0, 0x00B46A8B, 0x00B60346, 0x00B79C01, 0x00B934BC, 0x00BACD77, 0x00BC6632, 0x00BDFEED,
	0x00BF97A8, 0x00C13063, 0x00C2C91E, 0x00C461D9, 0x00C5FA94, 0x00C7934F, 0x00C92C0A, 0x00CAC4C5
};

#define CLAMP(x)	((x < 0) ? 0 : ((x > 255) ? 255 : x))
#define SWAP(x,y)	{ x ^= y; y ^= x; x ^= y; }
#define VIDEO_MEM	_MPEG_VIDEO_MEM_BASE
#define VIDEO_SIZE	_MPEG_VIDEO_MEM_SIZE
#define WIDTH_OFF	(VID_USER_MEM_BASE + 0x0100)
#define HEIGHT_OFF	(VID_USER_MEM_BASE + 0x0102)
#define GFXFB_MEM	_GFX_FB_MEM_BASE
#define GFXFB_SIZE	_GFX_FB_MEM_SIZE
/* TODO: aspect ratio correction and PIP */
bool cVideo::GetScreenImage(unsigned char * &video, int &xres, int &yres, bool get_video, bool get_osd, bool /*scale_to_video*/)
{
	hal_info("%s: get_video: %d get_osd: %d\n", __func__, get_video, get_osd);
	uint8_t *map;
	int mfd = open("/dev/mem", O_RDWR);
	if (mfd < 0) {
		hal_info("%s: cannot open open /dev/mem (%m)\n", __func__);
		return false;
	}
	/* this hints at incorrect usage */
	if (video != NULL)
		hal_info("%s: WARNING, video != NULL?\n", __func__);

	if (get_video)
	{
		map = (uint8_t *)mmap(NULL, VIDEO_SIZE, PROT_READ, MAP_SHARED, mfd, VIDEO_MEM);
		if (map == MAP_FAILED) {
			hal_info("%s: cannot mmap /dev/mem vor VIDEO (%m)\n", __func__);
			close(mfd);
			return false;
		}
		uint16_t w = *(uint16_t *)(map + WIDTH_OFF);
		uint16_t h = *(uint16_t *)(map + HEIGHT_OFF);
		if (w > 720 || h > 576) {
			hal_info("%s: unhandled resolution %dx%d, is the tuner locked?\n", __func__, w, h);
			munmap(map, VIDEO_SIZE);
			close(mfd);
			return false;
		}
		uint8_t *luma, *chroma;
		int needmem = w * h * 5 / 4; /* chroma is 1/4 in size of luma */
		int lumasize = w * h;
		int chromasize = needmem - lumasize;
		uint8_t *buf = (uint8_t *)malloc(needmem);
		if (!buf) {
			hal_info("%s: cannot allocate %d bytes (%m)\n", __func__, needmem);
			munmap(map, VIDEO_SIZE);
			close(mfd);
			return false;
		}
		/* luma is at the beginning of the buffer */
		memcpy(buf, map, lumasize);
		/* it looks like the chroma plane is always 720*576 bytes offset to the luma plane */
		memcpy(buf + lumasize, map + 720 * 576, chromasize);
		/* release the video buffer */
		munmap(map, VIDEO_SIZE);

		if (get_osd)
		{	/* in this case, the framebuffer determines the output resolution */
			xres = 720;
			yres = 576;
		}
		else
		{
			xres = w;
			yres = h;
		}
		video = (unsigned char *)malloc(xres * yres * 4);
		if (!video) {
			hal_info("%s: cannot allocate %d bytes for video buffer (%m)\n", __func__, yres * yres * 4);
			free(buf);
			close(mfd);
			return false;
		}

		luma = buf;
		chroma = buf + lumasize;

		int Y, U, V, y, x, out1, pos, RU, GU, GV, BV, rgbstride, i;

		// yuv2rgb conversion (4:2:0)
		hal_info("%s: converting Video from YUV to RGB color space\n", __func__);
		out1 = pos = 0;
		rgbstride = w * 4;

		for (y = h; y != 0; y -= 2)
		{
			for (x = w; x != 0; x -= 4)
			{
				U = *chroma++;
				V = *chroma++;
				RU = yuv2rgbtable_ru[U]; // use lookup tables to speedup the whole thing
				GU = yuv2rgbtable_gu[U];
				GV = yuv2rgbtable_gv[V];
				BV = yuv2rgbtable_bv[V];
				// now we do 8 pixels on each iteration this is more code but much faster
				for (i = 0; i < 4; i++)
				{
					Y = yuv2rgbtable_y[luma[pos]];
					video[out1    ] = CLAMP((Y + RU) >> 16);
					video[out1 + 1] = CLAMP((Y - GV - GU) >> 16);
					video[out1 + 2] = CLAMP((Y + BV) >> 16);
					video[out1 + 3] = 0xff;

					Y = yuv2rgbtable_y[luma[w + pos]];
					video[out1 +     rgbstride] = CLAMP((Y + RU) >> 16);
					video[out1 + 1 + rgbstride] = CLAMP((Y - GV - GU) >> 16);
					video[out1 + 2 + rgbstride] = CLAMP((Y + BV) >> 16);
					video[out1 + 3 + rgbstride] = 0xff;

					pos++;
					out1 += 4;
				}
			}
			out1 += rgbstride;
			pos += w;
		}
		if (get_osd && (w < xres || h < yres))
		{
			/* most trivial scaling algorithm:
			 * - no smoothing/antialiasing or similar
			 * - only upscaling
			 * more or less just memcpy
			 * works "backwards" (from bottom right up left),
			 * so that no extra buffer is needed */
			int j, k;
			uint32_t *v = (uint32_t *)video; /* int pointer to the video buffer */
			uint32_t *srcline;		 /* the start of the current line (unscaled) */
			uint32_t *dst = v + xres * yres; /* the current scaled pixel */
			for (j = yres -1 ; j >= 0; j--)
			{
				srcline = v + (((j * h) / yres) * w);
				for (k = xres - 1; k >= 0; k--) {
					dst--;
					*dst = *(srcline + ((k * w) / xres));
				}
			}
		}
		hal_info("%s: Video-Size: %d x %d\n", __func__, xres, yres);
		free(buf);
	}
	if (get_osd)
	{
		if (! get_video)
		{
			/* we don't use other framebuffer resolutions */
			xres = 720;
			yres = 576;
			video = (unsigned char *)calloc(xres * yres, 4);
			if (!video)
			{
				hal_info("%s: cannot allocate %d bytes for video buffer (%m)\n", __func__, yres * yres * 4);
				close(mfd);
				return false;
			}
		}
		/* we don't need the framebufferdevice, we know where the FB is located */
		map = (uint8_t *)mmap(NULL, GFXFB_SIZE, PROT_READ, MAP_SHARED, mfd, GFXFB_MEM);
		if (map == MAP_FAILED) {
			hal_info("%s: cannot mmap /dev/mem for GFXFB (%m)\n", __func__);
			close(mfd);
			return false;
		}
		unsigned int i, r, g, b, a, a2;
		uint8_t *p = map;
		uint8_t *q = video;
		for (i = xres * yres; i != 0; i--)
		{
			a = *p++;
			r = *p++;
			g = *p++;
			b = *p++;
			a2 = 0xff - a;
			/* blue */
			*q = ((*q * a2 ) + (b * a)) >> 8;
			q++;
			/* green */
			*q = ((*q * a2 ) + (g * a)) >> 8;
			q++;
			/* red */
			*q = ((*q * a2 ) + (r * a)) >> 8;
			q++;
			q++; /* skip alpha byte */
		}
		munmap(map, GFXFB_SIZE);
	}
	close(mfd);
	return true;
}

void cVideo::SetDemux(cDemux *)
{
	hal_debug("%s: not implemented yet\n", __func__);
}

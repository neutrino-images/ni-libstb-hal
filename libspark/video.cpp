/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 * (C) 2010-2011 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include <linux/dvb/video.h>
#include <linux/stmfb.h>
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

static bool hdmi_enabled = true;
static bool stillpicture = false;

#define VIDEO_STREAMTYPE_MPEG2 0
#define VIDEO_STREAMTYPE_MPEG4_H264 1
#define VIDEO_STREAMTYPE_VC1 3
#define VIDEO_STREAMTYPE_MPEG4_Part2 4
#define VIDEO_STREAMTYPE_VC1_SM 5
#define VIDEO_STREAMTYPE_MPEG1 6


static int proc_put(const char *path, const char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_WRONLY);
	if (pfd < 0)
		return pfd;
	ret = write(pfd, value, len);
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

static int proc_get(const char *path, char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_RDONLY);
	if (pfd < 0)
		return pfd;
	ret = read(pfd, value, len);
	value[len-1] = '\0'; /* make sure string is terminated */
	while (ret > 0 && isspace(value[ret-1]))
		value[--ret] = '\0';	/* remove trailing whitespace */
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

static unsigned int proc_get_hex(const char *path)
{
	unsigned int n, ret = 0;
	char buf[16];
	n = proc_get(path, buf, 16);
	if (n > 0)
		sscanf(buf, "%x", &ret);
	return ret;
}

static int hdmi_out(bool enable)
{
	struct stmfbio_output_configuration out;
	int ret = -1;
	lt_info_c("%s(%d)\n", __func__, enable);
	int fb = open("/dev/fb0", O_RDWR);
	if (fb < 0)
	{
		lt_debug_c("%s: can't open /dev/fb/0 (%m)\n", __func__);
		return -1;
	}
	out.outputid = STMFBIO_OUTPUTID_MAIN;
	if (ioctl(fb, STMFBIO_GET_OUTPUT_CONFIG, &out) < 0)
	{
		lt_debug_c("%s: STMFBIO_GET_OUTPUT_CONFIG (%m)\n", __func__);
		goto out;
	}
	hdmi_enabled = enable;
	out.caps = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
	out.activate = STMFBIO_ACTIVATE_IMMEDIATE;
	out.analogue_config = 0;
	if (enable)
		out.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_DISABLED;
	else
		out.hdmi_config |= STMFBIO_OUTPUT_HDMI_DISABLED;

	ret = ioctl(fb, STMFBIO_SET_OUTPUT_CONFIG, &out);
	if (ret < 0)
		_lt_debug(TRIPLE_DEBUG_VIDEO, NULL, "%s: STMFBIO_SET_OUTPUT_CONFIG (%m)\n", __func__);
out:
	close(fb);
	return ret;
}


cVideo::cVideo(int, void *, void *)
{
	lt_debug("%s\n", __FUNCTION__);

	//croppingMode = VID_DISPMODE_NORM;
	//outputformat = VID_OUTFMT_RGBC_SVIDEO;
	scartvoltage = -1;
	video_standby = 0;
	fd = -1;
	openDevice();
}

cVideo::~cVideo(void)
{
	closeDevice();
}

void cVideo::openDevice(void)
{
	int n = 0;
	lt_debug("%s\n", __func__);
	/* todo: this fd checking is racy, should be protected by a lock */
	if (fd != -1) /* already open */
		return;
retry:
	if ((fd = open(VIDEO_DEVICE, O_RDWR)) < 0)
	{
		if (errno == EBUSY)
		{
			/* sometimes we get busy quickly after close() */
			usleep(50000);
			if (++n < 10)
				goto retry;
		}
		lt_info("%s cannot open %s: %m, retries %d\n", __func__, VIDEO_DEVICE, n);
	}
	else
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	playstate = VIDEO_STOPPED;
}

void cVideo::closeDevice(void)
{
	lt_debug("%s\n", __func__);
	/* looks like sometimes close is unhappy about non-empty buffers */
	Start();
	if (fd >= 0)
		close(fd);
	fd = -1;
	playstate = VIDEO_STOPPED;
}

int cVideo::setAspectRatio(int aspect, int mode)
{
	static const char *a[] = { "n/a", "4:3", "14:9", "16:9" };
	static const char *m[] = { "panscan", "letterbox", "bestfit", "nonlinear", "(unset)" };
	int n;
	lt_debug("%s: a:%d m:%d  %s\n", __func__, aspect, mode, m[(mode < 0||mode > 3) ? 4 : mode]);

	if (aspect > 3 || aspect == 0)
		lt_info("%s: invalid aspect: %d\n", __func__, aspect);
	else if (aspect > 0) /* -1 == don't set */
	{
		lt_debug("%s: /proc/stb/video/aspect -> %s\n", __func__, a[aspect]);
		n = proc_put("/proc/stb/video/aspect", a[aspect], strlen(a[aspect]));
		if (n < 0)
			lt_info("%s: proc_put /proc/stb/video/aspect (%m)\n", __func__);
	}

	if (mode == -1)
		return 0;

	lt_debug("%s: /proc/stb/video/policy -> %s\n", __func__, m[mode]);
	n = proc_put("/proc/stb/video/policy", m[mode], strlen(m[mode]));
	if (n < 0)
		return 1;
	return 0;
}

int cVideo::getAspectRatio(void)
{
	video_size_t s;
	if (fd == -1)
	{
		/* in movieplayer mode, fd is not opened -> fall back to procfs */
		int n = proc_get_hex("/proc/stb/vmpeg/0/aspect");
		return n * 2 + 1;
	}
	if (fop(ioctl, VIDEO_GET_SIZE, &s) < 0)
	{
		lt_info("%s: VIDEO_GET_SIZE %m\n", __func__);
		return -1;
	}
	lt_debug("%s: %d\n", __func__, s.aspect_ratio);
	return s.aspect_ratio * 2 + 1;
}

int cVideo::setCroppingMode(int /*vidDispMode_t format*/)
{
	return 0;
#if 0
	croppingMode = format;
	const char *format_string[] = { "norm", "letterbox", "unknown", "mode_1_2", "mode_1_4", "mode_2x", "scale", "disexp" };
	const char *f;
	if (format >= VID_DISPMODE_NORM && format <= VID_DISPMODE_DISEXP)
		f = format_string[format];
	else
		f = "ILLEGAL format!";
	lt_debug("%s(%d) => %s\n", __FUNCTION__, format, f);
	return fop(ioctl, MPEG_VID_SET_DISPMODE, format);
#endif
}

int cVideo::Start(void * /*PcrChannel*/, unsigned short /*PcrPid*/, unsigned short /*VideoPid*/, void * /*hChannel*/)
{
	lt_debug("%s playstate=%d\n", __FUNCTION__, playstate);
#if 0
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
#endif
	playstate = VIDEO_PLAYING;
	fop(ioctl, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	return fop(ioctl, VIDEO_PLAY);
}

int cVideo::Stop(bool blank)
{
	lt_debug("%s(%d)\n", __FUNCTION__, blank);
	if (stillpicture)
	{
		lt_debug("%s: stillpicture == true\n", __func__);
		return -1;
	}
	playstate = blank ? VIDEO_STOPPED : VIDEO_FREEZED;
	return fop(ioctl, VIDEO_STOP, blank ? 1 : 0);
}

int cVideo::setBlank(int)
{
	return Stop(1);
}

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	lt_debug("%s(%d, %d)\n", __func__, video_system, remember);
	char current[32];
	static const char *modes[] = {
		"pal",		// VIDEO_STD_NTSC
		"pal",		// VIDEO_STD_SECAM
		"pal",		// VIDEO_STD_PAL
		"480p",		// VIDEO_STD_480P
		"576p50",	// VIDEO_STD_576P
		"720p60",	// VIDEO_STD_720P60
		"1080i60",	// VIDEO_STD_1080I60
		"720p50",	// VIDEO_STD_720P50
		"1080i50",	// VIDEO_STD_1080I50
		"1080p30",	// VIDEO_STD_1080P30
		"1080p24",	// VIDEO_STD_1080P24
		"1080p25",	// VIDEO_STD_1080P25
		"720p50",	// VIDEO_STD_AUTO -> not implemented
		"1080p50"	// VIDEO_STD_1080P50 -> SPARK only
	};

	if (video_system > VIDEO_STD_MAX)
	{
		lt_info("%s: video_system (%d) > VIDEO_STD_MAX (%d)\n", __func__, video_system, VIDEO_STD_MAX);
		return -1;
	}
	int ret = proc_get("/proc/stb/video/videomode", current, 32);
	if (strcmp(current,  modes[video_system]) == 0)
	{
		lt_info("%s: video_system %d (%s) already set, skipping\n", __func__, video_system, current);
		return 0;
	}
	lt_info("%s: old: '%s' new: '%s'\n", __func__, current, modes[video_system]);
	bool stopped = false;
	if (playstate == VIDEO_PLAYING)
	{
		lt_info("%s: playstate == VIDEO_PLAYING, stopping video\n", __func__);
		Stop();
		stopped = true;
	}
	hdmi_out(false);
	ret = proc_put("/proc/stb/video/videomode", modes[video_system],strlen(modes[video_system]));
	hdmi_out(true);
	if (stopped)
		Start();

	return ret;
}

int cVideo::getPlayState(void)
{
	return playstate;
}

void cVideo::SetVideoMode(analog_mode_t mode)
{
	lt_debug("%s(%d)\n", __func__, mode);
	if (!(mode & ANALOG_SCART_MASK))
	{
		lt_debug("%s: non-SCART mode ignored\n", __func__);
		return;
	}
	const char *m;
	switch(mode)
	{
		case ANALOG_SD_YPRPB_SCART:
			m = "yuv";
			break;
		case ANALOG_SD_RGB_SCART:
			m = "rgb";
			break;
		default:
			lt_info("%s unknown mode %d\n", __func__, mode);
			m = "rgb";
			break; /* default to rgb */
	}
	proc_put("/proc/stb/avs/0/colorformat", m, strlen(m));
}

void cVideo::ShowPicture(const char * fname)
{
	lt_debug("%s(%s)\n", __func__, fname);
	static const unsigned char pes_header[] = { 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0x00, 0x00 };
	static const unsigned char seq_end[] = { 0x00, 0x00, 0x01, 0xB7 };
	char destname[512];
	char cmd[512];
	char *p;
	int mfd;
	struct stat st, st2;
	if (video_standby)
	{
		/* does not work and the driver does not seem to like it */
		lt_info("%s: video_standby == true\n", __func__);
		return;
	}
	strcpy(destname, "/var/cache");
	if (stat(fname, &st2))
	{
		lt_info("%s: could not stat %s (%m)\n", __func__, fname);
		return;
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
		sprintf(cmd, "ffmpeg -y -f mjpeg -i '%s' -s 1280x720 '%s' </dev/null",
							fname, destname);
		system(cmd); /* TODO: use libavcodec to directly convert it */
		utime(destname, &u);
	}
	mfd = open(destname, O_RDONLY);
	if (mfd < 0)
	{
		lt_info("%s cannot open %s: %m", __func__, destname);
		goto out;
	}
	fstat(mfd, &st);

	closeDevice();
	openDevice();

	if (fd >= 0)
	{
		stillpicture = true;

		if (ioctl(fd, VIDEO_SET_FORMAT, VIDEO_FORMAT_16_9) < 0)
			lt_info("%s: VIDEO_SET_FORMAT failed (%m)\n", __func__);
		bool seq_end_avail = false;
		size_t pos=0;
		unsigned char *iframe = (unsigned char *)malloc((st.st_size < 8192) ? 8192 : st.st_size);
		if (! iframe)
		{
			lt_info("%s: malloc failed (%m)\n", __func__);
			goto out;
		}
		read(mfd, iframe, st.st_size);
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY);
		ioctl(fd, VIDEO_PLAY);
		ioctl(fd, VIDEO_CONTINUE);
		ioctl(fd, VIDEO_CLEAR_BUFFER);
		while (pos <= (st.st_size-4) && !(seq_end_avail = (!iframe[pos] && !iframe[pos+1] && iframe[pos+2] == 1 && iframe[pos+3] == 0xB7)))
			++pos;

		if ((iframe[3] >> 4) != 0xE) // no pes header
			write(fd, pes_header, sizeof(pes_header));
		write(fd, iframe, st.st_size);
		if (!seq_end_avail)
			write(fd, seq_end, sizeof(seq_end));
		memset(iframe, 0, 8192);
		write(fd, iframe, 8192);
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
		free(iframe);
	}
 out:
	close(mfd);
	return;
}

void cVideo::StopPicture()
{
	lt_debug("%s\n", __func__);
	stillpicture = false;
}

void cVideo::Standby(unsigned int bOn)
{
	lt_debug("%s(%d)\n", __func__, bOn);
	if (bOn)
	{
		closeDevice();
		hdmi_out(false);
	}
	else
	{
		/* only enable HDMI output when coming from standby, not on
		 * start. I have no idea why, but enabling it on startup leads
		 * to strange locking problems of the framebuffer driver :-( */
		if (!hdmi_enabled)
		{
			hdmi_out(true);
			/* make sure the driver has time to settle.
			 * again - lame, but makes it work... */
			sleep(1);
		}
		openDevice();
	}
	video_standby = bOn;
}

int cVideo::getBlank(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return 0;
}

/* this function is regularly called, checks if video parameters
   changed and triggers appropriate actions */
void cVideo::VideoParamWatchdog(void)
{
#if 0
	static unsigned int _v_info = (unsigned int) -1;
	unsigned int v_info;
	if (fd == -1)
		return;
	ioctl(fd, MPEG_VID_GET_V_INFO_RAW, &v_info);
	if (_v_info != v_info)
	{
		lt_debug("%s params changed. old: %08x new: %08x\n", __FUNCTION__, _v_info, v_info);
		setAspectRatio(-1, -1);
	}
	_v_info = v_info;
#endif
}

void cVideo::Pig(int x, int y, int w, int h, int osd_w, int osd_h)
{
	char buffer[64];
	int _x, _y, _w, _h;
	/* the target "coordinates" seem to be in a PAL sized plane
	 * TODO: check this in the driver sources */
	int xres = 720; /* proc_get_hex("/proc/stb/vmpeg/0/xres") */
	int yres = 576; /* proc_get_hex("/proc/stb/vmpeg/0/yres") */
	lt_debug("%s: x:%d y:%d w:%d h:%d ow:%d oh:%d\n", __func__, x, y, w, h, osd_w, osd_h);
	if (x == -1 && y == -1 && w == -1 && h == -1)
	{
		_w = xres;
		_h = yres;
		_x = 0;
		_y = 0;
	}
	else
	{
		_x = x * xres / osd_w;
		_w = w * xres / osd_w;
		_y = y * yres / osd_h;
		_h = h * yres / osd_h;
	}
	lt_debug("%s: x:%d y:%d w:%d h:%d xr:%d yr:%d\n", __func__, _x, _y, _w, _h, xres, yres);
	sprintf(buffer, "%x %x %x %x", _x, _y, _w, _h);
	proc_put("/proc/stb/vmpeg/0/dst_all", buffer, strlen(buffer));
}

static inline int rate2csapi(int rate)
{
	switch (rate)
	{
		case 23976:
			return 0;
		case 24000:
			return 1;
		case 25000:
			return 2;
		case 29976:
			return 3;
		case 30000:
			return 4;
		case 50000:
			return 5;
		case 50940:
			return 6;
		case 60000:
			return 7;
		default:
			break;
	}
	return -1;
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	video_size_t s;
	int r;
	if (fd == -1)
	{
		/* in movieplayer mode, fd is not opened -> fall back to procfs */
		r      = proc_get_hex("/proc/stb/vmpeg/0/framerate");
		width  = proc_get_hex("/proc/stb/vmpeg/0/xres");
		height = proc_get_hex("/proc/stb/vmpeg/0/yres");
		rate   = rate2csapi(r);
		return;
	}
	ioctl(fd, VIDEO_GET_SIZE, &s);
	ioctl(fd, VIDEO_GET_FRAME_RATE, &r);
	rate = rate2csapi(r);
	height = s.h;
	width = s.w;
	lt_debug("%s: rate: %d, width: %d height: %d\n", __func__, rate, width, height);
}

void cVideo::SetSyncMode(AVSYNC_TYPE mode)
{
	lt_debug("%s %d\n", __func__, mode);
	/*
	 * { 0, LOCALE_OPTIONS_OFF },
	 * { 1, LOCALE_OPTIONS_ON  },
	 * { 2, LOCALE_AUDIOMENU_AVSYNC_AM }
	 */
	const char *apply[] = { "disapply", "apply" };
	const char *clock[] = { "video", "audio" };
	const char *a = apply[mode > 0]; /* mode == 1 or mode == 2 -> "apply" */
	const char *c = clock[mode > 1];  /* mode == 2 -> "audio" */
	proc_put("/proc/stb/stream/policy/AV_SYNC", a, strlen(a));
	proc_put("/proc/stb/stream/policy/MASTER_CLOCK", c, strlen(c));
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
	int t;
	lt_debug("%s type=%s\n", __FUNCTION__, VF[type]);

	switch (type)
	{
		case VIDEO_FORMAT_MPEG4:
			t = VIDEO_STREAMTYPE_MPEG4_H264;
			break;
		case VIDEO_FORMAT_VC1:
			t = VIDEO_STREAMTYPE_VC1;
			break;
		case VIDEO_FORMAT_MPEG2:
		default:
			t = VIDEO_STREAMTYPE_MPEG2;
			break;
	}

	if (ioctl(fd, VIDEO_SET_STREAMTYPE, t) < 0)
		lt_info("%s VIDEO_SET_STREAMTYPE(%d) failed: %m\n", __func__, t);
	return 0;
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	if (ioctl(fd, VIDEO_GET_PTS, &pts) < 0)
		lt_info("%s: GET_PTS failed (%m)\n", __func__);
	return pts;
}

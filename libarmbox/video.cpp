/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 * (C) 2010-2013, 2015 Stefan Seyfried
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

#include <pthread.h>

#include <linux/dvb/video.h>
#include <linux/fb.h>
#include "video_lib.h"
#include "lt_debug.h"
#include "linux-uapi-cec.h"

#include <proc_tools.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

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
cVideo * pipDecoder = NULL;

int system_rev = 0;

static bool stillpicture = false;

static const char *VDEV[] = {
	"/dev/dvb/adapter0/video0",
	"/dev/dvb/adapter0/video1"
};
static const char *VMPEG_aspect[] = {
	"/proc/stb/vmpeg/0/aspect",
	"/proc/stb/vmpeg/1/aspect"
};

static const char *VMPEG_xres[] = {
	"/proc/stb/vmpeg/0/xres",
	"/proc/stb/vmpeg/1/xres"
};

static const char *VMPEG_yres[] = {
	"/proc/stb/vmpeg/0/yres",
	"/proc/stb/vmpeg/1/yres"
};

static const char *VMPEG_dst_height[] = {
	"/proc/stb/vmpeg/0/dst_height",
	"/proc/stb/vmpeg/1/dst_height"
};

static const char *VMPEG_dst_width[] = {
	"/proc/stb/vmpeg/0/dst_width",
	"/proc/stb/vmpeg/1/dst_width"
};

static const char *VMPEG_dst_top[] = {
	"/proc/stb/vmpeg/0/dst_top",
	"/proc/stb/vmpeg/1/dst_top"
};

static const char *VMPEG_dst_left[] = {
	"/proc/stb/vmpeg/0/dst_left",
	"/proc/stb/vmpeg/1/dst_left"
};

static const char *VMPEG_framerate[] = {
	"/proc/stb/vmpeg/0/framerate",
	"/proc/stb/vmpeg/1/framerate"
};

static const char *vid_modes[] = {
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
	"1080p50",	// VIDEO_STD_1080P50
	"1080p60",	// VIDEO_STD_1080P60
	"1080p2397",	// VIDEO_STD_1080P2397
	"1080p2997",	// VIDEO_STD_1080P2997
	"2160p24",	// VIDEO_STD_2160P24
	"2160p25",	// VIDEO_STD_2160P25
	"2160p30",	// VIDEO_STD_2160P30
	"2160p50",	// VIDEO_STD_2160P50
	"720p50"	// VIDEO_STD_AUTO
};

#define VIDEO_STREAMTYPE_MPEG2 0
#define VIDEO_STREAMTYPE_MPEG4_H264 1
#define VIDEO_STREAMTYPE_VC1 3
#define VIDEO_STREAMTYPE_MPEG4_Part2 4
#define VIDEO_STREAMTYPE_VC1_SM 5
#define VIDEO_STREAMTYPE_MPEG1 6
#define VIDEO_STREAMTYPE_H265_HEVC 7
#define VIDEO_STREAMTYPE_AVS 16

void init_parameters(AVFrame* in_frame, AVCodecContext *codec_context)
{
	/* put sample parameters */
	codec_context->bit_rate = 400000;
	/* resolution must be a multiple of two */
	codec_context->width = (in_frame->width/2)*2;
	codec_context->height = (in_frame->height/2)*2;
	/* frames per second */
	codec_context->time_base = (AVRational ) { 1, 60 };
	codec_context->gop_size = 10; /* emit one intra frame every ten frames */
	codec_context->max_b_frames = 1;
	codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
}

void write_frame(AVFrame* in_frame, FILE* fp)
{
	if(in_frame == NULL || fp == NULL)
		return;
	AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
	if (codec)
	{
		AVCodecContext *codec_context = avcodec_alloc_context3(codec);
		if (codec_context)
		{
			init_parameters(in_frame, codec_context);
			if (avcodec_open2(codec_context, codec, 0) != -1){
				AVPacket pkt;
				av_init_packet(&pkt);
				/* encode the image */
				int got_output = 0;
				int ret = avcodec_encode_video2(codec_context, &pkt, in_frame, &got_output);
				if (ret != -1){
					if (got_output){
						fwrite(pkt.data, 1, pkt.size, fp);
						av_packet_unref(&pkt);
					}
					int i =1;
					for (got_output = 1; got_output; i++){
					/* get the delayed frames */
						in_frame->pts = i;
						ret = avcodec_encode_video2(codec_context, &pkt, 0, &got_output);
						if (ret != -1 && got_output){
							fwrite(pkt.data, 1, pkt.size, fp);
							av_packet_unref(&pkt);
						}
					}
					avcodec_close(codec_context);
					av_free(codec_context);
				}
			}
		}
	}
}

int decode_frame(AVCodecContext *codecContext,AVPacket &packet, FILE* fp)
{
	int decode_ok = 0;
	AVFrame *frame = av_frame_alloc();
	if(frame){
		if ((avcodec_decode_video2(codecContext, frame, &decode_ok, &packet)) < 0 || !decode_ok){
			av_frame_free(&frame);
			return -1;
		}
		write_frame(frame, fp);
		av_frame_free(&frame);
	}
	return 0;

}

AVCodecContext* open_codec(AVMediaType mediaType, AVFormatContext* formatContext)
{
	int stream_index = av_find_best_stream(formatContext, mediaType, -1, -1, NULL, 0);
	if (stream_index < 0){
		return NULL;
	}
	AVCodecContext * codecContext = formatContext->streams[stream_index]->codec;
	AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
	if (codec && (avcodec_open2(codecContext, codec, NULL)) != 0){
		return NULL;
	}
	return codecContext;
}

int image_to_mpeg2(const char *image_name, const char *encode_name)
{
	int ret = 0;
	av_register_all();
	avcodec_register_all();

	AVFormatContext *formatContext = avformat_alloc_context();
	if (formatContext && (ret = avformat_open_input(&formatContext, image_name, NULL, NULL)) == 0){
		AVCodecContext *codecContext = open_codec(AVMEDIA_TYPE_VIDEO, formatContext);
		if(codecContext){
			AVPacket packet;
			av_init_packet(&packet);
			if ((ret = av_read_frame(formatContext, &packet)) !=-1){
				FILE* fp = fopen(encode_name, "wb");
				if(fp){
					if(decode_frame(codecContext, packet, fp) != 1){
						/* add sequence end code to have a real mpeg file */
						uint8_t endcode[] = { 0, 0, 1, 0xb7 };
						fwrite(endcode, 1, sizeof(endcode), fp);
					}
					fclose(fp);
				}
				avcodec_close(codecContext);
				av_free_packet(&packet);
			}
			avformat_close_input(&formatContext);
		}
	}
	av_free(formatContext);
	return 0;
}

cVideo::cVideo(int, void *, void *, unsigned int unit)
{
	lt_debug("%s unit %u\n", __func__, unit);

	brightness = -1;
	contrast = -1;
	saturation = -1;
	hue = -1;
	video_standby = 0;
	if (unit > 1) {
		lt_info("%s: unit %d out of range, setting to 0\n", __func__, unit);
		devnum = 0;
	} else
		devnum = unit;
	fd = -1;
	hdmiFd = -1;
	standby_cec_activ = autoview_cec_activ = false;
	openDevice();
}

cVideo::~cVideo(void)
{
	closeDevice();
}

void cVideo::openDevice(void)
{
	int n = 0;
	lt_debug("#%d: %s\n", devnum, __func__);
	/* todo: this fd checking is racy, should be protected by a lock */
	if (fd != -1) /* already open */
		return;
retry:
	if ((fd = open(VDEV[devnum], O_RDWR|O_CLOEXEC)) < 0)
	{
		if (errno == EBUSY)
		{
			/* sometimes we get busy quickly after close() */
			usleep(50000);
			if (++n < 10)
				goto retry;
		}
		lt_info("#%d: %s cannot open %s: %m, retries %d\n", devnum, __func__, VDEV[devnum], n);
	}
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
		int n = proc_get_hex(VMPEG_aspect[devnum]);
		return n * 2 + 1;
	}
	if (fop(ioctl, VIDEO_GET_SIZE, &s) < 0)
	{
		lt_info("%s: VIDEO_GET_SIZE %m\n", __func__);
		return -1;
	}
	lt_debug("#%d: %s: %d\n", devnum, __func__, s.aspect_ratio);
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
	lt_debug("#%d: %s playstate=%d\n", devnum, __func__, playstate);
#if 0
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
#endif
	/* implicitly do StopPicture() on video->Start() */
	if (stillpicture) {
		lt_info("%s: stillpicture == true, doing implicit StopPicture()\n", __func__);
		stillpicture = false;
		Stop(1);
	}
	playstate = VIDEO_PLAYING;
	fop(ioctl, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	int res = fop(ioctl, VIDEO_PLAY);
	if (brightness > -1) {
		SetControl(VIDEO_CONTROL_BRIGHTNESS, brightness);
		brightness = -1;
	}
	if (contrast > -1) {
		SetControl(VIDEO_CONTROL_CONTRAST, contrast);
		contrast = -1;
	}
	if (saturation > -1) {
		SetControl(VIDEO_CONTROL_SATURATION, saturation);
		saturation = -1;
	}
	if (hue > -1) {
		SetControl(VIDEO_CONTROL_HUE, hue);
		hue = -1;
	}
	return res;
}

int cVideo::Stop(bool blank)
{
	lt_debug("#%d: %s(%d)\n", devnum, __func__, blank);
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
	fop(ioctl, VIDEO_PLAY);
	fop(ioctl, VIDEO_CONTINUE);
	video_still_picture sp = { NULL, 0 };
	fop(ioctl, VIDEO_STILLPICTURE, &sp);
	return Stop(1);
}

int cVideo::GetVideoSystem(void)
{
	char current[32];
	proc_get("/proc/stb/video/videomode", current, 32);
	for (int i = 0; vid_modes[i]; i++)
	{
		if (strcmp(current, vid_modes[i]) == 0)
			return i;
	}
	lt_info("%s: could not find '%s' mode, returning VIDEO_STD_720P50\n", __func__, current);
	return VIDEO_STD_720P50;
}

void cVideo::GetVideoSystemFormatName(cs_vs_format_t *format, int system)
{
	if (system == -1)
		system = GetVideoSystem();
	if (system < 0 || system > VIDEO_STD_1080P50) {
		lt_info("%s: invalid system %d\n", __func__, system);
		strcpy(format->format, "invalid");
	} else
		strcpy(format->format, vid_modes[system]);
}

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	lt_debug("%s(%d, %d)\n", __func__, video_system, remember);
	char current[32];

	if (video_system > VIDEO_STD_MAX)
	{
		lt_info("%s: video_system (%d) > VIDEO_STD_MAX (%d)\n", __func__, video_system, VIDEO_STD_MAX);
		return -1;
	}
	int ret = proc_get("/proc/stb/video/videomode", current, 32);
	if (strcmp(current, vid_modes[video_system]) == 0)
	{
		lt_info("%s: video_system %d (%s) already set, skipping\n", __func__, video_system, current);
		return 0;
	}
	lt_info("%s: old: '%s' new: '%s'\n", __func__, current, vid_modes[video_system]);
	bool stopped = false;
	if (playstate == VIDEO_PLAYING)
	{
		lt_info("%s: playstate == VIDEO_PLAYING, stopping video\n", __func__);
		Stop();
		stopped = true;
	}
	ret = proc_put("/proc/stb/video/videomode", vid_modes[video_system],strlen(vid_modes[video_system]));
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
	lt_debug("#%d: %s(%d)\n", devnum, __func__, mode);
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

ssize_t write_all(int fd, const void *buf, size_t count)
{
	int retval;
	char *ptr = (char*)buf;
	size_t handledcount = 0;
	while (handledcount < count)
	{
		retval = write(fd, &ptr[handledcount], count - handledcount);
		if (retval == 0)
			return -1;
		if (retval < 0)
		{
			if (errno == EINTR)
				continue;
			return retval;
		}
		handledcount += retval;
	}
	return handledcount;
}

void cVideo::ShowPicture(const char * fname, const char *_destname)
{
	lt_debug("%s(%s)\n", __func__, fname);
	//static const unsigned char pes_header[] = { 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0x00, 0x00 };
	static const unsigned char pes_header[] = {0x0, 0x0, 0x1, 0xe0, 0x00, 0x00, 0x80, 0x80, 0x5, 0x21, 0x0, 0x1, 0x0, 0x1};
	static const unsigned char seq_end[] = { 0x00, 0x00, 0x01, 0xB7 };
	char destname[512];
	char *p;
	int mfd;
	struct stat st, st2;
	if (video_standby)
	{
		/* does not work and the driver does not seem to like it */
		lt_info("%s: video_standby == true\n", __func__);
		return;
	}
	const char *lastDot = strrchr(fname, '.');
	if (lastDot && !strcasecmp(lastDot + 1, "m2v"))
		strncpy(destname, fname, sizeof(destname));
	else {
		if (_destname)
			strncpy(destname, _destname, sizeof(destname));
		else {
			strcpy(destname, "/tmp/cache");
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
			p = &destname[strlen("/tmp/cache/")];
			while ((p = strchr(p, '/')) != NULL)
				*p = '.';
			strcat(destname, ".m2v");
		}
		/* ...then check if it exists already... */
		if (stat(destname, &st) || (st.st_mtime != st2.st_mtime) || (st.st_size == 0))
		{
			struct utimbuf u;
			u.actime = time(NULL);
			u.modtime = st2.st_mtime;
			/* it does not exist or has a different date, so call ffmpeg... */
			image_to_mpeg2(fname, destname);
			utime(destname, &u);
		}
	}
	mfd = open(destname, O_RDONLY);
	if (mfd < 0)
	{
		lt_info("%s cannot open %s: %m\n", __func__, destname);
		goto out;
	}
	fstat(mfd, &st);

	closeDevice();
	openDevice();

	if (fd >= 0)
	{
		stillpicture = true;

		bool seq_end_avail = false;
		off_t pos=0;
		unsigned char iframe[st.st_size];
		if (! iframe)
		{
			lt_info("%s: malloc failed (%m)\n", __func__);
			goto out;
		}
		read(mfd, iframe, st.st_size);
		if(iframe[0] == 0x00 && iframe[1] == 0x00 && iframe[2] == 0x00 && iframe[3] == 0x01 && (iframe[4] & 0x0f) == 0x07)
			ioctl(fd, VIDEO_SET_STREAMTYPE, 1); // set to mpeg4
		else
			ioctl(fd, VIDEO_SET_STREAMTYPE, 0); // set to mpeg2
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY);
		ioctl(fd, VIDEO_PLAY);
		ioctl(fd, VIDEO_CONTINUE);
		ioctl(fd, VIDEO_CLEAR_BUFFER);
		while (pos <= (st.st_size-4) && !(seq_end_avail = (!iframe[pos] && !iframe[pos+1] && iframe[pos+2] == 1 && iframe[pos+3] == 0xB7)))
			++pos;

		if ((iframe[3] >> 4) != 0xE) // no pes header
			write_all(fd, pes_header, sizeof(pes_header));
		else
			iframe[4] = iframe[5] = 0x00;
		write_all(fd, iframe, st.st_size);
		if (!seq_end_avail)
			write(fd, seq_end, sizeof(seq_end));
		memset(iframe, 0, 8192);
		write_all(fd, iframe, 8192);
		usleep(150000);
		ioctl(fd, VIDEO_STOP, 0);
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	}
 out:
	close(mfd);
	return;
}

void cVideo::StopPicture()
{
	lt_debug("%s\n", __func__);
	stillpicture = false;
	Stop(1);
}

void cVideo::Standby(unsigned int bOn)
{
	lt_debug("%s(%d)\n", __func__, bOn);
	if (bOn)
	{
		closeDevice();
	}
	else
	{
		openDevice();
	}
	video_standby = bOn;
	SetCECState(video_standby);
}

int cVideo::getBlank(void)
{
	int ret = proc_get_hex(VMPEG_xres[devnum]);
	lt_debug("%s => %d\n", __func__, !ret);
	return !ret;
}

void cVideo::Pig(int x, int y, int w, int h, int osd_w, int osd_h, int startx, int starty, int endx, int endy)
{
	char buffer[64];
	int _x, _y, _w, _h;
	/* the target "coordinates" seem to be in a PAL sized plane
	 * TODO: check this in the driver sources */
	int xres = 720; /* proc_get_hex("/proc/stb/vmpeg/0/xres") */
	int yres = 576; /* proc_get_hex("/proc/stb/vmpeg/0/yres") */
	lt_debug("#%d %s: x:%d y:%d w:%d h:%d ow:%d oh:%d\n", devnum, __func__, x, y, w, h, osd_w, osd_h);
	if (x == -1 && y == -1 && w == -1 && h == -1)
	{
		_w = xres;
		_h = yres;
		_x = 0;
		_y = 0;
	}
	else
	{
		// need to do some additional adjustments because osd border is handled by blitter
		x += startx;
		x *= endx - startx + 1;
		y += starty;
		y *= endy - starty + 1;
		w *= endx - startx + 1;
		h *= endy - starty + 1;
		_x = x * xres / osd_w;
		_w = w * xres / osd_w;
		_y = y * yres / osd_h;
		_h = h * yres / osd_h;
		_x /= 1280;
		_y /= 720;
		_w /= 1280;
		_h /= 720;
	}
	lt_debug("#%d %s: x:%d y:%d w:%d h:%d xr:%d yr:%d\n", devnum, __func__, _x, _y, _w, _h, xres, yres);
	sprintf(buffer, "%x", _x);
	proc_put(VMPEG_dst_left[devnum], buffer, strlen(buffer));

	sprintf(buffer, "%x", _y);
	proc_put(VMPEG_dst_top[devnum], buffer, strlen(buffer));

	sprintf(buffer, "%x", _w);
	proc_put(VMPEG_dst_width[devnum], buffer, strlen(buffer));

	sprintf(buffer, "%x", _h);
	proc_put(VMPEG_dst_height[devnum], buffer, strlen(buffer));
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
		r      = proc_get_hex(VMPEG_framerate[devnum]);
		width  = proc_get_hex(VMPEG_xres[devnum]);
		height = proc_get_hex(VMPEG_yres[devnum]);
		rate   = rate2csapi(r);
		return;
	}
	ioctl(fd, VIDEO_GET_SIZE, &s);
	ioctl(fd, VIDEO_GET_FRAME_RATE, &r);
	rate = rate2csapi(r);
	height = s.h;
	width = s.w;
	lt_debug("#%d: %s: rate: %d, width: %d height: %d\n", devnum, __func__, rate, width, height);
}

void cVideo::SetSyncMode(AVSYNC_TYPE mode)
{
	lt_debug("%s %d\n", __func__, mode);
	/*
	 * { 0, LOCALE_OPTIONS_OFF },
	 * { 1, LOCALE_OPTIONS_ON  },
	 * { 2, LOCALE_AUDIOMENU_AVSYNC_AM }
	 */
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
	lt_debug("#%d: %s type=%s\n", devnum, __func__, VF[type]);

	switch (type)
	{
		case VIDEO_FORMAT_MPEG4_H264:
			t = VIDEO_STREAMTYPE_MPEG4_H264;
			break;
		case VIDEO_FORMAT_MPEG4_H265:
			t = VIDEO_STREAMTYPE_H265_HEVC;
			break;
		case VIDEO_FORMAT_AVS:
			t = VIDEO_STREAMTYPE_AVS;
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

void cVideo::SetDemux(cDemux *)
{
	lt_debug("#%d %s not implemented yet\n", devnum, __func__);
}

void cVideo::SetControl(int control, int value) {
	const char *p = NULL;
	switch (control) {
	case VIDEO_CONTROL_BRIGHTNESS:
		brightness = value;
		p = "/proc/stb/vmpeg/0/pep_brightness";
		break;
	case VIDEO_CONTROL_CONTRAST:
		contrast = value;
		p = "/proc/stb/vmpeg/0/pep_contrast";
		break;
	case VIDEO_CONTROL_SATURATION:
		saturation = value;
		p = "/proc/stb/vmpeg/0/pep_saturation";
		break;
	case VIDEO_CONTROL_HUE:
		hue = value;
		p = "/proc/stb/vmpeg/0/pep_hue";
		break;
	}
	if (p) {
		char buf[20];
		int len = snprintf(buf, sizeof(buf), "%d", value);
		if (len < (int) sizeof(buf))
			proc_put(p, buf, len);
	}
}

void cVideo::SetColorFormat(COLOR_FORMAT color_format) {
	const char *p = NULL;
	switch(color_format) {
	case COLORFORMAT_RGB:
		p = "rgb";
		break;
	case COLORFORMAT_YUV:
		p = "yuv";
		break;
	case COLORFORMAT_CVBS:
		p = "cvbs";
		break;
	case COLORFORMAT_SVIDEO:
		p = "svideo";
		break;
	case COLORFORMAT_HDMI_RGB:
		p = "hdmi_rgb";
		break;
	case COLORFORMAT_HDMI_YCBCR444:
		p = "hdmi_yuv";
		break;
	case COLORFORMAT_HDMI_YCBCR422:
		p = "hdmi_422";
		break;
	}
	if (p)
		proc_put("/proc/stb/video/hdmi_colorspace", p, strlen(p));
}

/* TODO: aspect ratio correction and PIP */
bool cVideo::GetScreenImage(unsigned char * &video, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	lt_info("%s: video 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, video, xres, yres, get_video, get_osd, scale_to_video);

	return true;
}

bool cVideo::SetCECMode(VIDEO_HDMI_CEC_MODE _deviceType)
{
	physicalAddress[0] = 0x10;
	physicalAddress[1] = 0x00;
	logicalAddress = 1;
	
	if (_deviceType == VIDEO_HDMI_CEC_MODE_OFF)
	{
		if (hdmiFd >= 0) {
			close(hdmiFd);
			hdmiFd = -1;
		}
		return false;
	}
	else
		deviceType = _deviceType;

	if (hdmiFd == -1)
		hdmiFd = open("/dev/cec0", O_RDWR | O_CLOEXEC);

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

	}

	GetCECAddressInfo();

	return true;
}

void cVideo::GetCECAddressInfo()
{
	if (hdmiFd >= 0)
	{
		bool hasdata = false;
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
			lt_info("%s: detected physical address change: %02X%02X --> %02X%02X", __func__, physicalAddress[0], physicalAddress[1], addressinfo.physical[0], addressinfo.physical[1]);
			memcpy(physicalAddress, addressinfo.physical, sizeof(physicalAddress));
			ReportPhysicalAddress();
			// addressChanged((physicalAddress[0] << 8) | physicalAddress[1]);
		}
	}
}

void cVideo::ReportPhysicalAddress()
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

void cVideo::SendCECMessage(struct cec_message &message)
{
	if (hdmiFd >= 0)
	{
		lt_info("[CEC] send message");
		for (int i = 0; i < message.length; i++)
		{
			lt_info(" %02X", message.data[i]);
		}
		lt_info("\n");
		struct cec_msg msg;
		cec_msg_init(&msg, logicalAddress, message.address);
		memcpy(&msg.msg[1], message.data, message.length);
		msg.len = message.length + 1;
		ioctl(hdmiFd, CEC_TRANSMIT, &msg);
	}
}

void cVideo::SetCECAutoStandby(bool state)
{
	standby_cec_activ = state;
}

void cVideo::SetCECAutoView(bool state)
{
	autoview_cec_activ = state;	
}

void cVideo::SetCECState(bool state)
{
	struct cec_message message;

	if ((standby_cec_activ) && state){
		message.address = CEC_OP_PRIM_DEVTYPE_TV;
		message.data[0] = CEC_MSG_STANDBY;
		message.length = 1;
		SendCECMessage(message);
	}

	if ((autoview_cec_activ) && !state){
		message.address = CEC_OP_PRIM_DEVTYPE_TV;
		message.data[0] = CEC_MSG_IMAGE_VIEW_ON;
		message.length = 1;
		SendCECMessage(message);
		usleep(10000);
		message.address = 0x0f; /* broadcast */
		message.data[0] = CEC_MSG_ACTIVE_SOURCE;
		//message.data[1] = ((((int)physicalAddress >> 12) & 0xf) << 4) + (((int)physicalAddress >> 8) & 0xf);
		//message.data[2] = ((((int)physicalAddress >> 4) & 0xf) << 4)  + (((int)physicalAddress >> 0) & 0xf);
		message.data[1] = physicalAddress[0];
		message.data[2] = physicalAddress[1];
		message.length = 3;
		SendCECMessage(message);
	}

}

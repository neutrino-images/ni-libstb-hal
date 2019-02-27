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
#include "hal_debug.h"
#include "hdmi_cec.h"

#include <proc_tools.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define hal_debug(args...) _hal_debug(HAL_DEBUG_VIDEO, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_VIDEO, this, args)
#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_VIDEO, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_VIDEO, NULL, args)

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

void write_frame(AVFrame* in_frame, int fd)
{
	if(in_frame == NULL)
		return;
	static const unsigned char pes_header[] = {0x0, 0x0, 0x1, 0xe0, 0x00, 0x00, 0x80, 0x80, 0x5, 0x21, 0x0, 0x1, 0x0, 0x1};

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
					int i =1;
					/* get the delayed frames */
					in_frame->pts = i;
					ret = avcodec_encode_video2(codec_context, &pkt, 0, &got_output);
					if (ret != -1 && got_output){
						if ((pkt.data[3] >> 4) != 0xE){
							write_all(fd, pes_header, sizeof(pes_header));
						}else{
							pkt.data[4] = pkt.data[5] = 0x00;
						}
						write_all(fd,pkt.data, pkt.size);
						av_packet_unref(&pkt);
					}
				}
			}
			avcodec_close(codec_context);
			av_free(codec_context);
		}
	}
}

int decode_frame(AVCodecContext *codecContext,AVPacket &packet, int fd)
{
	int decode_ok = 0;
	AVFrame *frame = av_frame_alloc();
	if(frame){
		if ((avcodec_decode_video2(codecContext, frame, &decode_ok, &packet)) < 0 || !decode_ok){
			av_frame_free(&frame);
			return -1;
		}
		AVFrame *dest_frame = av_frame_alloc();
		if(dest_frame){
			dest_frame->height = (frame->height/2)*2;
			dest_frame->width = (frame->width/2)*2;
			dest_frame->format = AV_PIX_FMT_YUV420P;
			av_frame_get_buffer(dest_frame, 32);
			struct SwsContext *convert = NULL;
			convert = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, dest_frame->width, dest_frame->height, AV_PIX_FMT_YUVJ420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
			if(convert){
				sws_scale(convert, frame->data, frame->linesize, 0, frame->height, dest_frame->data, dest_frame->linesize);
				sws_freeContext(convert);
			}
			write_frame(dest_frame, fd);
			av_frame_free(&dest_frame);
		}
		av_frame_free(&frame);
	}
	return 0;

}

AVCodecContext* open_codec(AVMediaType mediaType, AVFormatContext* formatContext)
{
	int stream_index = av_find_best_stream(formatContext, mediaType, -1, -1, NULL, 0);
	if (stream_index >=0 ){
		AVCodecContext * codecContext = formatContext->streams[stream_index]->codec;
		if(codecContext){
			AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
			if(codec){
				if ((avcodec_open2(codecContext, codec, NULL)) != 0){
					return NULL;
				}
			}
			return codecContext;
		}
	}
	return NULL;
}

int image_to_mpeg2(const char *image_name, int fd)
{
	int ret = 0;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
	avcodec_register_all();
#endif

	AVFormatContext *formatContext = avformat_alloc_context();
	if (formatContext && (ret = avformat_open_input(&formatContext, image_name, NULL, NULL)) == 0){
		AVCodecContext *codecContext = open_codec(AVMEDIA_TYPE_VIDEO, formatContext);
		if(codecContext){
			AVPacket packet;
			av_init_packet(&packet);
			if ((ret = av_read_frame(formatContext, &packet)) !=-1){
				if((ret = decode_frame(codecContext, packet, fd)) != 1){
				/* add sequence end code to have a real mpeg file */
					uint8_t endcode[] = { 0, 0, 1, 0xb7 };
					write_all(fd,endcode, sizeof(endcode));
				}
				av_packet_unref(&packet);
			}
			avcodec_close(codecContext);
		}
		avformat_close_input(&formatContext);
	}
	av_free(formatContext);
	return ret;
}
enum{ENCODER,AUX};
void setAVInput(int val)
{
	int input_fd = open("/proc/stb/avs/0/input", O_WRONLY);
	if(input_fd){
		const char *input[] = {"encoder", "aux"};
		write(input_fd, input[val], strlen(input[val]));
		close(input_fd);
	}
}

cVideo::cVideo(int, void *, void *, unsigned int unit)
{
	hal_debug("%s unit %u\n", __func__, unit);

	brightness = -1;
	contrast = -1;
	saturation = -1;
	hue = -1;
	video_standby = 0;
	if (unit > 1) {
		hal_info("%s: unit %d out of range, setting to 0\n", __func__, unit);
		devnum = 0;
	} else
		devnum = unit;
	fd = -1;
	openDevice();
	setAVInput(ENCODER);
}

cVideo::~cVideo(void)
{
	if(fd >= 0)
		setAVInput(AUX);
	if (hdmi_cec::getInstance()->standby_cec_activ && fd >= 0)
		hdmi_cec::getInstance()->SetCECState(true);

	closeDevice();
}

void cVideo::openDevice(void)
{
	int n = 0;
	hal_debug("#%d: %s\n", devnum, __func__);
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
		hal_info("#%d: %s cannot open %s: %m, retries %d\n", devnum, __func__, VDEV[devnum], n);
	}
	playstate = VIDEO_STOPPED;
}

void cVideo::closeDevice(void)
{
	hal_debug("%s\n", __func__);
	/* looks like sometimes close is unhappy about non-empty buffers */
//	Start();
	if (fd >= 0)
		close(fd);
	fd = -1;
	playstate = VIDEO_STOPPED;
}

int cVideo::setAspectRatio(int aspect, int mode)
{
	static const char *a[] = { "n/a", "4:3", "14:9", "16:9" };
//	static const char *m[] = { "panscan", "letterbox", "bestfit", "nonlinear", "(unset)" };
	static const char *m[] = { "letterbox", "panscan", "bestfit", "nonlinear", "(unset)" };
	int n;

	int mo = (mode < 0||mode > 3) ? 4 : mode;
	hal_debug("%s: a:%d m:%d  %s\n", __func__, aspect, mode, m[mo]);

	if (aspect > 3 || aspect == 0)
		hal_info("%s: invalid aspect: %d\n", __func__, aspect);
	else if (aspect > 0) /* -1 == don't set */
	{
		hal_debug("%s: /proc/stb/video/aspect -> %s\n", __func__, a[aspect]);
		n = proc_put("/proc/stb/video/aspect", a[aspect], strlen(a[aspect]));
		if (n < 0)
			hal_info("%s: proc_put /proc/stb/video/aspect (%m)\n", __func__);
	}

	if (mode == -1)
		return 0;

	hal_debug("%s: /proc/stb/video/policy -> %s\n", __func__, m[mo]);
	n = proc_put("/proc/stb/video/policy", m[mo], strlen(m[mo]));
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
		return n;
	}
	if (fop(ioctl, VIDEO_GET_SIZE, &s) < 0)
	{
		hal_info("%s: VIDEO_GET_SIZE %m\n", __func__);
		return -1;
	}
	hal_debug("#%d: %s: %d\n", devnum, __func__, s.aspect_ratio);
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
	hal_debug("%s(%d) => %s\n", __FUNCTION__, format, f);
	return fop(ioctl, MPEG_VID_SET_DISPMODE, format);
#endif
}

int cVideo::Start(void * /*PcrChannel*/, unsigned short /*PcrPid*/, unsigned short /*VideoPid*/, void * /*hChannel*/)
{
	hal_debug("#%d: %s playstate=%d\n", devnum, __func__, playstate);
#if 0
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
#endif
	/* implicitly do StopPicture() on video->Start() */
	if (stillpicture) {
		hal_info("%s: stillpicture == true, doing implicit StopPicture()\n", __func__);
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
	hal_debug("#%d: %s(%d)\n", devnum, __func__, blank);
	if (stillpicture)
	{
		hal_debug("%s: stillpicture == true\n", __func__);
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

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	hal_debug("%s(%d, %d)\n", __func__, video_system, remember);
	char current[32];

	if (video_system > VIDEO_STD_MAX)
	{
		hal_info("%s: video_system (%d) > VIDEO_STD_MAX (%d)\n", __func__, video_system, VIDEO_STD_MAX);
		return -1;
	}
	int ret = proc_get("/proc/stb/video/videomode", current, 32);
	if (strcmp(current, vid_modes[video_system]) == 0)
	{
		hal_info("%s: video_system %d (%s) already set, skipping\n", __func__, video_system, current);
		return 0;
	}
	hal_info("%s: old: '%s' new: '%s'\n", __func__, current, vid_modes[video_system]);
	bool stopped = false;
	if (playstate == VIDEO_PLAYING)
	{
		hal_info("%s: playstate == VIDEO_PLAYING, stopping video\n", __func__);
		Stop();
		stopped = true;
	}
	ret = proc_put("/proc/stb/video/videomode", vid_modes[video_system],strlen(vid_modes[video_system]));
	if (stopped)
		Start();

	return ret;
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
	hal_info("%s: could not find '%s' mode, returning VIDEO_STD_720P50\n", __func__, current);
	return VIDEO_STD_720P50;
}

void cVideo::GetVideoSystemFormatName(cs_vs_format_t *format, int system)
{
	if (system == -1)
		system = GetVideoSystem();
	if (system < 0 || system > VIDEO_STD_1080P50) {
		hal_info("%s: invalid system %d\n", __func__, system);
		strcpy(format->format, "invalid");
	} else
		strcpy(format->format, vid_modes[system]);
}

int cVideo::getPlayState(void)
{
	return playstate;
}

void cVideo::SetVideoMode(analog_mode_t mode)
{
	hal_debug("#%d: %s(%d)\n", devnum, __func__, mode);
	if (!(mode & ANALOG_SCART_MASK))
	{
		hal_debug("%s: non-SCART mode ignored\n", __func__);
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
			hal_info("%s unknown mode %d\n", __func__, mode);
			m = "rgb";
			break; /* default to rgb */
	}
	proc_put("/proc/stb/avs/0/colorformat", m, strlen(m));
}

void cVideo::ShowPicture(const char * fname)
{
	hal_debug("%s(%s)\n", __func__, fname);
	if (video_standby)
	{
		/* does not work and the driver does not seem to like it */
		hal_info("%s: video_standby == true\n", __func__);
		return;
	}
	struct stat st;
	if (stat(fname, &st)){
		return;
	}
	closeDevice();
	openDevice();
	if (fd >= 0)
	{
		usleep(50000);//workaround for switch to radiomode
		stillpicture = true;
		ioctl(fd, VIDEO_SET_STREAMTYPE, VIDEO_STREAMTYPE_MPEG2); // set to mpeg2
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY);
		ioctl(fd, VIDEO_PLAY);
		ioctl(fd, VIDEO_CONTINUE);
		ioctl(fd, VIDEO_CLEAR_BUFFER);
		image_to_mpeg2(fname, fd);
		unsigned char iframe[8192];
		memset(iframe,0xff,sizeof(iframe));
		write_all(fd, iframe, 8192);
		usleep(150000);
		ioctl(fd, VIDEO_STOP, 0);
		ioctl(fd, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
	}
	return;
}

void cVideo::StopPicture()
{
	hal_debug("%s\n", __func__);
	stillpicture = false;
	Stop(1);
	closeDevice();
	openDevice();
}

void cVideo::Standby(unsigned int bOn)
{
	hal_debug("%s(%d)\n", __func__, bOn);
	if (bOn)
	{
		closeDevice();
		setAVInput(AUX);
	}
	else
	{
		openDevice();
		setAVInput(ENCODER);
	}
	video_standby = bOn;
	hdmi_cec::getInstance()->SetCECState(video_standby);
}

int cVideo::getBlank(void)
{
	int ret = proc_get_hex(VMPEG_xres[devnum]);
	hal_debug("%s => %d\n", __func__, !ret);
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
	hal_debug("#%d %s: x:%d y:%d w:%d h:%d ow:%d oh:%d\n", devnum, __func__, x, y, w, h, osd_w, osd_h);
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
	hal_debug("#%d %s: x:%d y:%d w:%d h:%d xr:%d yr:%d\n", devnum, __func__, _x, _y, _w, _h, xres, yres);
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
		case 29970:
			return 3;
		case 30000:
			return 4;
		case 50000:
			return 5;
		case 59940:
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
		char buf[16];
		int n = proc_get(VMPEG_framerate[devnum], buf, 16);
		if (n > 0)
			sscanf(buf, "%i", &r);
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
	hal_debug("#%d: %s: rate: %d, width: %d height: %d\n", devnum, __func__, rate, width, height);
}

void cVideo::SetSyncMode(AVSYNC_TYPE mode)
{
	hal_debug("%s %d\n", __func__, mode);
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
	hal_debug("#%d: %s type=%s\n", devnum, __func__, VF[type]);

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
		hal_info("%s VIDEO_SET_STREAMTYPE(%d) failed: %m\n", __func__, t);
	return 0;
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	if (ioctl(fd, VIDEO_GET_PTS, &pts) < 0)
		hal_info("%s: GET_PTS failed (%m)\n", __func__);
	return pts;
}

void cVideo::SetDemux(cDemux *)
{
	hal_debug("#%d %s not implemented yet\n", devnum, __func__);
}

void cVideo::SetControl(int control, int value)
{
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
	case VIDEO_CONTROL_SHARPNESS:
		sharpness = value;
		p = "/proc/stb/vmpeg/0/pep_sharpness";
		break;
	case VIDEO_CONTROL_BLOCK_NOISE_REDUCTION:
		block_noise_reduction = value;
		p = "/proc/stb/vmpeg/0/pep_block_noise_reduction";
		break;
	case VIDEO_CONTROL_MOSQUITO_NOISE_REDUCTION:
		mosquito_noise_reduction = value;
		p = "/proc/stb/vmpeg/0/pep_mosquito_noise_reduction";
		break;
	case VIDEO_CONTROL_DIGITAL_CONTOUR_REMOVAL:
		digital_contour_removal = value;
		p = "/proc/stb/vmpeg/0/pep_digital_contour_removal";
		break;
	case VIDEO_CONTROL_AUTO_FLESH:
		auto_flesh = value;
		p = "/proc/stb/vmpeg/0/pep_auto_flesh";
		break;
	case VIDEO_CONTROL_GREEN_BOOST:
		green_boost = value;
		p = "/proc/stb/vmpeg/0/pep_green_boost";
		break;
	case VIDEO_CONTROL_BLUE_BOOST:
		blue_boost = value;
		p = "/proc/stb/vmpeg/0/pep_blue_boost";
		break;
	case VIDEO_CONTROL_DYNAMIC_CONTRAST:
		dynamic_contrast = value;
		p = "/proc/stb/vmpeg/0/pep_dynamic_contrast";
		break;
	case VIDEO_CONTROL_SCALER_SHARPNESS:
		scaler_sharpness = value;
		p = "/proc/stb/vmpeg/0/pep_scaler_sharpness";
		break;
	case VIDEO_CONTROL_ZAPPING_MODE:
		zapping_mode = value;
		const char *mode_zapping[] = { "hold", "mute" };
		proc_put("/proc/stb/video/zapping_mode", mode_zapping[zapping_mode], strlen(mode_zapping[zapping_mode]));
		break;
	}
	if (p) {
		char buf[20];
		int fix_value = value * 256;
		int len = snprintf(buf, sizeof(buf), "%.8X", fix_value);
		if (len < (int) sizeof(buf))
			proc_put(p, buf, len);
	}
}

void cVideo::SetColorFormat(COLOR_FORMAT color_format)
{
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
	case COLORFORMAT_HDMI_AUTO:
		p = "Edid(Auto)";
		break;
	case COLORFORMAT_HDMI_RGB:
		p = "Hdmi_Rgb";
		break;
	case COLORFORMAT_HDMI_YCBCR444:
		p = "444";
		break;
	case COLORFORMAT_HDMI_YCBCR422:
		p = "422";
		break;
	case COLORFORMAT_HDMI_YCBCR420:
		p = "420";
		break;
	}
	if (p)
		proc_put("/proc/stb/video/hdmi_colorspace", p, strlen(p));
}

bool getvideo2(unsigned char *video, int xres, int yres)
{
	bool ret = false;
	if(video ==  NULL)
		return ret;
	char videosnapshot[] = "/dev/dvb/adapter0/video0";
	int fd_video = open(videosnapshot, O_RDONLY);
	if (fd_video < 0) {
		perror(videosnapshot);
		return ret;
	}
	ssize_t r = read(fd_video, video, xres * yres * 3);
	if(r){
		ret = true;
	}
	close(fd_video);
	return ret;
}
static bool swscale(unsigned char *src, unsigned char *dst, int sw, int sh, int dw, int dh, AVPixelFormat sfmt)
{
	bool ret = false;
	int len = 0;
	struct SwsContext *scale = NULL;
	scale = sws_getCachedContext(scale, sw, sh, sfmt, dw, dh, AV_PIX_FMT_RGB32, SWS_BICUBIC, 0, 0, 0);
	if (!scale) {
		hal_info_c("%s: ERROR setting up SWS context\n", __func__);
		return ret;
	}
	AVFrame *sframe = av_frame_alloc();
	AVFrame *dframe = av_frame_alloc();
	if (sframe && dframe) {
		len = av_image_fill_arrays(sframe->data, sframe->linesize, &(src)[0], sfmt, sw, sh, 1);
		if(len>-1)
			ret = true;

		if(ret && (len = av_image_fill_arrays(dframe->data, dframe->linesize, &(dst)[0], AV_PIX_FMT_RGB32, dw, dh, 1)<0))
			ret = false;

		if(ret && (len = sws_scale(scale, sframe->data, sframe->linesize, 0, sh, dframe->data, dframe->linesize)<0))
			ret = false;
		else
			ret = true;
	}else{
		hal_info_c("%s: could not alloc sframe (%p) or dframe (%p)\n", __func__, sframe, dframe);
		ret = false;
	}

	if(sframe){
		av_frame_free(&sframe);
		sframe = NULL;
	}
	if(dframe){
		av_frame_free(&dframe);
		dframe = NULL;
	}
	if(scale){
		sws_freeContext(scale);
		scale = NULL;
	}
	hal_info_c("%s: %s scale %ix%i to %ix%i ,len %i\n",ret?" ":"ERROR",__func__, sw, sh, dw, dh,len);

	return ret;
}

// grabing the osd picture
void get_osd_size(int &xres, int &yres, int &bits_per_pixel)
{
	int fb=open("/dev/fb/0", O_RDWR);
	if (fb == -1)
	{
		fprintf(stderr, "Framebuffer failed\n");
		return;
	}

	struct fb_var_screeninfo var_screeninfo;
	if(ioctl(fb, FBIOGET_VSCREENINFO, &var_screeninfo) == -1)
	{
		fprintf(stderr, "Framebuffer: <FBIOGET_VSCREENINFO failed>\n");
		close(fb);
		return;
	}
	close(fb);

	bits_per_pixel = var_screeninfo.bits_per_pixel;
	xres=var_screeninfo.xres;
	yres=var_screeninfo.yres;
	fprintf(stderr, "... Framebuffer-Size: %d x %d\n",xres,yres);

}
void get_osd_buf(unsigned char *osd_data)
{
	unsigned char *lfb = NULL;
	struct fb_fix_screeninfo fix_screeninfo;
	struct fb_var_screeninfo var_screeninfo;

	int fb=open("/dev/fb/0", O_RDWR);
	if (fb == -1)
	{
		fprintf(stderr, "Framebuffer failed\n");
		return;
	}

	if(ioctl(fb, FBIOGET_FSCREENINFO, &fix_screeninfo) == -1)
	{
		fprintf(stderr, "Framebuffer: <FBIOGET_FSCREENINFO failed>\n");
		close(fb);
		return;
	}

	if(ioctl(fb, FBIOGET_VSCREENINFO, &var_screeninfo) == -1)
	{
		fprintf(stderr, "Framebuffer: <FBIOGET_VSCREENINFO failed>\n");
		close(fb);
		return;
	}

	if(!(lfb = (unsigned char*)mmap(0, fix_screeninfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0)))
	{
		fprintf(stderr, "Framebuffer: <Memmapping failed>\n");
		close(fb);
		return;
	}

	if ( var_screeninfo.bits_per_pixel == 32 )
	{
		fprintf(stderr, "Grabbing 32bit Framebuffer ...\n");
		// get 32bit framebuffer
		memcpy(osd_data,lfb,fix_screeninfo.line_length*var_screeninfo.yres);
	}
	close(fb);
}

inline void rgb24torgb32(unsigned char  *src, unsigned char *dest,int picsize) {
	for (int i = 0; i < picsize; i++) {
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = *src++;
		*dest++ = 255;
	}
}

/* TODO: aspect ratio correction and PIP */
bool cVideo::GetScreenImage(unsigned char * &out_data, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
#define VDEC_PIXFMT AV_PIX_FMT_BGR24

	hal_info("%s: out_data 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, out_data, xres, yres, get_video, get_osd, scale_to_video);
	int aspect = 0;
	getPictureInfo(xres, yres, aspect); /* aspect is dummy here */
	aspect = getAspectRatio();
	if(xres < 1 || yres < 1 )
		get_video = false;


	if(!get_video && !get_osd)
		return false;

	int osd_w = 0;
	int osd_h = 0;
	int bits_per_pixel = 0;
	if(get_osd){
		get_osd_size(osd_w, osd_h, bits_per_pixel);
		if(osd_w < 1 || osd_h < 1 || bits_per_pixel != 32)
			get_osd = false;
		if(!scale_to_video && get_osd){
			xres = osd_w;
			yres = osd_h;
		}
	}
	unsigned char *osd_data = NULL;
	out_data = (unsigned char *)malloc(xres * yres * 4);/* will be freed by caller */
	if (out_data == NULL)
		return false;

	if (get_video) {
		const int grab_w = 1920; const int grab_h = 1080; //hd51 video0 is always 1920x1080 
		unsigned char *video_src = (unsigned char *)malloc(grab_w * grab_h * 3);
		if (video_src == NULL)
			return false;
		if(getvideo2(video_src, grab_w,grab_h) == false){
			free(out_data);
			free(video_src);
			return false;
		}
		if (grab_w != xres || grab_h != yres){ /* scale video into data... */
			bool ret = swscale(video_src, out_data, grab_w, grab_h, xres, yres,VDEC_PIXFMT);
			if(!ret){
				free(out_data);
				free(video_src);
				return false;
			}
		}else{ /* get_video and no fancy scaling needed */
			rgb24torgb32(video_src, out_data, grab_w * grab_h);
		}
		free(video_src);
	}

	if(get_osd){
		osd_data = (unsigned char *)malloc(osd_w * osd_h * 4);
		if(osd_data)
			get_osd_buf(osd_data);
	}

	if (get_osd && (osd_w != xres || osd_h != yres)) {
		/* rescale osd */
		unsigned char *osd_src = (unsigned char *)malloc(xres * yres * 4);
		if(osd_src){
			bool ret = swscale(osd_data, osd_src, osd_w, osd_h, xres, yres,AV_PIX_FMT_RGB32);
			if(!ret){
				free(out_data);
				free(osd_data);
				free(osd_src);
				return false;
			}
			free(osd_data);
			osd_data = NULL;
			osd_data = osd_src;
		}else{
			free(out_data);
			free(osd_data);
			return false;
		}
	}

	if (get_video && get_osd) {
		/* alpha blend osd onto out_data (video). TODO: maybe libavcodec can do this? */
		uint32_t *d = (uint32_t *)out_data;
		uint32_t *pixpos = (uint32_t *) osd_data;
		for (int count = 0; count < yres; count++) {
			for (int count2 = 0; count2 < xres; count2++ ) {
				uint32_t pix = *pixpos;
				if ((pix & 0xff000000) == 0xff000000)
					*d = pix;
				else {
					uint8_t *in = (uint8_t *)(pixpos);
					uint8_t *out = (uint8_t *)d;
					int a = in[3];	/* TODO: big/little endian? */
					*out = (*out + ((*in - *out) * a) / 256);
					in++; out++;
					*out = (*out + ((*in - *out) * a) / 256);
					in++; out++;
					*out = (*out + ((*in - *out) * a) / 256);
				}
				d++;
				pixpos++;
			}
		}
	}
	else if (get_osd) /* only get_osd, out_data is not yet populated */
		memcpy(out_data, osd_data, xres * yres * sizeof(uint32_t));

	if(osd_data)
		free(osd_data);

	return true;
}

bool cVideo::SetCECMode(VIDEO_HDMI_CEC_MODE _deviceType)
{
	return hdmi_cec::getInstance()->SetCECMode(_deviceType);
}

void cVideo::SetCECAutoStandby(bool state)
{
	hdmi_cec::getInstance()->SetCECAutoStandby(state);
}

void cVideo::SetCECAutoView(bool state)
{
	hdmi_cec::getInstance()->SetCECAutoView(state);
}

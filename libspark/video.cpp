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
#include <linux/stmfb.h>
#include <bpamem.h>
#include "video_lib.h"
#include "hal_debug.h"

#include <proc_tools.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
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

static bool hdmi_enabled = true;
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

static const char *VMPEG_dst_all[] = {
	"/proc/stb/vmpeg/0/dst_all",
	"/proc/stb/vmpeg/1/dst_all"
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
	"720p50",	// VIDEO_STD_AUTO -> not implemented
	"1080p50",	// VIDEO_STD_1080P50 -> SPARK only
	NULL
};

#define VIDEO_STREAMTYPE_MPEG2 0
#define VIDEO_STREAMTYPE_MPEG4_H264 1
#define VIDEO_STREAMTYPE_VC1 3
#define VIDEO_STREAMTYPE_MPEG4_Part2 4
#define VIDEO_STREAMTYPE_VC1_SM 5
#define VIDEO_STREAMTYPE_MPEG1 6
#define VIDEO_STREAMTYPE_H265_HEVC 7
#define VIDEO_STREAMTYPE_AVS 16

static int hdmi_out(bool enable)
{
	struct stmfbio_output_configuration out;
	int ret = -1;
	hal_info_c("%s(%d)\n", __func__, enable);
	int fb = open("/dev/fb0", O_RDWR);
	if (fb < 0)
	{
		hal_debug_c("%s: can't open /dev/fb0 (%m)\n", __func__);
		return -1;
	}
	out.outputid = STMFBIO_OUTPUTID_MAIN;
	if (ioctl(fb, STMFBIO_GET_OUTPUT_CONFIG, &out) < 0)
	{
		hal_debug_c("%s: STMFBIO_GET_OUTPUT_CONFIG (%m)\n", __func__);
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
		_hal_debug(HAL_DEBUG_VIDEO, NULL, "%s: STMFBIO_SET_OUTPUT_CONFIG (%m)\n", __func__);
out:
	close(fb);
	return ret;
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
				}
			}
			avcodec_close(codec_context);
			av_free(codec_context);
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
			write_frame(dest_frame, fp);
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
				av_free_packet(&packet);
			}
			avcodec_close(codecContext);
		}
		avformat_close_input(&formatContext);
	}
	av_free(formatContext);
	return 0;
}

cVideo::cVideo(int, void *, void *, unsigned int unit)
{
	hal_debug("%s unit %u\n", __func__, unit);

	brightness = -1;
	contrast = -1;
	saturation = -1;
	hue = -1;

	scartvoltage = -1;
	video_standby = 0;
	if (unit > 1) {
		hal_info("%s: unit %d out of range, setting to 0\n", __func__, unit);
		devnum = 0;
	} else
		devnum = unit;
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
		return n * 2 + 1;
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
	hdmi_out(false);
	ret = proc_put("/proc/stb/video/videomode", vid_modes[video_system],strlen(vid_modes[video_system]));
	hdmi_out(true);
	if (stopped)
		Start();

	return ret;
}

int cVideo::GetVideoSystem(void)
{
	char current[32];
	proc_get("/proc/stb/video/videomode", current, 32);
	for (int i = 2; vid_modes[i]; i++) /* 0,1,2 are all "pal" */
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

bool cVideo::ShowPicture(const char * fname, const char *_destname)
{
	bool ret = false;
	hal_debug("%s(%s)\n", __func__, fname);
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
		hal_info("%s: video_standby == true\n", __func__);
		return ret;
	}
	if (fd == -1)
	{
		/* in movieplayer mode, fd is not opened */
		hal_info("%s: decoder not opened\n", __func__);
		return ret;
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
				hal_info("%s: could not stat %s (%m)\n", __func__, fname);
				return ret;
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
		hal_info("%s cannot open %s: %m\n", __func__, destname);
		goto out;
	}
	fstat(mfd, &st);

	closeDevice();
	openDevice();

	if (fd >= 0)
	{
		stillpicture = true;

		if (ioctl(fd, VIDEO_SET_FORMAT, VIDEO_FORMAT_16_9) < 0)
			hal_info("%s: VIDEO_SET_FORMAT failed (%m)\n", __func__);
		bool seq_end_avail = false;
		off_t pos=0;
		unsigned char *iframe = (unsigned char *)malloc((st.st_size < 8192) ? 8192 : st.st_size);
		if (! iframe)
		{
			hal_info("%s: malloc failed (%m)\n", __func__);
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
		ret = true;
	}
 out:
	close(mfd);
	return ret;
}

void cVideo::StopPicture()
{
	hal_debug("%s\n", __func__);
	stillpicture = false;
	Stop(1);
}

void cVideo::Standby(unsigned int bOn)
{
	hal_debug("%s(%d)\n", __func__, bOn);
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
	static unsigned int lastcount = 0;
	unsigned int count = 0;
	size_t n = 0;
	ssize_t r;
	char *line = NULL;
	/* hack: the "mailbox" irq is not increasing if
	 * no audio or video is decoded... */
	FILE *f = fopen("/proc/interrupts", "r");
	if (! f) /* huh? */
		return 0;
	while ((r = getline(&line, &n, f)) != -1)
	{
		if (r <= (ssize_t) strlen("mailbox")) /* should not happen... */
			continue;
		line[r - 1] = 0; /* remove \n */
		if (!strcmp(&line[r - 1 - strlen("mailbox")], "mailbox"))
		{
			count =  atoi(line + 5);
			break;
		}
	}
	free(line);
	fclose(f);
	int ret = (count == lastcount); /* no new decode -> return 1 */
	hal_debug("#%d: %s: %d (irq++: %d)\n", devnum, __func__, ret, count - lastcount);
	lastcount = count;
	return ret;
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
		hal_debug("%s params changed. old: %08x new: %08x\n", __FUNCTION__, _v_info, v_info);
		setAspectRatio(-1, -1);
	}
	_v_info = v_info;
#endif
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
	sprintf(buffer, "%x %x %x %x", _x, _y, _w, _h);
	proc_put(VMPEG_dst_all[devnum], buffer, strlen(buffer));
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

void cVideo::SetControl(int control, int value) {
	const char *p = NULL;
	switch (control) {
	case VIDEO_CONTROL_BRIGHTNESS:
		brightness = value;
		p = "/proc/stb/video/plane/psi_brightness";
		break;
	case VIDEO_CONTROL_CONTRAST:
		contrast = value;
		p = "/proc/stb/video/plane/psi_contrast";
		break;
	case VIDEO_CONTROL_SATURATION:
		saturation = value;
		p = "/proc/stb/video/plane/psi_saturation";
		break;
	case VIDEO_CONTROL_HUE:
		hue = value;
		p = "/proc/stb/video/plane/psi_tint";
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
		proc_put("/proc/stb/avs/0/colorformat", p, strlen(p));
}

/* get an image of the video screen
 * this code is inspired by dreambox AIO-grab,
 * git://schwerkraft.elitedvb.net/aio-grab/aio-grab.git
 * and the patches for STi support from
 * https://github.com/Schischu/STLinux.BSP-Duckbox.git */
/* static lookup tables for faster yuv2rgb conversion */
static const uint32_t yuv2rgbtable_y[256] = {
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
static const uint32_t yuv2rgbtable_ru[256] = {
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
static const uint32_t yuv2rgbtable_gu[256] = {
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
static const uint32_t yuv2rgbtable_gv[256] = {
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
static const uint32_t yuv2rgbtable_bv[256] = {
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

#define OUT(x) \
	out[OUTITER] = (uint8_t)*(decode_surface + x)&0xFF; \
	OUTITER += OUTINC;

#define OUT4(x) \
	OUT(x + 0x03); \
	OUT(x + 0x02); \
	OUT(x + 0x01); \
	OUT(x + 0x00);

#define OUT8(x) \
	OUT4(x + 0x04); \
	OUT4(x + 0x00);

#define OUT_LU_16A(x) \
	OUT8(x); \
	OUT8(x + 0x40);

#define OUT_CH_8A(x) \
	OUT4(x); \
	OUT4(x + 0x20);

//pppppppppppppppp
//x: macroblock address
//l: line 0-15
#define OUT_LU_16(x,l) \
	OUT_LU_16A(x + (l/4) * 0x10 + (l%2) * 0x80 + ((l/2)%2?0x00:0x08));

//pppppppp
//x: macroblock address
//l: line 0-7
//b: 0=cr 1=cb
#define OUT_CH_8(x,l,b) \
	OUT_CH_8A(x + (l/4) * 0x10 + (l%2) * 0x40 + ((l/2)%2?0x00:0x08) + (b?0x04:0x00));

//----
#define CLAMP(x)	((x < 0) ? 0 : ((x > 255) ? 255 : x))
#define SWAP(x,y)	{ x ^= y; y ^= x; x ^= y; }

/* TODO: aspect ratio correction and PIP */
bool cVideo::GetScreenImage(unsigned char * &video, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	hal_info("%s: get_video: %d get_osd: %d scale_to_video: %d\n",
		 __func__, get_video, get_osd, scale_to_video);

	int fbfd = -1, bpafd = -1;
	int vid_x, vid_y, osd_x, osd_y, aspect;
	struct fb_fix_screeninfo fix_screeninfo;
	struct fb_var_screeninfo var_screeninfo;
	uint8_t *osd, *vid;
	uint8_t *bpa = (uint8_t *)MAP_FAILED;

	/* this hints at incorrect usage */
	if (video != NULL)
		hal_info("%s: WARNING, video != NULL?\n", __func__);

	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0) {
		hal_info("%s: cannot open open /dev/fb0 (%m)\n", __func__);
		return false;
	}
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fix_screeninfo) == -1)
		hal_info("%s: FBIOGET_FSCREENINFO (%m)\n", __func__);

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &var_screeninfo) == -1)
		hal_info("%s: FBIOGET_VSCREENINFO (%m)\n", __func__);

	if (var_screeninfo.bits_per_pixel != 32) {
		hal_info("%s: only 32bit framebuffer supported.\n", __func__);
		close(fbfd);
		return false;
	}
	if (fix_screeninfo.line_length - (var_screeninfo.xres * 4) != 0) {
		hal_info("%s: framebuffer with offset not supported.\n", __func__);
		close(fbfd);
		return false;
	}

	osd_x = var_screeninfo.xres;
	osd_y = var_screeninfo.yres;
	getPictureInfo(vid_x, vid_y, aspect); /* aspect is dummy here */
	aspect = getAspectRatio();
	//if x and y is zero than this is most probably a stillpicture
	if (vid_x == 0)
		vid_x = 1280;
	if (vid_y == 0)
		vid_y = 720;

	if (get_video && get_osd)
	{
		if (scale_to_video) {
			xres = vid_x;
			yres = vid_y;
		} else {
			xres = osd_x;
			yres = osd_y;
		}
	}
	else if (get_video)
	{
		xres = vid_x;
		yres = vid_y;
	}
	else
	{
		xres = osd_x;
		yres = osd_y;
	}

	int vidmem = 0;
	int osdmem = 0;
	int outmem = xres * yres * 4;
	if (get_video)
		vidmem = vid_x * vid_y * 3;
	if (get_osd)
		osdmem = osd_x * osd_y * 4;

	bpafd = open("/dev/bpamem0", O_RDWR | O_CLOEXEC);
	if (bpafd < 0)
	{
		hal_info("%s: cannot open /dev/bpamem0: %m\n", __func__);
		goto error_cleanup;
	}
	BPAMemAllocMemData bpa_data;
	bpa_data.bpa_part = (char *)"LMI_VID";
	bpa_data.mem_size = outmem + osdmem + vidmem + 4096;
	if (ioctl(bpafd, BPAMEMIO_ALLOCMEM, &bpa_data))
	{
		hal_info("%s: cannot allocate %lu bytes of BPA2 memory\n", __func__, bpa_data.mem_size);
		goto error_cleanup;
	}
	close(bpafd);

	char bpa_mem_device[30];
	sprintf(bpa_mem_device, "/dev/bpamem%d", bpa_data.device_num);
	bpafd = open(bpa_mem_device, O_RDWR | O_CLOEXEC);
	if (bpafd < 0)
	{
		hal_info("%s: cannot open secondary bpamem device %s: %m\n", __func__, bpa_mem_device);
		goto error_cleanup;
	}
	bpa = (uint8_t *)mmap(0, bpa_data.mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, bpafd, 0);
	if (bpa == MAP_FAILED)
	{
		hal_info("%s: cannot map from bpamem: %m\n", __func__);
		ioctl(bpafd, BPAMEMIO_FREEMEM);
		goto error_cleanup;
	}

	vid = bpa + outmem;
	osd = vid + vidmem;
	if (get_video)
	{
		int mfd = -1;
		uint8_t *decode_surface;
		// player2_191/linux/Makefile:CCFLAGSY+=-DPLAYER2_PRIMARY_VIDEO_BUFFER_MEMORY=0x02400000
		unsigned long vid_mem_size  = 0x02400000;
		unsigned long vid_phys_addr = 0x00000000;
		char buf[512];
		FILE *pipe = fopen("/proc/bpa2", "r");
		if (pipe)
		{
			bool found_part = false;
			unsigned long mem_size = 0;
			unsigned long phys_addr = 0;
			while (fgets(buf, sizeof(buf), pipe))
			{
				if (found_part || strstr(buf, "LMI_VID") != NULL)
				{
					found_part = true;
					if (sscanf(buf, "- %lu B at %lx", &mem_size, &phys_addr) == 2)
					{
						if (mem_size == vid_mem_size)
						{
							vid_phys_addr = phys_addr;
							break;
						}
					}
				}
			}
			fclose(pipe);
		}
		if (vid_phys_addr == 0) {
			hal_info("%s: primary display pane not found in /proc/bpa2\n", __func__);
		}

		mfd = open("/dev/mem", O_RDWR | O_CLOEXEC);
		if (mfd < 0) {
			hal_info("%s: cannot open open /dev/mem (%m)\n", __func__);
			goto error_cleanup;
		}

		hal_info("%s: Using bpa2 part LMI_VID - 0x%lx %lu\n", __func__, vid_phys_addr, vid_mem_size);
		decode_surface = (uint8_t *)mmap(0, vid_mem_size, PROT_READ, MAP_SHARED, mfd, vid_phys_addr);
		if (decode_surface == MAP_FAILED) {
			hal_info("%s: cannot mmap /dev/mem for VIDEO (%m)\n", __func__);
			close(mfd);
			goto error_cleanup;
		}

		int yblock, xblock, iyblock, ixblock, yblockoffset, offset, layer_offset, OUTITER, OUTINC, OUTITERoffset;
		uint8_t *out;
		uint8_t even, cr;
		int stride_half = vid_x / 2;
		uint8_t *luma, *chroma;
		luma = bpa;
		chroma = luma + (vid_x * vid_y);
		//memset(chroma, 0x80, vid_x * vid_y / 2);

		//we do not have to round that every luma res will be a multiple of 16
		yblock = vid_y / 16; //45
		xblock = vid_x / 16; //80

		//thereby yblockoffset does also not to be rounded up
		yblockoffset = xblock * 256/*16x16px*/ * 2/*2 block rows*/; //0xA000 for 1280

		ioctl(fbfd, FBIO_WAITFORVSYNC, 0);
#if 0
		if (vdec)
			ioctl(vdec->fd, VIDEO_FREEZE);
#endif
		//luma
		layer_offset  = 0;
		OUTITER       = 0;
		OUTITERoffset = 0;
		OUTINC        = 1; /*no spaces between pixel*/
		out           = luma;
		//now we have 16,6ms(60hz) to 50ms(20hz) to get the whole picture
		for (even = 0; even < 2; even++)
		{
			offset        = layer_offset + (even  << 8); /* * 0x100*/
			OUTITERoffset = even * xblock << 8; /* * 256=16x16px*/

			for (iyblock = even; iyblock < yblock; iyblock += 2)
			{
				for (ixblock = 0; ixblock < xblock; ixblock++)
				{
					int line;
					OUTITER = OUTITERoffset;
					for (line = 0; line < 16; line++)
					{
						OUT_LU_16(offset, line);
						OUTITER += (vid_x - 16); // we have already incremented by 16
					}

					//0x00, 0x200, ...
					offset += 0x200;
					OUTITERoffset += 16;
				}
				OUTITERoffset += (vid_x << 5) - vid_x; /* * 31*/
			}
		}

		//chroma
		layer_offset = ((vid_x * vid_y + (yblockoffset >> 1 /* /2*/ /*round up*/)) / yblockoffset) * yblockoffset;

		//cb
		//we do not have to round that every chroma y res will be a multiple of 16
		//and every chroma x res /2 will be a multiple of 8
		yblock = vid_y >> 4;/// 16; //45
		xblock = stride_half >> 3;/// 8; //no roundin

		//if xblock is not even than we will have to move to the next even value an
		yblockoffset = (((xblock + 1) >> 1 /* / 2*/) << 1 /* * 2*/ ) << 8 /* * 64=8x8px * 2=2 block rows * 2=cr cb*/;

		OUTITER       = 0;
		OUTITERoffset = 0;
		OUTINC        = 2;
		out           = chroma;

		for(cr = 0; cr < 2; cr++)
		{
			for(even = 0; even < 2; even++)
			{
				offset        = layer_offset + (even  << 8 /* * 0x100*/);
				OUTITERoffset = even * (xblock << 7 /* * 128=8x8px * 2*/) + cr;

				for (iyblock = even; iyblock < yblock; iyblock+=2)
				{
					for (ixblock = 0; ixblock < xblock; ixblock++)
					{
						int line;
						OUTITER = OUTITERoffset;

						for (line = 0; line < 8; line++)
						{
							OUT_CH_8(offset, line, !cr);
							OUTITER += (vid_x - 16); /*we have already incremented by OUTINC*8=16*/
						}

						//0x00 0x80 0x200 0x280, ...
						offset += (offset%0x100?0x180/*80->200*/:0x80/*0->80*/);
						OUTITERoffset += 16/*OUTINC*8=16*/;
					}
					OUTITERoffset += (vid_x << 4) - vid_x /* * 15*/;
				}
			}
		}
		munmap(decode_surface, vid_mem_size);
		close(mfd);
#if 0
		if (vdec)
			ioctl(vdec->fd, VIDEO_CONTINUE);
#endif
		/* yuv2rgb conversion (4:2:0)
		   TODO: there has to be a way to use the blitter for this */
		const int rgbstride = vid_x * 3;
		const int scans = vid_y / 2;
		int y;
		for (y=0; y < scans; ++y)
		{
			int x;
			int out1 = y * rgbstride * 2;
			int pos = y * vid_x * 2;
			const uint8_t *chroma_p = chroma + (y * vid_x);

			for (x = vid_x; x != 0; x -= 2)
			{
				int U = *chroma_p++;
				int V = *chroma_p++;

				int RU = yuv2rgbtable_ru[U]; // use lookup tables to speedup the whole thing
				int GU = yuv2rgbtable_gu[U];
				int GV = yuv2rgbtable_gv[V];
				int BV = yuv2rgbtable_bv[V];

				// now we do 4 pixels on each iteration this is more code but much faster
				int Y = yuv2rgbtable_y[luma[pos]];

				//p0:0
				vid[out1  ] = CLAMP((Y + RU)>>16);
				vid[out1+1] = CLAMP((Y - GV - GU)>>16);
				vid[out1+2] = CLAMP((Y + BV)>>16);

				Y = yuv2rgbtable_y[luma[vid_x + pos]];

				//p1:0
				vid[out1  +rgbstride] = CLAMP((Y + RU)>>16);
				vid[out1+1+rgbstride] = CLAMP((Y - GV - GU)>>16);
				vid[out1+2+rgbstride] = CLAMP((Y + BV)>>16);

				out1 += 3;
				pos++;

				Y = yuv2rgbtable_y[luma[pos]];

				//p0:1
				vid[out1  ] = CLAMP((Y + RU)>>16);
				vid[out1+1] = CLAMP((Y - GV - GU)>>16);
				vid[out1+2] = CLAMP((Y + BV)>>16);

				Y = yuv2rgbtable_y[luma[vid_x + pos]];

				//p1:1
				vid[out1  +rgbstride] = CLAMP((Y + RU)>>16);
				vid[out1+1+rgbstride] = CLAMP((Y - GV - GU)>>16);
				vid[out1+2+rgbstride] = CLAMP((Y + BV)>>16);

				out1 += 3;
				pos++;
			}
		}
	}

	if (get_osd)
	{
		uint8_t *lfb = (uint8_t *)mmap(0, fix_screeninfo.smem_len, PROT_READ, MAP_SHARED, fbfd, 0);
		if (lfb == MAP_FAILED)
			hal_info("%s: mmap fb memory failed (%m)\n", __func__);
		else {
			memcpy(osd, lfb, fix_screeninfo.line_length*var_screeninfo.yres);
			munmap(lfb, fix_screeninfo.smem_len);
		}
	}

	/* use the blitter to copy / scale / blend the pictures */
	STMFBIO_BLT_EXTERN_DATA blt_data;
	if (get_video)
	{
		int pip_x = 0;
		int pip_y = 0;
		int pip_w = xres;
		int pip_h = yres;
		bool scale = false;
		if (get_osd) {
			pip_x = proc_get_hex("/proc/stb/vmpeg/0/dst_left");
			pip_y = proc_get_hex("/proc/stb/vmpeg/0/dst_top");
			pip_w = proc_get_hex("/proc/stb/vmpeg/0/dst_width");
			pip_h = proc_get_hex("/proc/stb/vmpeg/0/dst_height");
			if (pip_x != 0 || pip_y != 0 || pip_w != 720 || pip_h != 576)
				scale = true;
			pip_x = pip_x * xres / 720;
			pip_y = pip_y * yres / 576;
			pip_w = pip_w * xres / 720;
			pip_h = pip_h * yres / 576;
			if (scale == false && aspect == 1)
			{
				pip_w = xres * 9/16*4/3;
				pip_x = (xres - pip_w) / 2;
			}
		}
		if (scale || aspect == 1) {
			/* todo: use the blitter, luke */
			uint8_t *p = bpa - 1;
			for (int i = 0; i < outmem; i += 4) {
				*++p = 0; *++p = 0; *++p = 0; *++p = 0xff;
			}
		}

		memset(&blt_data, 0, sizeof(STMFBIO_BLT_EXTERN_DATA));
		blt_data.operation  = BLT_OP_COPY;
		blt_data.ulFlags    = 0;
		blt_data.srcOffset  = 0;
		blt_data.srcPitch   = vid_x * 3;
		blt_data.dstOffset  = 0;
		blt_data.dstPitch   = xres * 4;
		blt_data.src_left   = 0;
		blt_data.src_top    = 0;
		blt_data.src_right  = vid_x;
		blt_data.src_bottom = vid_y;
		blt_data.dst_left   = pip_x;
		blt_data.dst_top    = pip_y;
		blt_data.dst_right  = pip_x + pip_w;
		blt_data.dst_bottom = pip_y + pip_h;
		blt_data.srcFormat  = SURF_RGB888;
		blt_data.dstFormat  = SURF_ARGB8888;
		blt_data.srcMemBase = (char *)vid;
		blt_data.dstMemBase = (char *)bpa;
		blt_data.srcMemSize = vidmem;
		blt_data.dstMemSize = outmem;
		if (ioctl(fbfd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
			hal_info("%s: STMFBIO_BLT_EXTERN video: %m\n", __func__);
	}
	if (get_osd)
	{
		memset(&blt_data, 0, sizeof(STMFBIO_BLT_EXTERN_DATA));
		blt_data.operation  = BLT_OP_COPY;
		if (get_video)
			blt_data.ulFlags    = BLT_OP_FLAGS_BLEND_SRC_ALPHA|BLT_OP_FLAGS_BLEND_DST_MEMORY;
		else
			blt_data.ulFlags    = 0;
		blt_data.srcOffset  = 0;
		blt_data.srcPitch   = fix_screeninfo.line_length;
		blt_data.dstOffset  = 0;
		blt_data.dstPitch   = xres * 4;
		blt_data.src_left   = 0;
		blt_data.src_top    = 0;
		blt_data.src_right  = osd_x;
		blt_data.src_bottom = osd_y;
		blt_data.dst_left   = 0;
		blt_data.dst_top    = 0;
		blt_data.dst_right  = xres;
		blt_data.dst_bottom = yres;
		blt_data.srcFormat  = SURF_ARGB8888;
		blt_data.dstFormat  = SURF_ARGB8888;
		blt_data.srcMemBase = (char *)osd;
		blt_data.dstMemBase = (char *)bpa;
		blt_data.srcMemSize = osdmem;
		blt_data.dstMemSize = outmem;
		if (ioctl(fbfd, STMFBIO_BLT_EXTERN, &blt_data) < 0)
			hal_info("%s: STMFBIO_BLT_EXTERN osd: %m\n", __func__);
	}
	ioctl(fbfd, STMFBIO_SYNC_BLITTER);

	video = (unsigned char *)malloc(outmem);
	if (video)
		memcpy(video, bpa, outmem);
	else
		hal_info("%s: could not allocate screenshot buffer (%d bytes)\n", __func__, xres * yres * 4);
	munmap(bpa, bpa_data.mem_size);
	ioctl(bpafd, BPAMEMIO_FREEMEM);
	close(bpafd);
	close(fbfd);
	return true;

 error_cleanup:
	if (bpa != MAP_FAILED)
		munmap(bpa, bpa_data.mem_size);
	if (bpafd > -1) {
		ioctl(bpafd, BPAMEMIO_FREEMEM);
		close(bpafd);
	}
	if (fbfd > -1)
		close(fbfd);
	return false;
}

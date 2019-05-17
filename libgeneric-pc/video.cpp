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
 *
 * cVideo implementation with decoder.
 * uses ffmpeg <http://ffmpeg.org> for demuxing / decoding
 * decoded frames are stored in SWFramebuffer class
 *
 * TODO: buffer handling surely needs some locking...
 */

#include "config.h"
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

/* ffmpeg buf 32k */
#define INBUF_SIZE 0x8000
/* my own buf 256k */
#define DMX_BUF_SZ 0x20000

#if USE_OPENGL
#define VDEC_PIXFMT AV_PIX_FMT_RGB32
#endif
#if USE_CLUTTER
#define VDEC_PIXFMT AV_PIX_FMT_BGR24
#endif

#include "video_lib.h"
#include "dmx_hal.h"
#include "glfb_priv.h"
#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_VIDEO, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_VIDEO, this, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_VIDEO, NULL, args)

cVideo *videoDecoder = NULL;
extern cDemux *videoDemux;
extern GLFbPC *glfb_priv;
int system_rev = 0;

extern bool HAL_nodec;

static uint8_t *dmxbuf;
static int bufpos;

static const AVRational aspect_ratios[6] = {
	{  1, 1 },
	{  4, 3 },
	{ 14, 9 },
	{ 16, 9 },
	{ 20, 9 },
	{ -1,-1 }
};

cVideo::cVideo(int, void *, void *, unsigned int)
{
	hal_debug("%s\n", __func__);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif
	if (!HAL_nodec)
		dmxbuf = (uint8_t *)malloc(DMX_BUF_SZ);
	bufpos = 0;
	thread_running = false;
	w_h_changed = false;
	dec_w = dec_h = 0;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;
	pig_x = pig_y = pig_w = pig_h = 0;
	pig_changed = false;
	display_aspect = DISPLAY_AR_16_9;
	display_crop = DISPLAY_AR_MODE_LETTERBOX;
	v_format = VIDEO_FORMAT_MPEG2;
	output_h = 0;
	stillpicture = false;
}

cVideo::~cVideo(void)
{
	Stop();
	/* ouch :-( */
	videoDecoder = NULL;
}

int cVideo::setAspectRatio(int vformat, int cropping)
{
	hal_info("%s(%d, %d)\n", __func__, vformat, cropping);
	if (vformat >= 0)
		display_aspect = (DISPLAY_AR) vformat;
	if (cropping >= 0)
		display_crop = (DISPLAY_AR_MODE) cropping;
	if (display_aspect < DISPLAY_AR_RAW && output_h > 0) /* don't know what to do with this */
		glfb_priv->setOutputFormat(aspect_ratios[display_aspect], output_h, display_crop);
	return 0;
}

int cVideo::getAspectRatio(void)
{
	buf_m.lock();
	int ret = 0;
	int w, h, ar;
	AVRational a;
	if (buf_num == 0)
		goto out;
	a = buffers[buf_out].AR();
	w = buffers[buf_out].width();
	h = buffers[buf_out].height();
	if (a.den == 0 || h == 0)
		goto out;
	ar = w * 100 * a.num / h / a.den;
	if (ar < 100 || ar > 225) /* < 4:3, > 20:9 */
		; /* ret = 0: N/A */
	else if (ar < 140)	/* 4:3 */
		ret = 1;
	else if (ar < 165)	/* 14:9 */
		ret = 2;
	else if (ar < 200)	/* 16:9 */
		ret = 3;
	else
		ret = 4;	/* 20:9 */
 out:
	buf_m.unlock();
	return ret;
}

int cVideo::setCroppingMode(int)
{
	return 0;
}

int cVideo::Start(void *, unsigned short, unsigned short, void *)
{
	hal_debug("%s running %d >\n", __func__, thread_running);
	if (!thread_running && !HAL_nodec)
		OpenThreads::Thread::start();
	hal_debug("%s running %d <\n", __func__, thread_running);
	return 0;
}

int cVideo::Stop(bool)
{
	hal_debug("%s running %d >\n", __func__, thread_running);
	if (thread_running) {
		thread_running = false;
		OpenThreads::Thread::join();
	}
	hal_debug("%s running %d <\n", __func__, thread_running);
	return 0;
}

int cVideo::setBlank(int)
{
	return 1;
}

int cVideo::GetVideoSystem()
{
	int current_video_system = VIDEO_STD_1080I50;

	if(dec_w < 720)
		current_video_system = VIDEO_STD_PAL;
	else if(dec_w > 720 && dec_w <= 1280)
		current_video_system = VIDEO_STD_720P50;

	return current_video_system;
}

int cVideo::SetVideoSystem(int system, bool)
{
	int h;
	switch(system)
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
//	v_std = (VIDEO_STD) system;
	output_h = h;
	if (display_aspect < DISPLAY_AR_RAW && output_h > 0) /* don't know what to do with this */
		glfb_priv->setOutputFormat(aspect_ratios[display_aspect], output_h, display_crop);
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
	bool ret = false;
	hal_info("%s(%s)\n", __func__, fname);
	if (access(fname, R_OK))
		return ret;
	still_m.lock();
	stillpicture = true;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;
	still_m.unlock();

	unsigned int i;
	int stream_id = -1;
	int got_frame = 0;
	int len;
	AVFormatContext *avfc = NULL;
	AVCodecContext *c = NULL;
	AVCodecParameters *p = NULL;
	AVCodec *codec;
	AVFrame *frame, *rgbframe;
	AVPacket avpkt;

	if (avformat_open_input(&avfc, fname, NULL, NULL) < 0) {
		hal_info("%s: Could not open file %s\n", __func__, fname);
		return ret;
	}

	if (avformat_find_stream_info(avfc, NULL) < 0) {
		hal_info("%s: Could not find file info %s\n", __func__, fname);
		goto out_close;
	}
	for (i = 0; i < avfc->nb_streams; i++) {
		if (avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream_id = i;
			break;
		}
	}
	if (stream_id < 0)
		goto out_close;
	p = avfc->streams[stream_id]->codecpar;
	codec = avcodec_find_decoder(p->codec_id);
	c = avcodec_alloc_context3(codec);
	if (avcodec_open2(c, codec, NULL) < 0) {
		hal_info("%s: Could not find/open the codec, id 0x%x\n", __func__, p->codec_id);
		goto out_close;
	}
	frame = av_frame_alloc();
	rgbframe = av_frame_alloc();
	if (!frame || !rgbframe) {
		hal_info("%s: Could not allocate video frame\n", __func__);
		goto out_free;
	}
	av_init_packet(&avpkt);
	if (av_read_frame(avfc, &avpkt) < 0) {
		hal_info("%s: av_read_frame < 0\n", __func__);
		goto out_free;
	}
	len = avcodec_decode_video2(c, frame, &got_frame, &avpkt);
	if (len < 0) {
		hal_info("%s: avcodec_decode_video2 %d\n", __func__, len);
		av_packet_unref(&avpkt);
		goto out_free;
	}
	if (avpkt.size > len)
		hal_info("%s: WARN: pkt->size %d != len %d\n", __func__, avpkt.size, len);
	if (got_frame) {
		unsigned int need = av_image_get_buffer_size(VDEC_PIXFMT, c->width, c->height, 1);
		struct SwsContext *convert = sws_getContext(c->width, c->height, c->pix_fmt,
							    c->width, c->height, VDEC_PIXFMT,
							    SWS_BICUBIC, 0, 0, 0);
		if (!convert)
			hal_info("%s: ERROR setting up SWS context\n", __func__);
		else {
			buf_m.lock();
			SWFramebuffer *f = &buffers[buf_in];
			if (f->size() < need)
				f->resize(need);
			av_image_fill_arrays(rgbframe->data, rgbframe->linesize, &(*f)[0], VDEC_PIXFMT,
					c->width, c->height, 1);
			sws_scale(convert, frame->data, frame->linesize, 0, c->height,
					rgbframe->data, rgbframe->linesize);
			sws_freeContext(convert);
			f->width(c->width);
			f->height(c->height);
			f->pts(AV_NOPTS_VALUE);
			AVRational a = av_guess_sample_aspect_ratio(avfc, avfc->streams[stream_id], frame);
			f->AR(a);
			buf_in++;
			buf_in %= VDEC_MAXBUFS;
			buf_num++;
			if (buf_num > (VDEC_MAXBUFS - 1)) {
				hal_debug("%s: buf_num overflow\n", __func__);
				buf_out++;
				buf_out %= VDEC_MAXBUFS;
				buf_num--;
			}
			buf_m.unlock();
			ret = true;
		}
	}
	av_packet_unref(&avpkt);
 out_free:
	avcodec_close(c);
	av_free(c);
	av_frame_free(&frame);
	av_frame_free(&rgbframe);
 out_close:
	avformat_close_input(&avfc);
	hal_debug("%s(%s) end\n", __func__, fname);
	return ret;
}

void cVideo::StopPicture()
{
	hal_info("%s\n", __func__);
	still_m.lock();
	stillpicture = false;
	still_m.unlock();
}

void cVideo::Standby(unsigned int)
{
}

int cVideo::getBlank(void)
{
	return 0;
}

void cVideo::Pig(int x, int y, int w, int h, int /*osd_w*/, int /*osd_h*/, int /*startx*/, int /*starty*/, int /*endx*/, int /*endy*/)
{
	pig_x = x;
	pig_y = y;
	pig_w = w;
	pig_h = h;
	pig_changed = true;
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	width = dec_w;
	height = dec_h;
	switch (dec_r) {
		case 23://23.976fps
			rate = VIDEO_FRAME_RATE_23_976;
			break;
		case 24:
			rate = VIDEO_FRAME_RATE_24;
			break;
		case 25:
			rate = VIDEO_FRAME_RATE_25;
			break;
		case 29://29,976fps
			rate = VIDEO_FRAME_RATE_29_97;
			break;
		case 30:
			rate = VIDEO_FRAME_RATE_30;
			break;
		case 50:
			rate = VIDEO_FRAME_RATE_50;
			break;
		case 60:
			rate = VIDEO_FRAME_RATE_60;
			break;
		default:
			rate = dec_r;
			break;
	}
}

void cVideo::SetSyncMode(AVSYNC_TYPE)
{
};

int cVideo::SetStreamType(VIDEO_FORMAT v)
{
	v_format = v;
	return 0;
}

cVideo::SWFramebuffer *cVideo::getDecBuf(void)
{
	buf_m.lock();
	if (buf_num == 0) {
		buf_m.unlock();
		return NULL;
	}
	SWFramebuffer *p = &buffers[buf_out];
	buf_out++;
	buf_num--;
	buf_out %= VDEC_MAXBUFS;
	buf_m.unlock();
	return p;
}

static int my_read(void *, uint8_t *buf, int buf_size)
{
	int tmp = 0;
	if (videoDecoder && bufpos < DMX_BUF_SZ - 4096) {
		while (bufpos < buf_size && ++tmp < 20) { /* retry max 20 times */
			int ret = videoDemux->Read(dmxbuf + bufpos, DMX_BUF_SZ - bufpos, 20);
			if (ret > 0)
				bufpos += ret;
		}
	}
	if (bufpos == 0)
		return 0;
	if (bufpos > buf_size) {
		memcpy(buf, dmxbuf, buf_size);
		memmove(dmxbuf, dmxbuf + buf_size, bufpos - buf_size);
		bufpos -= buf_size;
		return buf_size;
	}
	memcpy(buf, dmxbuf, bufpos);
	tmp = bufpos;
	bufpos = 0;
	return tmp;
}

void cVideo::run(void)
{
	hal_info("====================== start decoder thread ================================\n");
	AVCodec *codec;
	AVCodecParameters *p = NULL;
	AVCodecContext *c= NULL;
	AVFormatContext *avfc = NULL;
	AVInputFormat *inp;
	AVFrame *frame, *rgbframe;
	uint8_t *inbuf = (uint8_t *)av_malloc(INBUF_SIZE);
	AVPacket avpkt;
	struct SwsContext *convert = NULL;

	time_t warn_r = 0; /* last read error */
	time_t warn_d = 0; /* last decode error */

	bufpos = 0;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;
	dec_r = 0;

	av_init_packet(&avpkt);
	inp = av_find_input_format("mpegts");
	AVIOContext *pIOCtx = avio_alloc_context(inbuf, INBUF_SIZE, // internal Buffer and its size
			0,		// bWriteable (1=true,0=false)
			NULL,		// user data; will be passed to our callback functions
			my_read,	// read callback
			NULL,		// write callback
			NULL);		// seek callback
	avfc = avformat_alloc_context();
	avfc->pb = pIOCtx;
	avfc->iformat = inp;
	avfc->probesize = 188*5;

	thread_running = true;
	if (avformat_open_input(&avfc, NULL, inp, NULL) < 0) {
		hal_info("%s: Could not open input\n", __func__);
		goto out;
	}
	while (avfc->nb_streams < 1)
	{
		hal_info("%s: nb_streams %d, should be 1 => retry\n", __func__, avfc->nb_streams);
		if (av_read_frame(avfc, &avpkt) < 0)
			hal_info("%s: av_read_frame < 0\n", __func__);
		av_packet_unref(&avpkt);
		if (! thread_running)
			goto out;
	}

	p = avfc->streams[0]->codecpar;
	if (p->codec_type != AVMEDIA_TYPE_VIDEO)
		hal_info("%s: no video codec? 0x%x\n", __func__, p->codec_type);

	codec = avcodec_find_decoder(p->codec_id);
	if (!codec) {
		hal_info("%s: Codec for %s not found\n", __func__, avcodec_get_name(p->codec_id));
		goto out;
	}
	c = avcodec_alloc_context3(codec);
	if (avcodec_open2(c, codec, NULL) < 0) {
		hal_info("%s: Could not open codec\n", __func__);
		goto out;
	}
	frame = av_frame_alloc();
	rgbframe = av_frame_alloc();
	if (!frame || !rgbframe) {
		hal_info("%s: Could not allocate video frame\n", __func__);
		goto out2;
	}
	hal_info("decoding %s\n", avcodec_get_name(c->codec_id));
	while (thread_running) {
		if (av_read_frame(avfc, &avpkt) < 0) {
			if (warn_r - time(NULL) > 4) {
				hal_info("%s: av_read_frame < 0\n", __func__);
				warn_r = time(NULL);
			}
			usleep(10000);
			continue;
		}
		int got_frame = 0;
		int len = avcodec_decode_video2(c, frame, &got_frame, &avpkt);
		if (len < 0) {
			if (warn_d - time(NULL) > 4) {
				hal_info("%s: avcodec_decode_video2 %d\n", __func__, len);
				warn_d = time(NULL);
			}
			av_packet_unref(&avpkt);
			continue;
		}
		if (avpkt.size > len)
			hal_info("%s: WARN: pkt->size %d != len %d\n", __func__, avpkt.size, len);
		still_m.lock();
		if (got_frame && ! stillpicture) {
			unsigned int need = av_image_get_buffer_size(VDEC_PIXFMT, c->width, c->height, 1);
			convert = sws_getCachedContext(convert,
						       c->width, c->height, c->pix_fmt,
						       c->width, c->height, VDEC_PIXFMT,
						       SWS_BICUBIC, 0, 0, 0);
			if (!convert)
				hal_info("%s: ERROR setting up SWS context\n", __func__);
			else {
				buf_m.lock();
				SWFramebuffer *f = &buffers[buf_in];
				if (f->size() < need)
					f->resize(need);
				av_image_fill_arrays(rgbframe->data, rgbframe->linesize, &(*f)[0], VDEC_PIXFMT,
						c->width, c->height, 1);
				sws_scale(convert, frame->data, frame->linesize, 0, c->height,
						rgbframe->data, rgbframe->linesize);
				if (dec_w != c->width || dec_h != c->height) {
					hal_info("%s: pic changed %dx%d -> %dx%d\n", __func__,
							dec_w, dec_h, c->width, c->height);
					dec_w = c->width;
					dec_h = c->height;
					w_h_changed = true;
				}
				f->width(c->width);
				f->height(c->height);
#if (LIBAVUTIL_VERSION_MAJOR < 54)
				int64_t vpts = av_frame_get_best_effort_timestamp(frame);
#else
				int64_t vpts = frame->best_effort_timestamp;
#endif
				/* a/v delay determined experimentally :-) */
#if USE_OPENGL
				if (v_format == VIDEO_FORMAT_MPEG2)
					vpts += 90000*4/10; /* 400ms */
				else
					vpts += 90000*3/10; /* 300ms */
#endif
#if USE_CLUTTER
				/* no idea why there's a difference between OpenGL and clutter rendering... */
				if (v_format == VIDEO_FORMAT_MPEG2)
					vpts += 90000*3/10; /* 300ms */
#endif
				f->pts(vpts);
				AVRational a = av_guess_sample_aspect_ratio(avfc, avfc->streams[0], frame);
				f->AR(a);
				buf_in++;
				buf_in %= VDEC_MAXBUFS;
				buf_num++;
				if (buf_num > (VDEC_MAXBUFS - 1)) {
					hal_debug("%s: buf_num overflow\n", __func__);
					buf_out++;
					buf_out %= VDEC_MAXBUFS;
					buf_num--;
				}
				dec_r = c->time_base.den/(c->time_base.num * c->ticks_per_frame);
				buf_m.unlock();
			}
			hal_debug("%s: time_base: %d/%d, ticks: %d rate: %d pts 0x%" PRIx64 "\n", __func__,
					c->time_base.num, c->time_base.den, c->ticks_per_frame, dec_r,
#if (LIBAVUTIL_VERSION_MAJOR < 54)
					av_frame_get_best_effort_timestamp(frame));
#else
					frame->best_effort_timestamp);
#endif
		} else
			hal_debug("%s: got_frame: %d stillpicture: %d\n", __func__, got_frame, stillpicture);
		still_m.unlock();
		av_packet_unref(&avpkt);
	}
	sws_freeContext(convert);
 out2:
	avcodec_close(c);
	av_free(c);
	av_frame_free(&frame);
	av_frame_free(&rgbframe);
 out:
	avformat_close_input(&avfc);
	av_free(pIOCtx->buffer);
	av_free(pIOCtx);
	/* reset output buffers */
	bufpos = 0;
	still_m.lock();
	if (!stillpicture) {
		buf_num = 0;
		buf_in = 0;
		buf_out = 0;
	}
	still_m.unlock();
	hal_info("======================== end decoder thread ================================\n");
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

bool cVideo::GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	hal_info("%s: data 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, data, xres, yres, get_video, get_osd, scale_to_video);
	SWFramebuffer video;
	std::vector<unsigned char> *osd = NULL;
	std::vector<unsigned char> s_osd; /* scaled OSD */
	int vid_w = 0, vid_h = 0;
	int osd_w = glfb_priv->getOSDWidth();
	int osd_h = glfb_priv->getOSDHeight();
	xres = osd_w;
	yres = osd_h;
	if (get_video) {
		buf_m.lock();
		video = buffers[buf_out];
		buf_m.unlock();
		vid_w = video.width();
		vid_h = video.height();
		if (scale_to_video || !get_osd) {
			xres = vid_w;
			yres = vid_h;
			AVRational a = video.AR();
			/* TODO: this does not consider display_aspect and display_crop */
			if (a.num > 0 && a.den > 0)
				xres = vid_w * a.num / a.den;
		}
	}
	if(video.empty()){
		get_video=false;
		xres = osd_w;
		yres = osd_h;
	}
	if (get_osd)
		osd = glfb_priv->getOSDBuffer();
	unsigned int need = av_image_get_buffer_size(AV_PIX_FMT_RGB32, xres, yres, 1);
	data = (unsigned char *)realloc(data, need); /* will be freed by caller */
	if (data == NULL)	/* out of memory? */
		return false;

	if (get_video) {
#if USE_OPENGL //memcpy dont work with copy BGR24 to RGB32
		if (vid_w != xres || vid_h != yres){ /* scale video into data... */
#endif
			bool ret = swscale(&video[0], data, vid_w, vid_h, xres, yres,VDEC_PIXFMT);
			if(!ret){
				free(data);
				return false;
			}
#if USE_OPENGL //memcpy dont work with copy BGR24 to RGB32
		}else{ /* get_video and no fancy scaling needed */
			memcpy(data, &video[0], xres * yres * sizeof(uint32_t));
		}
#endif
	}

	if (get_osd && (osd_w != xres || osd_h != yres)) {
		/* rescale osd */
		s_osd.resize(need);
		bool ret = swscale(&(*osd)[0], &s_osd[0], osd_w, osd_h, xres, yres,AV_PIX_FMT_RGB32);
		if(!ret){
			free(data);
			return false;
		}
		osd = &s_osd;
	}

	if (get_video && get_osd) {
		/* alpha blend osd onto data (video). TODO: maybe libavcodec can do this? */
		uint32_t *d = (uint32_t *)data;
		uint32_t *pixpos = (uint32_t *)&(*osd)[0];
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
	else if (get_osd) /* only get_osd, data is not yet populated */
		memcpy(data, &(*osd)[0], xres * yres * sizeof(uint32_t));

	return true;
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	buf_m.lock();
	if (buf_num != 0)
		pts = buffers[buf_out].pts();
	buf_m.unlock();
	return pts;
}

void cVideo::SetDemux(cDemux *)
{
	hal_debug("%s: not implemented yet\n", __func__);
}

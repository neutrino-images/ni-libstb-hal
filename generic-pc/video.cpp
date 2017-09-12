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

#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

/* ffmpeg buf 32k */
#define INBUF_SIZE 0x8000
/* my own buf 256k */
#define DMX_BUF_SZ 0x20000

#include "video_lib.h"
#include "dmx_lib.h"
#include "glfb.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info_c(args...) _lt_info(TRIPLE_DEBUG_VIDEO, NULL, args)

cVideo *videoDecoder = NULL;
extern cDemux *videoDemux;
extern GLFramebuffer *glfb;
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
	lt_debug("%s\n", __func__);
	av_register_all();
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
	display_aspect = DISPLAY_AR_16_9;
	display_crop = DISPLAY_AR_MODE_LETTERBOX;
	v_format = VIDEO_FORMAT_MPEG2;
	output_h = 0;
}

cVideo::~cVideo(void)
{
	Stop();
	/* ouch :-( */
	videoDecoder = NULL;
}

int cVideo::setAspectRatio(int vformat, int cropping)
{
	lt_info("%s(%d, %d)\n", __func__, vformat, cropping);
	if (vformat >= 0)
		display_aspect = (DISPLAY_AR) vformat;
	if (cropping >= 0)
		display_crop = (DISPLAY_AR_MODE) cropping;
	if (display_aspect < DISPLAY_AR_RAW && output_h > 0) /* don't know what to do with this */
		glfb->setOutputFormat(aspect_ratios[display_aspect], output_h, display_crop);
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
	lt_debug("%s running %d >\n", __func__, thread_running);
	if (!thread_running && !HAL_nodec)
		Thread::startThread();
	lt_debug("%s running %d <\n", __func__, thread_running);
	return 0;
}

int cVideo::Stop(bool)
{
	lt_debug("%s running %d >\n", __func__, thread_running);
	if (thread_running) {
		thread_running = false;
		Thread::joinThread();
	}
	lt_debug("%s running %d <\n", __func__, thread_running);
	return 0;
}

int cVideo::setBlank(int)
{
	return 1;
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
			lt_info("%s: VIDEO_STD_AUTO not implemented\n", __func__);
			// fallthrough
		case VIDEO_STD_SECAM:
		case VIDEO_STD_PAL:
		case VIDEO_STD_576P:
			h = 576;
			break;
		default:
			lt_info("%s: unhandled value %d\n", __func__, system);
			return 0;
	}
	v_std = (VIDEO_STD) system;
	output_h = h;
	if (display_aspect < DISPLAY_AR_RAW && output_h > 0) /* don't know what to do with this */
		glfb->setOutputFormat(aspect_ratios[display_aspect], output_h, display_crop);
	return 0;
}

int cVideo::GetVideoSystem()
{
	int current_video_system = VIDEO_STD_1080I50;

	if(dec_w < 720)
		current_video_system = VIDEO_STD_PAL;
	else if(dec_w > 720 && dec_w <= 1280)
		current_video_system = VIDEO_STD_720P50;

	return 0;
}

int cVideo::getPlayState(void)
{
	return VIDEO_PLAYING;
}

void cVideo::SetVideoMode(analog_mode_t)
{
}

void cVideo::ShowPicture(const char *fname, const char *)
{
	lt_info("%s(%s)\n", __func__, fname);
	if (access(fname, R_OK))
		return;

	unsigned int i;
	int stream_id = -1;
	int got_frame = 0;
	int len;
	AVFormatContext *avfc = NULL;
	AVCodecContext *c = NULL;
	AVCodec *codec;
	AVFrame *frame, *rgbframe;
	AVPacket avpkt;

	if (avformat_open_input(&avfc, fname, NULL, NULL) < 0) {
		lt_info("%s: Could not open file %s\n", __func__, fname);
		return;
	}

	if (avformat_find_stream_info(avfc, NULL) < 0) {
		lt_info("%s: Could not find file info %s\n", __func__, fname);
		goto out_close;
	}
	for (i = 0; i < avfc->nb_streams; i++) {
		if (avfc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream_id = i;
			break;
		}
	}
	if (stream_id < 0)
		goto out_close;
	c = avfc->streams[stream_id]->codec;
	codec = avcodec_find_decoder(c->codec_id);
	if (!avcodec_open2(c, codec, NULL) < 0) {
		lt_info("%s: Could not find/open the codec, id 0x%x\n", __func__, c->codec_id);
		goto out_close;
	}
	frame = av_frame_alloc();
	rgbframe = av_frame_alloc();
	if (!frame || !rgbframe) {
		lt_info("%s: Could not allocate video frame\n", __func__);
		goto out_free;
	}
	av_init_packet(&avpkt);
	if (av_read_frame(avfc, &avpkt) < 0) {
		lt_info("%s: av_read_frame < 0\n", __func__);
		goto out_free;
	}
	len = avcodec_decode_video2(c, frame, &got_frame, &avpkt);
	if (len < 0) {
		lt_info("%s: avcodec_decode_video2 %d\n", __func__, len);
		av_free_packet(&avpkt);
		goto out_free;
	}
	if (avpkt.size > len)
		lt_info("%s: WARN: pkt->size %d != len %d\n", __func__, avpkt.size, len);
	if (got_frame) {
		unsigned int need = avpicture_get_size(PIX_FMT_RGB32, c->width, c->height);
		struct SwsContext *convert = sws_getContext(c->width, c->height, c->pix_fmt,
							    c->width, c->height, PIX_FMT_RGB32,
							    SWS_BICUBIC, 0, 0, 0);
		if (!convert)
			lt_info("%s: ERROR setting up SWS context\n", __func__);
		else {
			buf_m.lock();
			SWFramebuffer *f = &buffers[buf_in];
			if (f->size() < need)
				f->resize(need);
			avpicture_fill((AVPicture *)rgbframe, &(*f)[0], PIX_FMT_RGB32,
					c->width, c->height);
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
				lt_debug("%s: buf_num overflow\n", __func__);
				buf_out++;
				buf_out %= VDEC_MAXBUFS;
				buf_num--;
			}
			buf_m.unlock();
		}
	}
	av_free_packet(&avpkt);
 out_free:
	avcodec_close(c);
	av_frame_free(&frame);
	av_frame_free(&rgbframe);
 out_close:
	avformat_close_input(&avfc);
	lt_debug("%s(%s) end\n", __func__, fname);
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

void cVideo::Pig(int x, int y, int w, int h, int /*osd_w*/, int /*osd_h*/, int /*startx*/, int /*starty*/, int /*endx*/, int /*endy*/)
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
	lt_info("====================== start decoder thread ================================\n");
	AVCodec *codec;
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
		lt_info("%s: Could not open input\n", __func__);
		goto out;
	}
	while (avfc->nb_streams < 1)
	{
		lt_info("%s: nb_streams %d, should be 1 => retry\n", __func__, avfc->nb_streams);
		if (av_read_frame(avfc, &avpkt) < 0)
			lt_info("%s: av_read_frame < 0\n", __func__);
		av_free_packet(&avpkt);
		if (! thread_running)
			goto out;
	}

	if (avfc->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
		lt_info("%s: no video codec? 0x%x\n", __func__, avfc->streams[0]->codec->codec_type);

	c = avfc->streams[0]->codec;
	codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
		lt_info("%s: Codec for %s not found\n", __func__, avcodec_get_name(c->codec_id));
		goto out;
	}
	if (avcodec_open2(c, codec, NULL) < 0) {
		lt_info("%s: Could not open codec\n", __func__);
		goto out;
	}
	frame = av_frame_alloc();
	rgbframe = av_frame_alloc();
	if (!frame || !rgbframe) {
		lt_info("%s: Could not allocate video frame\n", __func__);
		goto out2;
	}
	lt_info("decoding %s\n", avcodec_get_name(c->codec_id));
	while (thread_running) {
		if (av_read_frame(avfc, &avpkt) < 0) {
			if (warn_r - time(NULL) > 4) {
				lt_info("%s: av_read_frame < 0\n", __func__);
				warn_r = time(NULL);
			}
			usleep(10000);
			continue;
		}
		int got_frame = 0;
		int len = avcodec_decode_video2(c, frame, &got_frame, &avpkt);
		if (len < 0) {
			if (warn_d - time(NULL) > 4) {
				lt_info("%s: avcodec_decode_video2 %d\n", __func__, len);
				warn_d = time(NULL);
			}
			av_free_packet(&avpkt);
			continue;
		}
		if (avpkt.size > len)
			lt_info("%s: WARN: pkt->size %d != len %d\n", __func__, avpkt.size, len);
		if (got_frame) {
			unsigned int need = avpicture_get_size(PIX_FMT_RGB32, c->width, c->height);
			convert = sws_getCachedContext(convert,
						       c->width, c->height, c->pix_fmt,
						       c->width, c->height, PIX_FMT_RGB32,
						       SWS_BICUBIC, 0, 0, 0);
			if (!convert)
				lt_info("%s: ERROR setting up SWS context\n", __func__);
			else {
				buf_m.lock();
				SWFramebuffer *f = &buffers[buf_in];
				if (f->size() < need)
					f->resize(need);
				avpicture_fill((AVPicture *)rgbframe, &(*f)[0], PIX_FMT_RGB32,
						c->width, c->height);
				sws_scale(convert, frame->data, frame->linesize, 0, c->height,
						rgbframe->data, rgbframe->linesize);
				if (dec_w != c->width || dec_h != c->height) {
					lt_info("%s: pic changed %dx%d -> %dx%d\n", __func__,
							dec_w, dec_h, c->width, c->height);
					dec_w = c->width;
					dec_h = c->height;
					w_h_changed = true;
				}
				f->width(c->width);
				f->height(c->height);
				int64_t vpts = av_frame_get_best_effort_timestamp(frame);
				if (v_format == VIDEO_FORMAT_MPEG2)
					vpts += 90000*3/10; /* 300ms */
				f->pts(vpts);
				AVRational a = av_guess_sample_aspect_ratio(avfc, avfc->streams[0], frame);
				f->AR(a);
				buf_in++;
				buf_in %= VDEC_MAXBUFS;
				buf_num++;
				if (buf_num > (VDEC_MAXBUFS - 1)) {
					lt_debug("%s: buf_num overflow\n", __func__);
					buf_out++;
					buf_out %= VDEC_MAXBUFS;
					buf_num--;
				}
				dec_r = c->time_base.den/(c->time_base.num * c->ticks_per_frame);
				buf_m.unlock();
			}
			lt_debug("%s: time_base: %d/%d, ticks: %d rate: %d pts 0x%" PRIx64 "\n", __func__,
					c->time_base.num, c->time_base.den, c->ticks_per_frame, dec_r,
					av_frame_get_best_effort_timestamp(frame));
		}
		av_free_packet(&avpkt);
	}
	sws_freeContext(convert);
 out2:
	avcodec_close(c);
	av_frame_free(&frame);
	av_frame_free(&rgbframe);
 out:
	avformat_close_input(&avfc);
	av_free(pIOCtx->buffer);
	av_free(pIOCtx);
	/* reset output buffers */
	bufpos = 0;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;
	lt_info("======================== end decoder thread ================================\n");
}

static bool swscale(unsigned char *src, unsigned char *dst, int sw, int sh, int dw, int dh)
{
	bool ret = false;
	struct SwsContext *scale = NULL;
	AVFrame *sframe, *dframe;
	scale = sws_getCachedContext(scale, sw, sh, PIX_FMT_RGB32, dw, dh, PIX_FMT_RGB32, SWS_BICUBIC, 0, 0, 0);
	if (!scale) {
		lt_info_c("%s: ERROR setting up SWS context\n", __func__);
		return false;
	}
	sframe = av_frame_alloc();
	dframe = av_frame_alloc();
	if (!sframe || !dframe) {
		lt_info_c("%s: could not alloc sframe (%p) or dframe (%p)\n", __func__, sframe, dframe);
		goto out;
	}
	avpicture_fill((AVPicture *)sframe, &(src[0]), PIX_FMT_RGB32, sw, sh);
	avpicture_fill((AVPicture *)dframe, &(dst[0]), PIX_FMT_RGB32, dw, dh);
	sws_scale(scale, sframe->data, sframe->linesize, 0, sh, dframe->data, dframe->linesize);
 out:
	av_frame_free(&sframe);
	av_frame_free(&dframe);
	sws_freeContext(scale);
	return ret;
}

bool cVideo::GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	lt_info("%s: data 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, data, xres, yres, get_video, get_osd, scale_to_video);
	SWFramebuffer video;
	std::vector<unsigned char> *osd = NULL;
	std::vector<unsigned char> s_osd; /* scaled OSD */
	int vid_w = 0, vid_h = 0;
	int osd_w = glfb->getOSDWidth();
	int osd_h = glfb->getOSDHeight();
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
	if (get_osd)
		osd = glfb->getOSDBuffer();
	unsigned int need = avpicture_get_size(PIX_FMT_RGB32, xres, yres);
	data = (unsigned char *)realloc(data, need); /* will be freed by caller */
	if (data == NULL)	/* out of memory? */
		return false;

	if (get_video) {
		if (vid_w != xres || vid_h != yres) /* scale video into data... */
			swscale(&video[0], data, vid_w, vid_h, xres, yres);
		else /* get_video and no fancy scaling needed */
			memcpy(data, &video[0], xres * yres * sizeof(uint32_t));
	}

	if (get_osd && (osd_w != xres || osd_h != yres)) {
		/* rescale osd */
		s_osd.resize(need);
		swscale(&(*osd)[0], &s_osd[0], osd_w, osd_h, xres, yres);
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
	lt_debug("%s: not implemented yet\n", __func__);
}

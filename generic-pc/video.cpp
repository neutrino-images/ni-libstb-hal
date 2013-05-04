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
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)

cVideo *videoDecoder = NULL;
extern cDemux *videoDemux;
int system_rev = 0;

static uint8_t *dmxbuf;
static int bufpos;

cVideo::cVideo(int, void *, void *)
{
	lt_debug("%s\n", __func__);
	av_register_all();
	dmxbuf = (uint8_t *)malloc(DMX_BUF_SZ);
	bufpos = 0;
	thread_running = false;
	w_h_changed = false;
	dec_w = dec_h = 0;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;
	firstpts = AV_NOPTS_VALUE;
}

cVideo::~cVideo(void)
{
	Stop();
	/* ouch :-( */
	videoDecoder = NULL;
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
	lt_info("%s running %d >\n", __func__, thread_running);
	if (!thread_running)
		OpenThreads::Thread::start();
	lt_info("%s running %d <\n", __func__, thread_running);
	return 0;
}

int cVideo::Stop(bool)
{
	lt_info("%s running %d >\n", __func__, thread_running);
	if (thread_running) {
		thread_running = false;
		OpenThreads::Thread::join();
	}
	lt_info("%s running %d <\n", __func__, thread_running);
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
	width = dec_w;
	height = dec_h;
	rate = dec_r;
}

void cVideo::SetSyncMode(AVSYNC_TYPE)
{
};

int cVideo::SetStreamType(VIDEO_FORMAT)
{
	return 0;
}

cVideo::SWFramebuffer *cVideo::getDecBuf(void)
{
	if (buf_num == 0)
		return NULL;
	SWFramebuffer *p = &buffers[buf_out];
	buf_out++;
	buf_num--;
	buf_out %= VDEC_MAXBUFS;
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

	time_t warn_r = 0; /* last read error */
	time_t warn_d = 0; /* last decode error */

	bufpos = 0;
	buf_num = 0;
	buf_in = 0;
	buf_out = 0;

	firstpts = AV_NOPTS_VALUE;
	framecount = 0;
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
	lt_info("%s: nb_streams %d\n", __func__, avfc->nb_streams);

	if (avfc->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
		lt_info("%s: no video codec? 0x%x\n", __func__, avfc->streams[0]->codec->codec_type);

	c = avfc->streams[0]->codec;
	codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
		lt_info("%s: Codec not found\n", __func__);
		goto out;
	}
	if (avcodec_open2(c, codec, NULL) < 0) {
		lt_info("%s: Could not open codec\n", __func__);
		goto out;
	}
	frame = avcodec_alloc_frame();
	rgbframe = avcodec_alloc_frame();
	if (!frame || !rgbframe) {
		lt_info("%s: Could not allocate video frame\n", __func__);
		goto out2;
	}
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
			struct SwsContext *convert = sws_getContext(c->width, c->height, c->pix_fmt,
								    c->width, c->height, PIX_FMT_RGB32,
								    SWS_BICUBIC, 0, 0, 0);
			if (!convert)
				lt_info("%s: ERROR setting up SWS context\n", __func__);
			else {
				SWFramebuffer *f = &buffers[buf_in];
				if (f->size() < need)
					f->resize(need);
				avpicture_fill((AVPicture *)rgbframe, &(*f)[0], PIX_FMT_RGB32,
						c->width, c->height);
				sws_scale(convert, frame->data, frame->linesize, 0, c->height,
						rgbframe->data, rgbframe->linesize);
				sws_freeContext(convert);
				// TODO: locking needed!
				if (dec_w != c->width || dec_h != c->height) {
					lt_info("%s: pic changed %dx%d -> %dx%d\n", __func__,
							dec_w, dec_h, c->width, c->height);
					dec_w = c->width;
					dec_h = c->height;
					w_h_changed = true;
				}
				f->width(c->width);
				f->height(c->height);
				f->pts(av_frame_get_best_effort_timestamp(frame));
				buf_in++;
				buf_in %= VDEC_MAXBUFS;
				buf_num++;
				if (buf_num > (VDEC_MAXBUFS - 1)) {
					lt_info("%s: buf_num overflow\n", __func__);
					buf_out++;
					buf_out %= VDEC_MAXBUFS;
					buf_num--;
				}
				if (firstpts == AV_NOPTS_VALUE && f->pts() != AV_NOPTS_VALUE)
					firstpts = f->pts();
			}
			dec_r = c->time_base.den/(c->time_base.num * c->ticks_per_frame);
			framecount++;
			lt_debug("%s: time_base: %d/%d, ticks: %d rate: %d pts 0x%" PRIx64 "\n", __func__,
					c->time_base.num, c->time_base.den, c->ticks_per_frame, dec_r,
					av_frame_get_best_effort_timestamp(frame));
		}
		av_free_packet(&avpkt);
	}
 out2:
	avcodec_close(c);
	avcodec_free_frame(&frame);
	avcodec_free_frame(&rgbframe);
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

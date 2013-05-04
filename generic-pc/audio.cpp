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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * cAudio implementation with decoder.
 * uses libao  <http://www.xiph.org/ao/> for output
 *      ffmpeg <http://ffmpeg.org> for demuxing / decoding
 */

#include <cstdio>
#include <cstdlib>

#include "audio_lib.h"
#include "dmx_lib.h"
#include "lt_debug.h"

#define lt_debug(args...) _lt_debug(HAL_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(HAL_DEBUG_AUDIO, this, args)

#include <OpenThreads/Thread>

extern "C" {
#include <libavformat/avformat.h>
#include <ao/ao.h>
}
/* ffmpeg buf 2k */
#define INBUF_SIZE 0x0800
/* my own buf 16k */
#define DMX_BUF_SZ 0x4000

cAudio * audioDecoder = NULL;
extern cDemux *audioDemux;
static uint8_t *dmxbuf = NULL;
static int bufpos;

static cAudio *gThiz = NULL;

cAudio::cAudio(void *, void *, void *)
{
	thread_started = false;
	dmxbuf = (uint8_t *)malloc(DMX_BUF_SZ);
	bufpos = 0;
	gThiz = this;
	ao_initialize();
}

cAudio::~cAudio(void)
{
	closeDevice();
	free(dmxbuf);
	ao_shutdown();
}

void cAudio::openDevice(void)
{
	lt_debug("%s\n", __func__);
}

void cAudio::closeDevice(void)
{
	lt_debug("%s\n", __func__);
}

int cAudio::do_mute(bool enable, bool remember)
{
	lt_debug("%s(%d, %d)\n", __func__, enable, remember);
	return 0;
}

int cAudio::setVolume(unsigned int left, unsigned int right)
{
	lt_debug("%s(%d, %d)\n", __func__, left, right);
	return 0;
}

int cAudio::Start(void)
{
	lt_info("%s >\n", __func__);
	OpenThreads::Thread::start();
	lt_info("%s <\n", __func__);
	return 0;
}

int cAudio::Stop(void)
{
	lt_info("%s >\n", __func__);
	if (thread_started)
	{
		thread_started = false;
		OpenThreads::Thread::join();
	}
	lt_info("%s <\n", __func__);
	return 0;
}

bool cAudio::Pause(bool /*Pcm*/)
{
	return true;
};

void cAudio::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __func__, Mode);
};

void cAudio::SetStreamType(AUDIO_FORMAT type)
{
	lt_debug("%s %d\n", __func__, type);
};

int cAudio::setChannel(int /*channel*/)
{
	return 0;
};

int cAudio::PrepareClipPlay(int ch, int srate, int bits, int little_endian)
{
	lt_debug("%s ch %d srate %d bits %d le %d\n", __func__, ch, srate, bits, little_endian);
	return 0;
};

int cAudio::WriteClip(unsigned char * /*buffer*/, int /*size*/)
{
	lt_debug("cAudio::%s\n", __func__);
	return 0;
};

int cAudio::StopClip()
{
	lt_debug("%s\n", __func__);
	return 0;
};

void cAudio::getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode)
{
	lt_debug("%s\n", __func__);
	type = 0;
	layer = 0;
	freq = 0;
	bitrate = 0;
	mode = 0;
};

void cAudio::SetSRS(int /*iq_enable*/, int /*nmgr_enable*/, int /*iq_mode*/, int /*iq_level*/)
{
	lt_debug("%s\n", __func__);
};

void cAudio::SetHdmiDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::SetSpdifDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::ScheduleMute(bool On)
{
	lt_debug("%s %d\n", __func__, On);
};

void cAudio::EnableAnalogOut(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

void cAudio::setBypassMode(bool disable)
{
	lt_debug("%s %d\n", __func__, disable);
}

static int _my_read(void *, uint8_t *buf, int buf_size)
{
	return gThiz->my_read(buf, buf_size);
}

int cAudio::my_read(uint8_t *buf, int buf_size)
{
	int tmp = 0;
	if (audioDecoder && bufpos < DMX_BUF_SZ - 4096) {
		while (bufpos < buf_size && ++tmp < 20) { /* retry max 20 times */
			int ret = audioDemux->Read(dmxbuf + bufpos, DMX_BUF_SZ - bufpos, 10);
			if (ret > 0)
				bufpos += ret;
			if (! thread_started)
				break;
		}
	}
	if (bufpos == 0)
		return 0;
	//lt_info("%s buf_size %d bufpos %d th %d tmp %d\n", __func__, buf_size, bufpos, thread_started, tmp);
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

void cAudio::run()
{
	lt_info("====================== start decoder thread ================================\n");
	/* libavcodec & friends */
	av_register_all();

	AVCodec *codec;
	AVCodecContext *c= NULL;
	AVFormatContext *avfc = NULL;
	AVInputFormat *inp;
	AVFrame *frame;
	uint8_t *inbuf = (uint8_t *)av_malloc(INBUF_SIZE);
	AVPacket avpkt;
	int ret, driver;
	/* libao */
	ao_info *ai;
	ao_device *adevice;
	ao_sample_format sformat;
	av_init_packet(&avpkt);
	inp = av_find_input_format("mpegts");
	AVIOContext *pIOCtx = avio_alloc_context(inbuf, INBUF_SIZE, // internal Buffer and its size
			0,		// bWriteable (1=true,0=false)
			NULL,		// user data; will be passed to our callback functions
			_my_read,	// read callback
			NULL,		// write callback
			NULL);		// seek callback
	avfc = avformat_alloc_context();
	avfc->pb = pIOCtx;
	avfc->iformat = inp;
	avfc->probesize = 188*5;
	thread_started = true;

	if (avformat_open_input(&avfc, NULL, inp, NULL) < 0) {
		lt_info("%s: avformat_open_input() failed.\n", __func__);
		goto out;
	}
	ret = avformat_find_stream_info(avfc, NULL);
	lt_debug("%s: avformat_find_stream_info: %d\n", __func__, ret);
	if (avfc->nb_streams != 1)
	{
		lt_info("%s: nb_streams: %d, should be 1!\n", __func__, avfc->nb_streams);
		goto out;
	}
	if (avfc->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
		lt_info("%s: stream 0 no audio codec? 0x%x\n", __func__, avfc->streams[0]->codec->codec_type);

	c = avfc->streams[0]->codec;
	codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
		lt_info("%s: Codec not found\n", __func__);
		goto out;
	}
	if (avcodec_open2(c, codec, NULL) < 0) {
		lt_info("%s: avcodec_open2() failed\n", __func__);
		goto out;
	}
	frame = avcodec_alloc_frame();
	if (!frame) {
		lt_info("%s: avcodec_alloc_frame failed\n", __func__);
		goto out2;
	}
	driver = ao_default_driver_id();
	sformat.bits = 16;
	sformat.channels = c->channels;
	sformat.rate = c->sample_rate;
	sformat.byte_format = AO_FMT_NATIVE;
	sformat.matrix = 0;
	adevice = ao_open_live(driver, &sformat, NULL);
	ai = ao_driver_info(driver);
	lt_info("libao driver: %d name '%s' short '%s' author '%s'\n",
			driver, ai->name, ai->short_name, ai->author);
#if 0
	lt_info(" driver options:");
	for (int i = 0; i < ai->option_count; ++i)
		fprintf(stderr, " %s", ai->options[i]);
	fprintf(stderr, "\n");
#endif
	lt_info("codec params: sample_fmt %d sample_rate %d channels %d\n",
			c->sample_fmt, c->sample_rate, c->channels);
	while (thread_started) {
		int gotframe = 0;
		if (av_read_frame(avfc, &avpkt) < 0)
			break;
		avcodec_decode_audio4(c, frame, &gotframe, &avpkt);
		if (gotframe && thread_started) {
			int64_t pts = av_frame_get_best_effort_timestamp(frame);
			lt_debug("%s: pts 0x%" PRIx64 " %" PRId64 " %3f\n", __func__, pts, pts, pts/90000.0);
			curr_pts = pts;
			ao_play(adevice, (char*)frame->extended_data[0], frame->linesize[0]);
		}
		av_free_packet(&avpkt);
	}
	ao_close(adevice); /* can take long :-( */
	avcodec_free_frame(&frame);
 out2:
	avcodec_close(c);
 out:
	avformat_close_input(&avfc);
	av_free(pIOCtx->buffer);
	av_free(pIOCtx);
	lt_info("======================== end decoder thread ================================\n");
}

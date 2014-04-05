/*
 * Container handling for all stream's handled by ffmpeg
 * konfetti 2010; based on code from crow
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <pthread.h>
#include <sys/prctl.h>

#include "common.h"
#include "misc.h"
#include "debug.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define FFMPEG_DEBUG

#ifdef FFMPEG_DEBUG

static short debug_level = 0;

#define ffmpeg_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, FILENAME, __FUNCTION__, ## x); } while (0)
#else
#define ffmpeg_printf(level, fmt, x...)
#endif

#ifndef FFMPEG_SILENT
#define ffmpeg_err(fmt, x...) do { printf("[%s:%s] " fmt, FILENAME, __FUNCTION__, ## x); } while (0)
#else
#define ffmpeg_err(fmt, x...)
#endif

/* Error Constants */
#define cERR_CONTAINER_FFMPEG_NO_ERROR        0
#define cERR_CONTAINER_FFMPEG_INIT           -1
#define cERR_CONTAINER_FFMPEG_NOT_SUPPORTED  -2
#define cERR_CONTAINER_FFMPEG_INVALID_FILE   -3
#define cERR_CONTAINER_FFMPEG_RUNNING        -4
#define cERR_CONTAINER_FFMPEG_NOMEM          -5
#define cERR_CONTAINER_FFMPEG_OPEN           -6
#define cERR_CONTAINER_FFMPEG_STREAM         -7
#define cERR_CONTAINER_FFMPEG_NULL           -8
#define cERR_CONTAINER_FFMPEG_ERR            -9
#define cERR_CONTAINER_FFMPEG_END_OF_FILE    -10

static const char *FILENAME = "container_ffmpeg.c";

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Variables                     */
/* ***************************** */


static pthread_t PlayThread;
static int hasPlayThreadStarted = 0;
static AVFormatContext *avContext = NULL;
static unsigned char isContainerRunning = 0;
static float seek_sec_abs = -1.0, seek_sec_rel = 0.0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

static char *Codec2Encoding(AVCodecContext * codec, int *version)
{
    fprintf(stderr, "Codec ID: %ld (%.8lx)\n", (long) codec->codec_id, (long) codec->codec_id);
    switch (codec->codec_id) {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
	return "V_MPEG1";
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
	return "V_H263";
    case AV_CODEC_ID_FLV1:
	return "V_FLV";
    case AV_CODEC_ID_VP5:
    case AV_CODEC_ID_VP6:
    case AV_CODEC_ID_VP6F:
	return "V_VP6";
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
	return "V_RMV";
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_MSMPEG4V1:
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
	return "V_MSCOMP";
    case AV_CODEC_ID_WMV1:
	*version = 1;
	return "V_WMV";
    case AV_CODEC_ID_WMV2:
	*version = 2;
	return "V_WMV";
    case AV_CODEC_ID_WMV3:
	*version = 3;
	return "V_WMV";
    case AV_CODEC_ID_VC1:
	return "V_VC1";
    case AV_CODEC_ID_H264:
	return "V_MPEG4/ISO/AVC";
    case AV_CODEC_ID_AVS:
	return "V_AVS";
    case AV_CODEC_ID_MP2:
	return "A_MPEG/L3";
    case AV_CODEC_ID_MP3:
	return "A_MP3";
    case AV_CODEC_ID_AC3:
	return "A_AC3";
    case AV_CODEC_ID_EAC3:
	return "A_EAC3";
    case AV_CODEC_ID_DTS:
	return "A_DTS";
#if 0
    case AV_CODEC_ID_AAC:
	return "A_AAC";
    case AV_CODEC_ID_WMAV1:
    case AV_CODEC_ID_WMAV2:
    case AV_CODEC_ID_WMAPRO:
	return "A_WMA";
    case AV_CODEC_ID_MLP:
	return "A_MLP";
    case AV_CODEC_ID_RA_144:
	return "A_RMA";
    case AV_CODEC_ID_RA_288:
	return "A_RMA";
    case AV_CODEC_ID_VORBIS:
	return "A_VORBIS";
    case AV_CODEC_ID_FLAC:
	return return "A_FLAC";
    case AV_CODEC_ID_PCM_S16LE:
	return "A_PCM";
#endif
/* subtitle */
    case AV_CODEC_ID_SSA:
	return "S_TEXT/ASS";	/* Hellmaster1024: seems to be ASS instead of SSA */
    case AV_CODEC_ID_TEXT:	/* Hellmaster1024: i dont have most of this, but lets hope it is normal text :-) */
    case AV_CODEC_ID_DVD_SUBTITLE:
    case AV_CODEC_ID_DVB_SUBTITLE:
    case AV_CODEC_ID_XSUB:
    case AV_CODEC_ID_MOV_TEXT:
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_DVB_TELETEXT:
    case AV_CODEC_ID_SRT:
	return "S_TEXT/SRT";	/* fixme */
    default:
	// Default to injected-pcm for unhandled audio types.
	if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
	    return "A_IPCM";
	ffmpeg_err("Codec ID %ld (%.8lx) not found\n", (long) codec->codec_id, (long) codec->codec_id);
    }
    return NULL;
}

long long int calcPts(AVStream * stream, int64_t pts)
{
    if (!stream) {
	ffmpeg_err("stream / packet null\n");
	return INVALID_PTS_VALUE;
    }

    if (pts == AV_NOPTS_VALUE)
	pts = INVALID_PTS_VALUE;
    else if (avContext->start_time == AV_NOPTS_VALUE)
	pts = 90000.0 * (double) pts * av_q2d(stream->time_base);
    else
	pts = 90000.0 * (double) pts * av_q2d(stream->time_base) - 90000.0 * avContext->start_time / AV_TIME_BASE;

    if (pts & 0x8000000000000000ull)
	pts = INVALID_PTS_VALUE;

    return pts;
}

/* **************************** */
/* Worker Thread                */
/* **************************** */

// from neutrino-mp/lib/libdvbsubtitle/dvbsub.cpp
extern void dvbsub_write(AVSubtitle *, int64_t);
extern void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid);
extern void dvbsub_ass_clear(void);
// from neutrino-mp/lib/lib/libtuxtxt/tuxtxt_common.h
extern void teletext_write(int pid, uint8_t *data, int size);

static void FFMPEGThread(Context_t * context)
{
    char threadname[17];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[16] = 0;
    prctl(PR_SET_NAME, (unsigned long) &threadname);

    hasPlayThreadStarted = 1;

    int64_t currentVideoPts = -1, currentAudioPts = -1, showtime = 0, bofcount = 0;
    AudioVideoOut_t avOut;

    SwrContext *swr = NULL;
    AVFrame *decoded_frame = NULL;
    int out_sample_rate = 44100;
    int out_channels = 2;
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    int restart_audio_resampling = 0;

    ffmpeg_printf(10, "\n");

    while (context->playback->isCreationPhase) {
	ffmpeg_err("Thread waiting for end of init phase...\n");
	usleep(1000);
    }
    ffmpeg_printf(10, "Running!\n");

    while (context && context->playback && context->playback->isPlaying && !context->playback->abortRequested) {

	//IF MOVIE IS PAUSED, WAIT
	if (context->playback->isPaused) {
	    ffmpeg_printf(20, "paused\n");

	    usleep(100000);
	    continue;
	}

	int seek_target_flag = 0;
	int64_t seek_target = INT64_MIN;

	if (seek_sec_rel != 0.0) {
	    if (avContext->iformat->flags & AVFMT_TS_DISCONT) {
		float br = (avContext->bit_rate) ? avContext->bit_rate / 8.0 : 180000.0;
		seek_target_flag = AVSEEK_FLAG_BYTE;
		seek_target = avio_tell(avContext->pb) + seek_sec_rel * br;
	    } else {
		seek_target = ((((currentVideoPts > 0) ? currentVideoPts : currentAudioPts) / 90000.0) + seek_sec_rel) * AV_TIME_BASE;
	    }
	    seek_sec_rel = 0.0;
	} else if (seek_sec_abs >= 0.0) {
	    if (avContext->iformat->flags & AVFMT_TS_DISCONT) {
		float br = (avContext->bit_rate) ? avContext->bit_rate / 8.0 : 180000.0;
		seek_target_flag = AVSEEK_FLAG_BYTE;
		seek_target = seek_sec_abs * br;
	    } else {
		seek_target = seek_sec_abs * AV_TIME_BASE;
	    }
	    seek_sec_abs = -1.0;
	} else if (context->playback->BackWard && av_gettime() >= showtime) {
	    context->output->Command(context, OUTPUT_CLEAR, "video");

	    if (bofcount == 1) {
		showtime = av_gettime();
		usleep(100000);
		continue;
	    }

	    if (avContext->iformat->flags & AVFMT_TS_DISCONT) {
		off_t pos = avio_tell(avContext->pb);

		if (pos > 0) {
		    float br;
		    if (avContext->bit_rate)
			br = avContext->bit_rate / 8.0;
		    else
			br = 180000.0;
		    seek_target = pos + context->playback->Speed * 8 * br;
		    seek_target_flag = AVSEEK_FLAG_BYTE;
		}
	    } else {
		seek_target = ((((currentVideoPts > 0) ? currentVideoPts : currentAudioPts) / 90000.0) + context->playback->Speed * 8) * AV_TIME_BASE;;
	    }
	    showtime = av_gettime() + 300000;	//jump back every 300ms
	} else {
	    bofcount = 0;
	}

	if (seek_target > INT64_MIN) {
	    int res;
	    if (seek_target < 0)
		seek_target = 0;
	    res = avformat_seek_file(avContext, -1, INT64_MIN, seek_target, INT64_MAX, seek_target_flag);

	    if (res < 0 && context->playback->BackWard)
		bofcount = 1;
	    seek_target = INT64_MIN;
	    restart_audio_resampling = 1;

	    // flush streams
	    unsigned int i;
	    for (i = 0; i < avContext->nb_streams; i++)
		if (avContext->streams[i]->codec && avContext->streams[i]->codec->codec)
		    avcodec_flush_buffers(avContext->streams[i]->codec);
	}

	AVPacket packet;
	av_init_packet(&packet);

	int av_res = av_read_frame(avContext, &packet);
	if (av_res == AVERROR(EAGAIN)) {
	    av_free_packet(&packet);
	    continue;
	}
	if (av_res) {		// av_read_frame failed
	    ffmpeg_err("no data ->end of file reached ?\n");
	    av_free_packet(&packet);
	    break;		// while
	}
	long long int pts;
	uint8_t *packet_data = packet.data;
	int packet_size = packet.size;
	Track_t *videoTrack = NULL;
	Track_t *audioTrack = NULL;
	Track_t *subtitleTrack = NULL;
	Track_t *teletextTrack = NULL;

	context->playback->readCount += packet_size;

	int pid = avContext->streams[packet.stream_index]->id;

	if (context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack) < 0)
	    ffmpeg_err("error getting video track\n");

	if (context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack) < 0)
	    ffmpeg_err("error getting audio track\n");

	if (context->manager->subtitle->Command(context, MANAGER_GET_TRACK, &subtitleTrack) < 0)
	    ffmpeg_err("error getting subtitle track\n");

	if (context->manager->teletext->Command(context, MANAGER_GET_TRACK, &teletextTrack) < 0)
	    ffmpeg_err("error getting teletext track\n");

	ffmpeg_printf(200, "packet_size %d - index %d\n", packet_size, pid);

	if (videoTrack && (videoTrack->Id == pid)) {
	    currentVideoPts = videoTrack->pts = pts = calcPts(videoTrack->stream, packet.pts);

	    ffmpeg_printf(200, "VideoTrack index = %d %lld\n", pid, currentVideoPts);

	    avOut.data = packet_data;
	    avOut.len = packet_size;
	    avOut.pts = pts;
	    avOut.frameRate = videoTrack->frame_rate;
	    avOut.timeScale = videoTrack->TimeScale;
	    avOut.width = videoTrack->width;
	    avOut.height = videoTrack->height;
	    avOut.type = "video";
	    avOut.stream = videoTrack->stream;
	    avOut.avfc = avContext;

	    if (context->output->video->Write(context, &avOut) < 0) {
		ffmpeg_err("writing data to video device failed\n");
	    }
	} else if (audioTrack && (audioTrack->Id == pid)) {
	    if (!context->playback->BackWard) {
		currentAudioPts = audioTrack->pts = pts = calcPts(audioTrack->stream, packet.pts);

		ffmpeg_printf(200, "AudioTrack index = %d\n", pid);
		if (audioTrack->inject_raw_pcm == 1) {
		    ffmpeg_printf(200, "write audio raw pcm\n");

		    avOut.uNoOfChannels = ((AVStream *) audioTrack->stream)->codec->channels;
		    avOut.uSampleRate = ((AVStream *) audioTrack->stream)->codec->sample_rate;
		    avOut.uBitsPerSample = 16;
		    avOut.bLittleEndian = 1;

		    avOut.data = packet_data;
		    avOut.len = packet_size;
		    avOut.pts = pts;
		    avOut.frameRate = 0;
		    avOut.timeScale = 0;
		    avOut.width = 0;
		    avOut.height = 0;
		    avOut.type = "audio";
		    avOut.stream = audioTrack->stream;
		    avOut.avfc = avContext;

		    if (context->output->audio->Write(context, &avOut) < 0)
			ffmpeg_err("(raw pcm) writing data to audio device failed\n");
		} else if (audioTrack->inject_as_pcm == 1) {
		    AVCodecContext *c = ((AVStream *) (audioTrack->stream))->codec;

		    if (restart_audio_resampling) {
			restart_audio_resampling = 0;
			if (swr) {
			    swr_free(&swr);
			    swr = NULL;
			}
			if (decoded_frame) {
			    av_frame_free(&decoded_frame);
			    decoded_frame = NULL;
			}
			context->output->Command(context, OUTPUT_CLEAR, NULL);
			context->output->Command(context, OUTPUT_PLAY, NULL);

			AVCodec *codec = avcodec_find_decoder(c->codec_id);

			if (!codec || avcodec_open2(c, codec, NULL))
			    fprintf(stderr, "%s %d: avcodec_open2 failed\n", __func__, __LINE__);
		    }

		    while (packet_size > 0) {
			int got_frame = 0;
			if (!decoded_frame) {
			    if (!(decoded_frame = av_frame_alloc())) {
				fprintf(stderr, "out of memory\n");
				exit(1);
			    }
			} else
			    av_frame_unref(decoded_frame);

			int len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &packet);
			if (len < 0) {
			    restart_audio_resampling = 1;
			    break;
			}

			packet_data += len;
			packet_size -= len;

			if (!got_frame)
			    continue;

			int e;
			if (!swr) {
			    int rates[] = { 48000, 96000, 192000, 44100, 88200, 176400, 0 };
			    int *rate = rates;
			    int in_rate = c->sample_rate;
			    while (*rate && ((*rate / in_rate) * in_rate != *rate)
				   && (in_rate / *rate) * *rate != in_rate)
				rate++;
			    out_sample_rate = *rate ? *rate : 44100;
			    swr = swr_alloc();
			    out_channels = c->channels;
			    if (c->channel_layout == 0) {
				// FIXME -- need to guess, looks pretty much like a bug in the FFMPEG WMA decoder
				c->channel_layout = AV_CH_LAYOUT_STEREO;
			    }

			    out_channel_layout = c->channel_layout;
			    // player2 won't play mono
			    if (out_channel_layout == AV_CH_LAYOUT_MONO) {
				out_channel_layout = AV_CH_LAYOUT_STEREO;
				out_channels = 2;
			    }

			    av_opt_set_int(swr, "in_channel_layout", c->channel_layout, 0);
			    av_opt_set_int(swr, "out_channel_layout", out_channel_layout, 0);
			    av_opt_set_int(swr, "in_sample_rate", c->sample_rate, 0);
			    av_opt_set_int(swr, "out_sample_rate", out_sample_rate, 0);
			    av_opt_set_int(swr, "in_sample_fmt", c->sample_fmt, 0);
			    av_opt_set_int(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

			    e = swr_init(swr);
			    if (e < 0) {
				fprintf(stderr,
					"swr_init: %d (icl=%d ocl=%d isr=%d osr=%d isf=%d osf=%d\n",
					-e, (int) c->channel_layout,
					(int) out_channel_layout, c->sample_rate, out_sample_rate, c->sample_fmt, AV_SAMPLE_FMT_S16);
				swr_free(&swr);
				swr = NULL;
			    }
			}

			uint8_t *output = NULL;
			int in_samples = decoded_frame->nb_samples;
			int out_samples = av_rescale_rnd(swr_get_delay(swr, c->sample_rate) + in_samples, out_sample_rate, c->sample_rate, AV_ROUND_UP);
			e = av_samples_alloc(&output, NULL, out_channels, out_samples, AV_SAMPLE_FMT_S16, 1);
			if (e < 0) {
			    fprintf(stderr, "av_samples_alloc: %d\n", -e);
			    continue;
			}
			// FIXME. PTS calculation is probably broken.
			int64_t next_in_pts =  av_rescale(av_frame_get_best_effort_timestamp(decoded_frame),
							  ((AVStream *) audioTrack->stream)->time_base.num * (int64_t) out_sample_rate * c->sample_rate,
							  ((AVStream *) audioTrack->stream)->time_base.den);
			int64_t next_out_pts = av_rescale(swr_next_pts(swr, next_in_pts),
							  ((AVStream *) audioTrack->stream)->time_base.den,
							  ((AVStream *) audioTrack->stream)->time_base.num * (int64_t) out_sample_rate * c->sample_rate);
			currentAudioPts = audioTrack->pts = pts = calcPts(audioTrack->stream, next_out_pts);
			out_samples = swr_convert(swr, &output, out_samples, (const uint8_t **)
						  &decoded_frame->data[0], in_samples);

			avOut.uSampleRate = out_sample_rate;
			avOut.uNoOfChannels = av_get_channel_layout_nb_channels(out_channel_layout);
			avOut.uBitsPerSample = 16;
			avOut.bLittleEndian = 1;

			avOut.data = output;
			avOut.len = out_samples * sizeof(short) * out_channels;

			avOut.pts = videoTrack ? pts : 0;
			avOut.frameRate = 0;
			avOut.timeScale = 0;
			avOut.width = 0;
			avOut.height = 0;
			avOut.type = "audio";
		        avOut.stream = audioTrack->stream;
		        avOut.avfc = avContext;

			if (context->output->audio->Write(context, &avOut) < 0)
			    ffmpeg_err("writing data to audio device failed\n");
			av_freep(&output);
		    }
		} else {
		    avOut.data = packet_data;
		    avOut.len = packet_size;
		    avOut.pts = pts;
		    avOut.frameRate = 0;
		    avOut.timeScale = 0;
		    avOut.width = 0;
		    avOut.height = 0;
		    avOut.type = "audio";
		    avOut.stream = audioTrack->stream;
		    avOut.avfc = avContext;

		    if (context->output->audio->Write(context, &avOut) < 0)
			ffmpeg_err("writing data to audio device failed\n");
		}
	    }
	} else if (subtitleTrack && (subtitleTrack->Id == pid)) {
	    float duration = 3.0;
	    ffmpeg_printf(100, "subtitleTrack->stream %p \n", subtitleTrack->stream);

	    pts = calcPts(subtitleTrack->stream, packet.pts);

	    if (duration > 0.0) {
		/* is there a decoder ? */
		if (((AVStream *) subtitleTrack->stream)->codec->codec) {
		    AVSubtitle sub;
		    memset(&sub, 0, sizeof(sub));
		    int got_sub_ptr;

		    if (avcodec_decode_subtitle2(((AVStream *) subtitleTrack->stream)->codec, &sub, &got_sub_ptr, &packet) < 0) {
			ffmpeg_err("error decoding subtitle\n");
		    }

		    if (got_sub_ptr && sub.num_rects > 0) {
			    switch (sub.rects[0]->type) {
				case SUBTITLE_TEXT: // FIXME?
				case SUBTITLE_ASS:
					dvbsub_ass_write(((AVStream *) subtitleTrack->stream)->codec, &sub, pid);
					break;
				case SUBTITLE_BITMAP:
					ffmpeg_printf(0, "bitmap\n");
					dvbsub_write(&sub, pts);
					// avsubtitle_free() will be called by handler
					break;
				default:
					break;
			    }
		    }
		}
	    }			/* duration */
	} else if (teletextTrack && (teletextTrack->Id == pid)) {
		teletext_write(pid, packet_data, packet_size);
	}

	av_free_packet(&packet);
    }				/* while */

    if (context && context->playback && context->output && context->playback->abortRequested)
	context->output->Command(context, OUTPUT_CLEAR, NULL);

    dvbsub_ass_clear();

    if (swr)
	swr_free(&swr);
    if (decoded_frame)
	av_frame_free(&decoded_frame);

    if (context->playback)
	context->playback->abortPlayback = 1;
    hasPlayThreadStarted = 0;

    ffmpeg_printf(10, "terminating\n");
}

/* **************************** */
/* Container part for ffmpeg    */
/* **************************** */

static int terminating = 0;
static int interrupt_cb(void *ctx)
{
    PlaybackHandler_t *p = (PlaybackHandler_t *) ctx;
    return p->abortPlayback | p->abortRequested;
}

static void log_callback(void *ptr __attribute__ ((unused)), int lvl __attribute__ ((unused)), const char *format, va_list ap)
{
    if (debug_level > 10)
	vfprintf(stderr, format, ap);
}

static void container_ffmpeg_read_subtitle(Context_t * context, const char *filename, const char *format, int pid) {
	char *lastDot = strrchr(filename, '.');
	if (!lastDot)
		return;
	char *subfile = alloca(strlen(filename) + strlen(format));
	strcpy(subfile, filename);
	strcpy(subfile + (lastDot + 1 - filename), format);

	AVFormatContext *avfc = avformat_alloc_context();
	if (avformat_open_input(&avfc, subfile, av_find_input_format(format), 0)) {
        	avformat_free_context(avfc);
		return;
        }
        avformat_find_stream_info(avfc, NULL);
	if (avfc->nb_streams != 1) {
        	avformat_free_context(avfc);
		return;
	}

        AVCodecContext *c = avfc->streams[0]->codec;
        AVCodec *codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
        	avformat_free_context(avfc);
		return;
	}

        // fprintf(stderr, "codec=%s\n", avcodec_get_name(c->codec_id));
        if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "%s %d: avcodec_open\n", __FILE__, __LINE__);
        	avformat_free_context(avfc);
		return;
	}
        AVPacket avpkt;
        av_init_packet(&avpkt);
#if 0
	// FIXME, use custom values
        if (c->subtitle_header && !strcmp(format, "srt")) {
                av_freep(&c->subtitle_header);

		#define ASS_DEFAULT_FONT        "Arial"
		#define ASS_DEFAULT_FONT_SIZE   16
		#define ASS_DEFAULT_COLOR       0xffffff

        		ff_ass_subtitle_header(c, ASS_DEFAULT_FONT, ASS_DEFAULT_FONT_SIZE, ASS_DEFAULT_COLOR, 0, 0, 0, 0, 2);
	}
#endif

        if (c->subtitle_header)
                fprintf(stderr, "%s\n", c->subtitle_header);

        while (av_read_frame(avfc, &avpkt) > -1) {
		AVSubtitle sub;
	    	memset(&sub, 0, sizeof(sub));
                int got_sub = 0;
                avcodec_decode_subtitle2(c, &sub, &got_sub, &avpkt);
                if (got_sub)
			dvbsub_ass_write(c, &sub, pid);
                av_free_packet(&avpkt);
        }
        avformat_close_input(&avfc);
        avformat_free_context(avfc);

	Track_t track;
	memset(&track, 0, sizeof(track));
	track.Name = (char *) format;
	track.is_static = 1;
	track.Id = pid;
	track.Encoding = strcmp(format, "srt") ? "S_TEXT/ASS" : "S_TEXT/SRT";
	context->manager->subtitle->Command(context, MANAGER_ADD, &track);
}

static void container_ffmpeg_read_subtitles(Context_t * context, const char *filename) {
	if (strncmp(filename, "file://", 7))
		return;
	filename += 7;
	container_ffmpeg_read_subtitle(context, filename, "srt", 0xFFFF);
	container_ffmpeg_read_subtitle(context, filename, "ass", 0xFFFE);
	container_ffmpeg_read_subtitle(context, filename, "ssa", 0xFFFD);
}

int container_ffmpeg_init(Context_t * context, char *filename)
{
    int err;

    ffmpeg_printf(10, ">\n");

    av_log_set_callback(log_callback);

    if (filename == NULL) {
	ffmpeg_err("filename NULL\n");

	return cERR_CONTAINER_FFMPEG_NULL;
    }

    if (context == NULL) {
	ffmpeg_err("context NULL\n");

	return cERR_CONTAINER_FFMPEG_NULL;
    }

    ffmpeg_printf(10, "filename %s\n", filename);

    if (isContainerRunning) {
	ffmpeg_err("ups already running?\n");
	return cERR_CONTAINER_FFMPEG_RUNNING;
    }
    isContainerRunning = 1;

    /* initialize ffmpeg */
    avcodec_register_all();
    av_register_all();

    avformat_network_init();

    context->playback->abortRequested = 0;
    context->playback->abortPlayback = 0;
    avContext = avformat_alloc_context();
    avContext->interrupt_callback.callback = interrupt_cb;
    avContext->interrupt_callback.opaque = context->playback;

    if ((err = avformat_open_input(&avContext, filename, NULL, 0)) != 0) {
	char error[512];

	ffmpeg_err("avformat_open_input failed %d (%s)\n", err, filename);
	av_strerror(err, error, 512);
	ffmpeg_err("Cause: %s\n", error);

	isContainerRunning = 0;
	return cERR_CONTAINER_FFMPEG_OPEN;
    }

    avContext->iformat->flags |= AVFMT_SEEK_TO_PTS;
    avContext->flags = AVFMT_FLAG_GENPTS;
    if (context->playback->noprobe) {
	ffmpeg_printf(5, "noprobe\n");
	avContext->max_analyze_duration = 1;
	avContext->probesize = 8192;
    }

    ffmpeg_printf(20, "find_streaminfo\n");

    if (avformat_find_stream_info(avContext, NULL) < 0) {
	ffmpeg_err("Error avformat_find_stream_info\n");
#ifdef this_is_ok
	/* crow reports that sometimes this returns an error
	 * but the file is played back well. so remove this
	 * until other works are done and we can prove this.
	 */
	avformat_close_input(&avContext);
	isContainerRunning = 0;
	return cERR_CONTAINER_FFMPEG_STREAM;
#endif
    }

    terminating = 0;
    int res = container_ffmpeg_update_tracks(context, filename);

    unsigned int n, found_av = 0;
    for (n = 0; n < avContext->nb_streams; n++) {
	AVStream *stream = avContext->streams[n];
	switch (stream->codec->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
	case AVMEDIA_TYPE_VIDEO:
		found_av = 1;
	default:
		break;
    	}
    }
    if (!found_av) {
	avformat_close_input(&avContext);
	isContainerRunning = 0;
	return cERR_CONTAINER_FFMPEG_STREAM;
    }

    container_ffmpeg_read_subtitles(context, filename);
    return res;
}

int container_ffmpeg_update_tracks(Context_t * context, char *filename)
{
    if (terminating)
	return cERR_CONTAINER_FFMPEG_NO_ERROR;

    if (context->manager->chapter) {
	unsigned int i;
	context->manager->video->Command(context, MANAGER_INIT_UPDATE, NULL);
	for (i = 0; i < avContext->nb_chapters; i++) {
	    Track_t track;
	    memset(&track, 0, sizeof(track));
	    track.Id = i;
	    AVDictionaryEntry *title;
	    AVChapter *ch = avContext->chapters[i];
	    title = av_dict_get(ch->metadata, "title", NULL, 0);
	    track.Name = strdup(title ? title->value : "und");
	    ffmpeg_printf(10, "Chapter %s\n", track.Name);
	    track.chapter_start = (double) ch->start * av_q2d(ch->time_base) * 1000.0;
	    track.chapter_end = (double) ch->end * av_q2d(ch->time_base) * 1000.0;
	    context->manager->chapter->Command(context, MANAGER_ADD, &track);
	}
    }

    if (context->manager->video)
	context->manager->video->Command(context, MANAGER_INIT_UPDATE, NULL);
    if (context->manager->audio)
	context->manager->audio->Command(context, MANAGER_INIT_UPDATE, NULL);
    if (context->manager->subtitle)
	context->manager->subtitle->Command(context, MANAGER_INIT_UPDATE, NULL);
    if (context->manager->teletext)
	context->manager->teletext->Command(context, MANAGER_INIT_UPDATE, NULL);

    ffmpeg_printf(20, "dump format\n");
    av_dump_format(avContext, 0, filename, 0);

    ffmpeg_printf(1, "number streams %d\n", avContext->nb_streams);

    unsigned int n;

    for (n = 0; n < avContext->nb_streams; n++) {
	Track_t track;
	AVStream *stream = avContext->streams[n];
	int version = 0;

	char *encoding = Codec2Encoding(stream->codec, &version);

	if (encoding != NULL)
	    ffmpeg_printf(1, "%d. encoding = %s - version %d\n", n, encoding, version);

	if (!stream->id)
	    stream->id = n;

	/* some values in track are unset and therefor copyTrack segfaults.
	 * so set it by default to NULL!
	 */
	memset(&track, 0, sizeof(track));

	track.avfc = avContext;
	track.stream = stream;

	switch (stream->codec->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
	    ffmpeg_printf(10, "CODEC_TYPE_VIDEO %d\n", stream->codec->codec_type);

	    if (encoding != NULL) {
		track.type = eTypeES;
		track.version = version;

		track.width = stream->codec->width;
		track.height = stream->codec->height;

		track.frame_rate = stream->r_frame_rate.num;

		double frame_rate = av_q2d(stream->r_frame_rate);	/* rational to double */

		ffmpeg_printf(10, "frame_rate = %f\n", frame_rate);

		track.frame_rate = frame_rate * 1000.0;

		/* fixme: revise this */

		if (track.frame_rate < 23970)
		    track.TimeScale = 1001;
		else
		    track.TimeScale = 1000;

		ffmpeg_printf(20, "bit_rate = %d\n", stream->codec->bit_rate);
		ffmpeg_printf(20, "flags = %d\n", stream->codec->flags);
		ffmpeg_printf(20, "frame_bits = %d\n", stream->codec->frame_bits);
		ffmpeg_printf(20, "time_base.den %d\n", stream->time_base.den);
		ffmpeg_printf(20, "time_base.num %d\n", stream->time_base.num);
		ffmpeg_printf(20, "frame_rate %d\n", stream->r_frame_rate.num);
		ffmpeg_printf(20, "TimeScale %d\n", stream->r_frame_rate.den);

		ffmpeg_printf(20, "frame_rate %d\n", track.frame_rate);
		ffmpeg_printf(20, "TimeScale %d\n", track.TimeScale);

		track.Name = "und";
		track.Encoding = encoding;
		track.avfc = avContext;
		track.Id = stream->id;

		if (stream->duration == AV_NOPTS_VALUE) {
		    ffmpeg_printf(10, "Stream has no duration so we take the duration from context\n");
		    track.duration = (double) avContext->duration / 1000.0;
		} else {
		    track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;
		}

		if (context->manager->video)
		    if (context->manager->video->Command(context, MANAGER_ADD, &track) < 0) {
			/* konfetti: fixme: is this a reason to return with error? */
			ffmpeg_err("failed to add track %d\n", n);
		    }

	    } else {
		ffmpeg_err("codec type video but codec unknown %d\n", stream->codec->codec_id);
	    }
	    break;
	case AVMEDIA_TYPE_AUDIO:
	    ffmpeg_printf(10, "CODEC_TYPE_AUDIO %d\n", stream->codec->codec_type);

	    if (encoding != NULL) {
		AVDictionaryEntry *lang;
		track.type = eTypeES;

		lang = av_dict_get(stream->metadata, "language", NULL, 0);

		track.Name = lang ? lang->value : "und";

		ffmpeg_printf(10, "Language %s\n", track.Name);

		track.Encoding = encoding;
		track.Id = stream->id;
		track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;

		if (stream->duration == AV_NOPTS_VALUE) {
		    ffmpeg_printf(10, "Stream has no duration so we take the duration from context\n");
		    track.duration = (double) avContext->duration / 1000.0;
		} else {
		    track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;
		}

		if (!strncmp(encoding, "A_IPCM", 6)) {
		    track.inject_as_pcm = 1;
		    ffmpeg_printf(10, " Handle inject_as_pcm = %d\n", track.inject_as_pcm);
		}

		if (context->manager->audio) {
		    if (context->manager->audio->Command(context, MANAGER_ADD, &track) < 0) {
			/* konfetti: fixme: is this a reason to return with error? */
			ffmpeg_err("failed to add track %d\n", n);
		    }
		}

	    } else {
		ffmpeg_err("codec type audio but codec unknown %d\n", stream->codec->codec_id);
	    }
	    break;
	case AVMEDIA_TYPE_SUBTITLE:
	    {
		AVDictionaryEntry *lang;

		ffmpeg_printf(10, "CODEC_TYPE_SUBTITLE %d\n", stream->codec->codec_type);

		lang = av_dict_get(stream->metadata, "language", NULL, 0);

		track.Name = lang ? lang->value : "und";

		ffmpeg_printf(10, "Language %s\n", track.Name);

		track.Encoding = encoding;
		track.Id = stream->id;
		track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;

#if 0
		track.aacbuf = 0;
		track.have_aacheader = -1;
#endif

		track.width = -1;	/* will be filled online from videotrack */
		track.height = -1;	/* will be filled online from videotrack */

		ffmpeg_printf(10, "subtitle codec %d\n", stream->codec->codec_id);
		ffmpeg_printf(10, "subtitle width %d\n", stream->codec->width);
		ffmpeg_printf(10, "subtitle height %d\n", stream->codec->height);
		ffmpeg_printf(10, "subtitle stream %p\n", stream);

		ffmpeg_printf(10, "FOUND SUBTITLE %s\n", track.Name);

		if (stream->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT && context->manager->teletext) {
		    ffmpeg_printf(10, "dvb_teletext\n");
		    int i = 0;
		    AVDictionaryEntry *t = NULL;
		    do {
			char tmp[30];
			snprintf(tmp, sizeof(tmp), "teletext_%d", i);
			t = av_dict_get(stream->metadata, tmp, NULL, 0);
			if (t) {
			    track.Name = t->value;
			    if (context->manager->teletext->Command(context, MANAGER_ADD, &track) < 0)
				ffmpeg_err("failed to add teletext track %d\n", n);
			}
			i++;
		    } while (t);
		} else if (context->manager->subtitle) {
		    if (!stream->codec->codec) {
			stream->codec->codec = avcodec_find_decoder(stream->codec->codec_id);
			if (!stream->codec->codec)
			    ffmpeg_err("avcodec_find_decoder failed for subtitle track %d\n", n);
			else if (avcodec_open2(stream->codec, stream->codec->codec, NULL)) {
			    ffmpeg_err("avcodec_open2 failed for subtitle track %d\n", n);
			    stream->codec->codec = NULL;
			}
		    }
		    if (stream->codec->codec && context->manager->subtitle->Command(context, MANAGER_ADD, &track) < 0) {
			/* konfetti: fixme: is this a reason to return with error? */
			ffmpeg_err("failed to add subtitle track %d\n", n);
		    }
		}

		break;
	    }
	case AVMEDIA_TYPE_UNKNOWN:
	case AVMEDIA_TYPE_DATA:
	case AVMEDIA_TYPE_ATTACHMENT:
	case AVMEDIA_TYPE_NB:
	default:
	    ffmpeg_err("not handled or unknown codec_type %d\n", stream->codec->codec_type);
	    break;
	}

    }				/* for */

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_play(Context_t * context)
{
    int error;
    int ret = 0;
    pthread_attr_t attr;

    ffmpeg_printf(10, "\n");

    if (context && context->playback && context->playback->isPlaying) {
	ffmpeg_printf(10, "is Playing\n");
    } else {
	ffmpeg_printf(10, "is NOT Playing\n");
    }

    if (hasPlayThreadStarted == 0) {
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if ((error = pthread_create(&PlayThread, &attr, (void *) &FFMPEGThread, context)) != 0) {
	    ffmpeg_printf(10, "Error creating thread, error:%d:%s\n", error, strerror(error));
	    ret = cERR_CONTAINER_FFMPEG_ERR;
	} else {
	    ffmpeg_printf(10, "Created thread\n");
	}
    } else {
	ffmpeg_printf(10, "A thread already exists!\n");
	ret = cERR_CONTAINER_FFMPEG_ERR;
    }

    ffmpeg_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int container_ffmpeg_stop(Context_t * context)
{
    int ret = cERR_CONTAINER_FFMPEG_NO_ERROR;

    ffmpeg_printf(10, "\n");

    if (!isContainerRunning) {
	ffmpeg_err("Container not running\n");
	return cERR_CONTAINER_FFMPEG_ERR;
    }
    if (context->playback)
	context->playback->abortRequested = 1;

    while (hasPlayThreadStarted != 0)
	usleep(100000);

    terminating = 1;

    if (avContext)
	avformat_close_input(&avContext);

    isContainerRunning = 0;
    avformat_network_deinit();

    ffmpeg_printf(10, "ret %d\n", ret);
    return ret;
}

static int container_ffmpeg_seek(Context_t * context __attribute__ ((unused)), float sec, int absolute)
{
    if (absolute)
	seek_sec_abs = sec, seek_sec_rel = 0.0;
    else
	seek_sec_abs = -1.0, seek_sec_rel = sec;
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_get_length(Context_t * context, double *length)
{
    ffmpeg_printf(50, "\n");
    Track_t *videoTrack = NULL;
    Track_t *audioTrack = NULL;
    Track_t *subtitleTrack = NULL;
    Track_t *current = NULL;

    if (length == NULL) {
	ffmpeg_err("null pointer passed\n");
	return cERR_CONTAINER_FFMPEG_ERR;
    }

    context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
    context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);
    context->manager->subtitle->Command(context, MANAGER_GET_TRACK, &subtitleTrack);

    if (videoTrack != NULL)
	current = videoTrack;
    else if (audioTrack != NULL)
	current = audioTrack;
    else if (subtitleTrack != NULL)
	current = subtitleTrack;

    *length = 0.0;

    if (current != NULL) {
	if (current->duration == 0)
	    return cERR_CONTAINER_FFMPEG_ERR;
	else
	    *length = (current->duration / 1000.0);
    } else {
	if (avContext != NULL) {
	    *length = (avContext->duration / 1000.0);
	} else {
	    ffmpeg_err("no Track not context ->no problem :D\n");
	    return cERR_CONTAINER_FFMPEG_ERR;
	}
    }

    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_switch_audio(Context_t * context, int *arg)
{
    ffmpeg_printf(10, "track %d\n", *arg);
    /* Hellmaster1024: nothing to do here! */
    float sec = -5.0;
    context->playback->Command(context, PLAYBACK_SEEK, (void *) &sec);
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_switch_subtitle(Context_t * context __attribute__ ((unused)), int *arg __attribute__ ((unused)))
{
    /* Hellmaster1024: nothing to do here! */
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_switch_teletext(Context_t * context __attribute__ ((unused)), int *arg __attribute__ ((unused)))
{
    return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int container_ffmpeg_get_metadata(Context_t * context, char ***p)
{
	Track_t *videoTrack = NULL;
	Track_t *audioTrack = NULL;
	AVDictionaryEntry *tag = NULL;
	size_t psize = 1;
	char **pp;

	if (!context) {
		fprintf(stderr, "BUG %s:%d\n", __func__, __LINE__);
		return cERR_CONTAINER_FFMPEG_ERR;
	}

	if (!p || *p) {
		fprintf(stderr, "BUG %s:%d\n", __func__, __LINE__);
		return cERR_CONTAINER_FFMPEG_ERR;
	}

	context->manager->video->Command(context, MANAGER_GET_TRACK, &videoTrack);
	context->manager->audio->Command(context, MANAGER_GET_TRACK, &audioTrack);

	if (avContext->metadata)
		psize += av_dict_count(avContext->metadata);
	if (videoTrack)
		psize += av_dict_count(((AVStream *)(videoTrack->stream))->metadata);
	if (audioTrack)
		psize += av_dict_count(((AVStream *)(audioTrack->stream))->metadata);

	*p = malloc(sizeof(char *) * psize * 2);
	if (!*p) {
		fprintf(stderr, "MALLOC %s:%d\n", __func__, __LINE__);
		return cERR_CONTAINER_FFMPEG_ERR;
	}
	pp = *p;

	if (avContext->metadata)
		while ((tag = av_dict_get(avContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			*pp++ = strdup(tag->key);
			*pp++ = strdup(tag->value);
		}

	if (videoTrack) {
		tag = NULL;
		while ((tag = av_dict_get(((AVStream *)(videoTrack->stream))->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			*pp++ = strdup(tag->key);
			*pp++ = strdup(tag->value);
		}
	}
	if (audioTrack) {
		tag = NULL;
		while ((tag = av_dict_get(((AVStream *)(audioTrack->stream))->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			*pp++ = strdup(tag->key);
			*pp++ = strdup(tag->value);
		}
	}
	*pp++ = NULL;
	*pp = NULL;

	return cERR_CONTAINER_FFMPEG_NO_ERROR;
}

static int Command(Context_t *context, ContainerCmd_t command, void *argument)
{
    int ret = cERR_CONTAINER_FFMPEG_NO_ERROR;

    ffmpeg_printf(50, "Command %d\n", command);

    if (command != CONTAINER_INIT && !avContext)
	return cERR_CONTAINER_FFMPEG_ERR;
    if (command == CONTAINER_INIT && avContext)
	return cERR_CONTAINER_FFMPEG_ERR;
    switch (command) {
    case CONTAINER_INIT:{
	    char *filename = (char *) argument;
	    ret = container_ffmpeg_init(context, filename);
	    break;
	}
    case CONTAINER_PLAY:{
	    ret = container_ffmpeg_play(context);
	    break;
	}
    case CONTAINER_STOP:{
	    ret = container_ffmpeg_stop(context);
	    break;
	}
    case CONTAINER_SEEK:{
	    ret = container_ffmpeg_seek(context, (float) *((float *) argument), 0);
	    break;
	}
    case CONTAINER_SEEK_ABS:{
	    ret = container_ffmpeg_seek(context, (float) *((float *) argument), -1);
	    break;
	}
    case CONTAINER_LENGTH:{
	    double length = 0;
	    ret = container_ffmpeg_get_length(context, &length);

	    *((double *) argument) = (double) length;
	    break;
	}
    case CONTAINER_SWITCH_AUDIO:{
	    ret = container_ffmpeg_switch_audio(context, (int *) argument);
	    break;
	}
    case CONTAINER_SWITCH_SUBTITLE:{
	    ret = container_ffmpeg_switch_subtitle(context, (int *) argument);
	    break;
	}
    case CONTAINER_METADATA:{
	    ret = container_ffmpeg_get_metadata(context, (char ***) argument);
	    break;
	}
    case CONTAINER_SWITCH_TELETEXT:{
	    ret = container_ffmpeg_switch_teletext(context, (int *) argument);
	    break;
	}
    default:
	ffmpeg_err("ContainerCmd %d not supported!\n", command);
	ret = cERR_CONTAINER_FFMPEG_ERR;
	break;
    }

    ffmpeg_printf(50, "exiting with value %d\n", ret);

    return ret;
}

static char *FFMPEG_Capabilities[] = { "avi", "mkv", "mp4", "ts", "mov", "flv", "flac", "mp3", "mpg",
    "m2ts", "vob", "wmv", "wma", "asf", "mp2", "m4v", "m4a", "divx", "dat",
    "mpeg", "trp", "mts", "vdr", "ogg", "wav", "wtv", "ogm", "3gp", NULL
};

Container_t FFMPEGContainer = {
    "FFMPEG",
    &Command,
    FFMPEG_Capabilities
};

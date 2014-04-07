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

#include <linux/dvb/stm_ioctls.h>

#include "player.h"
#include "misc.h"
#include "debug.h"

Input::Input()
{
	videoTrack = NULL;
	audioTrack = NULL;
	subtitleTrack = NULL;
	teletextTrack = NULL;

	hasPlayThreadStarted = 0;
	isContainerRunning = 0;
	seek_sec_abs = -1.0;
	seek_sec_rel = 0.0;
	terminating = false;
}

Input::~Input()
{
	Stop();
}

int64_t calcPts(AVFormatContext *avfc, AVStream * stream, int64_t pts)
{
    if (!avfc || !stream) {
	fprintf(stderr, "player / stream null\n");
	return INVALID_PTS_VALUE;
    }

    if (pts == AV_NOPTS_VALUE)
	pts = INVALID_PTS_VALUE;
    else if (avfc->start_time == AV_NOPTS_VALUE)
	pts = 90000.0 * (double) pts * av_q2d(stream->time_base);
    else
	pts = 90000.0 * (double) pts * av_q2d(stream->time_base) - 90000.0 * avfc->start_time / AV_TIME_BASE;

    if (pts & 0x8000000000000000ull)
	pts = INVALID_PTS_VALUE;

    return pts;
}

// from neutrino-mp/lib/libdvbsubtitle/dvbsub.cpp
extern "C" void dvbsub_write(AVSubtitle *, int64_t);
extern "C" void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid);
extern "C" void dvbsub_ass_clear(void);
// from neutrino-mp/lib/lib/libtuxtxt/tuxtxt_common.h
extern "C" void teletext_write(int pid, uint8_t *data, int size);

bool Input::Play()
{
    char threadname[17];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[16] = 0;
    prctl(PR_SET_NAME, (unsigned long) &threadname);

    hasPlayThreadStarted = 1;

    int64_t currentVideoPts = 0, currentAudioPts = 0, showtime = 0, bofcount = 0;

    while (player->isCreationPhase) {
	fprintf(stderr, "Thread waiting for end of init phase...\n");
	usleep(1000);
    }
    fprintf(stderr, "Running!\n");

    bool restart_audio_resampling = false;

    while (player->isPlaying && !player->abortRequested) {

	//IF MOVIE IS PAUSED, WAIT
	if (player->isPaused) {
	    fprintf(stderr, "paused\n");

	    usleep(100000);
	    continue;
	}

	int seek_target_flag = 0;
	int64_t seek_target = INT64_MIN;

	if (seek_sec_rel != 0.0) {
	    if (avfc->iformat->flags & AVFMT_TS_DISCONT) {
		float br = (avfc->bit_rate) ? avfc->bit_rate / 8.0 : 180000.0;
		seek_target_flag = AVSEEK_FLAG_BYTE;
		seek_target = avio_tell(avfc->pb) + seek_sec_rel * br;
	    } else {
		seek_target = ((((currentVideoPts > 0) ? currentVideoPts : currentAudioPts) / 90000.0) + seek_sec_rel) * AV_TIME_BASE;
	    }
	    seek_sec_rel = 0.0;
	} else if (seek_sec_abs >= 0.0) {
	    if (avfc->iformat->flags & AVFMT_TS_DISCONT) {
		float br = (avfc->bit_rate) ? avfc->bit_rate / 8.0 : 180000.0;
		seek_target_flag = AVSEEK_FLAG_BYTE;
		seek_target = seek_sec_abs * br;
	    } else {
		seek_target = seek_sec_abs * AV_TIME_BASE;
	    }
	    seek_sec_abs = -1.0;
	} else if (player->isBackWard && av_gettime() >= showtime) {
	    player->output.ClearVideo();

	    if (bofcount == 1) {
		showtime = av_gettime();
		usleep(100000);
		continue;
	    }

	    if (avfc->iformat->flags & AVFMT_TS_DISCONT) {
		off_t pos = avio_tell(avfc->pb);

		if (pos > 0) {
		    float br;
		    if (avfc->bit_rate)
			br = avfc->bit_rate / 8.0;
		    else
			br = 180000.0;
		    seek_target = pos + player->Speed * 8 * br;
		    seek_target_flag = AVSEEK_FLAG_BYTE;
		}
	    } else {
		seek_target = ((((currentVideoPts > 0) ? currentVideoPts : currentAudioPts) / 90000.0) + player->Speed * 8) * AV_TIME_BASE;;
	    }
	    showtime = av_gettime() + 300000;	//jump back every 300ms
	} else {
	    bofcount = 0;
	}

	if (seek_target > INT64_MIN) {
	    int res;
	    if (seek_target < 0)
		seek_target = 0;
	    res = avformat_seek_file(avfc, -1, INT64_MIN, seek_target, INT64_MAX, seek_target_flag);

	    if (res < 0 && player->isBackWard)
		bofcount = 1;
	    seek_target = INT64_MIN;
	    restart_audio_resampling = true;

	    // flush streams
	    unsigned int i;
	    for (i = 0; i < avfc->nb_streams; i++)
		if (avfc->streams[i]->codec && avfc->streams[i]->codec->codec)
		    avcodec_flush_buffers(avfc->streams[i]->codec);
	}

	AVPacket packet;
	av_init_packet(&packet);

	int av_res = av_read_frame(avfc, &packet);
	if (av_res == AVERROR(EAGAIN)) {
	    av_free_packet(&packet);
	    continue;
	}
	if (av_res) {		// av_read_frame failed
	    fprintf(stderr, "no data ->end of file reached ?\n");
	    av_free_packet(&packet);
	    break;		// while
	}
	long long int pts;

	player->readCount += packet.size;

	int pid = avfc->streams[packet.stream_index]->id;


Track *_videoTrack = videoTrack;
Track *_audioTrack = audioTrack;
Track *_subtitleTrack = subtitleTrack;
Track *_teletextTrack = teletextTrack;


	if (_videoTrack && (_videoTrack->pid == pid)) {
	    currentVideoPts = pts = calcPts(avfc, _videoTrack->stream, packet.pts);

	    if (!player->output.Write(avfc, _videoTrack->stream, &packet, currentVideoPts))
		;//fprintf(stderr, "writing data to video device failed\n");
	} else if (_audioTrack && (_audioTrack->pid == pid)) {
	    if (restart_audio_resampling) {
	    	restart_audio_resampling = false;
		player->output.Write(avfc, _audioTrack->stream, NULL, currentAudioPts);
	    }
	    if (!player->isBackWard) {
		currentAudioPts = pts = calcPts(avfc, _audioTrack->stream, packet.pts);
		if (!player->output.Write(avfc, _audioTrack->stream, &packet, currentAudioPts))
			;//fprintf(stderr, "writing data to audio device failed\n");
	    }
	} else if (_subtitleTrack && (_subtitleTrack->pid == pid)) {
	    float duration = 3.0;

	    pts = calcPts(avfc, _subtitleTrack->stream, packet.pts);

	    if (duration > 0.0) {
		/* is there a decoder ? */
		if (((AVStream *) _subtitleTrack->stream)->codec->codec) {
		    AVSubtitle sub;
		    memset(&sub, 0, sizeof(sub));
		    int got_sub_ptr;

		    if (avcodec_decode_subtitle2(((AVStream *) _subtitleTrack->stream)->codec, &sub, &got_sub_ptr, &packet) < 0) {
			fprintf(stderr, "error decoding subtitle\n");
		    }

		    if (got_sub_ptr && sub.num_rects > 0) {
			    switch (sub.rects[0]->type) {
				case SUBTITLE_TEXT: // FIXME?
				case SUBTITLE_ASS:
					dvbsub_ass_write(((AVStream *) _subtitleTrack->stream)->codec, &sub, pid);
					break;
				case SUBTITLE_BITMAP:
					dvbsub_write(&sub, pts);
					// avsubtitle_free() will be called by handler
					break;
				default:
					break;
			    }
		    }
		}
	    }			/* duration */
	} else if (_teletextTrack && (_teletextTrack->pid == pid)) {
		teletext_write(pid, packet.data, packet.size);
	}

	av_free_packet(&packet);
    }				/* while */

    if (player && player->abortRequested)
	player->output.Clear();

    dvbsub_ass_clear();

	player->abortPlayback = 1;
    hasPlayThreadStarted = 0;

	return true;
}

/* **************************** */
/* Container part for ffmpeg    */
/* **************************** */

/*static*/ int interrupt_cb(void *arg)
{
    Player *player = (Player *) arg;
    return player->abortPlayback | player->abortRequested;
}

static void log_callback(void *ptr __attribute__ ((unused)), int lvl __attribute__ ((unused)), const char *format, va_list ap)
{
//    if (debug_level > 10)
	vfprintf(stderr, format, ap);
}

bool Input::ReadSubtitle(const char *filename, const char *format, int pid)
{
	const char *lastDot = strrchr(filename, '.');
	if (!lastDot)
		return false;
	char *subfile = (char *) alloca(strlen(filename) + strlen(format));
	strcpy(subfile, filename);
	strcpy(subfile + (lastDot + 1 - filename), format);

	AVFormatContext *subavfc = avformat_alloc_context();
	if (avformat_open_input(&subavfc, subfile, av_find_input_format(format), 0)) {
        	avformat_free_context(subavfc);
		return false;
        }
        avformat_find_stream_info(subavfc, NULL);
	if (subavfc->nb_streams != 1) {
        	avformat_free_context(subavfc);
		return false;
	}

        AVCodecContext *c = subavfc->streams[0]->codec;
        AVCodec *codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
        	avformat_free_context(subavfc);
		return false;
	}

        // fprintf(stderr, "codec=%s\n", avcodec_get_name(c->codec_id));
        if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "%s %d: avcodec_open\n", __FILE__, __LINE__);
        	avformat_free_context(subavfc);
		return false;
	}
        AVPacket avpkt;
        av_init_packet(&avpkt);

        if (c->subtitle_header)
                fprintf(stderr, "%s\n", c->subtitle_header);

        while (av_read_frame(subavfc, &avpkt) > -1) {
		AVSubtitle sub;
	    	memset(&sub, 0, sizeof(sub));
                int got_sub = 0;
                avcodec_decode_subtitle2(c, &sub, &got_sub, &avpkt);
                if (got_sub)
			dvbsub_ass_write(c, &sub, pid);
                av_free_packet(&avpkt);
        }
        avformat_close_input(&subavfc);
        avformat_free_context(subavfc);

	Track track;
	track.Name = format;
	track.is_static = 1;
	track.pid = pid;
	player->manager.addSubtitleTrack(track);
	return true;
}

bool Input::ReadSubtitles(const char *filename) {
	if (strncmp(filename, "file://", 7))
		return false;
	filename += 7;
	bool ret = false;
	ret |= ReadSubtitle(filename, "srt", 0xFFFF);
	ret |= ReadSubtitle(filename, "ass", 0xFFFE);
	ret |= ReadSubtitle(filename, "ssa", 0xFFFD);
	return ret;
}

bool Input::Init(const char *filename)
{
    int err;

    av_log_set_callback(log_callback);

    if (filename == NULL) {
	fprintf(stderr, "filename NULL\n");

	return false;
    }

    if (isContainerRunning) {
	fprintf(stderr, "ups already running?\n");
	return false;
    }
    isContainerRunning = true;

    /* initialize ffmpeg */
    avcodec_register_all();
    av_register_all();

    avformat_network_init();

    player->abortRequested = 0;
    player->abortPlayback = 0;
    videoTrack = NULL;
    audioTrack = NULL;
    subtitleTrack = NULL;
    teletextTrack = NULL;
    avfc = avformat_alloc_context();
    avfc->interrupt_callback.callback = interrupt_cb;
    avfc->interrupt_callback.opaque = (void *) &player;

    if ((err = avformat_open_input(&avfc, filename, NULL, 0)) != 0) {
	char error[512];

	fprintf(stderr, "avformat_open_input failed %d (%s)\n", err, filename);
	av_strerror(err, error, 512);
	fprintf(stderr, "Cause: %s\n", error);

	isContainerRunning = false;
	return false;
    }

    avfc->iformat->flags |= AVFMT_SEEK_TO_PTS;
    avfc->flags = AVFMT_FLAG_GENPTS;
    if (player->noprobe) {
	avfc->max_analyze_duration = 1;
	avfc->probesize = 8192;
    }

    if (avformat_find_stream_info(avfc, NULL) < 0) {
	fprintf(stderr, "Error avformat_find_stream_info\n");
#ifdef this_is_ok
	/* crow reports that sometimes this returns an error
	 * but the file is played back well. so remove this
	 * until other works are done and we can prove this.
	 */
	avformat_close_input(&avfc);
	isContainerRunning = false;
	return false;
#endif
    }

    terminating = false;
    int res = UpdateTracks();

    if (!videoTrack && !audioTrack) {
	avformat_close_input(&avfc);
	isContainerRunning = false;
	return false;
    }

    if (videoTrack)
	player->output.SwitchVideo(videoTrack->stream);
    if (audioTrack)
	player->output.SwitchAudio(audioTrack->stream);
	
    ReadSubtitles(filename);

    return res;
}

bool Input::UpdateTracks()
{
    if (terminating)
	return true;

	std::vector<Chapter> chapters;
	for (unsigned int i = 0; i < avfc->nb_chapters; i++) {
		AVDictionaryEntry *title = av_dict_get(avfc->metadata, "title", NULL, 0);
		if (!title)
			continue;
		AVChapter *ch = avfc->chapters[i];
		if (!ch)
			continue;
		Chapter chapter;
		chapter.title = title ? title->value : "unknown";
		chapter.start = (double) ch->start * av_q2d(ch->time_base) * 1000.0;
		chapter.end = (double) ch->end * av_q2d(ch->time_base) * 1000.0;
		chapters.push_back(chapter);
	}
	player->SetChapters(chapters);

    player->manager.initTrackUpdate();

    av_dump_format(avfc, 0, player->url.c_str(), 0);

    unsigned int n;

    for (n = 0; n < avfc->nb_streams; n++) {
	Track track;
	AVStream *stream = avfc->streams[n];

	if (!stream->id)
	    stream->id = n + 1;

	track.avfc = avfc;
	track.stream = stream;

	switch (stream->codec->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		track.Name = "und";
		track.avfc = avfc;
		track.pid = stream->id;

		if (stream->duration == AV_NOPTS_VALUE) {
		    track.duration = (double) avfc->duration / 1000.0;
		} else {
		    track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;
		}

		player->manager.addVideoTrack(track);
		if (!videoTrack)
			videoTrack = player->manager.getVideoTrack(track.pid);
	    break;
	case AVMEDIA_TYPE_AUDIO:
		AVDictionaryEntry *lang;

		lang = av_dict_get(stream->metadata, "language", NULL, 0);

		track.Name = lang ? lang->value : "und";

		track.pid = stream->id;
		track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;

		if (stream->duration == AV_NOPTS_VALUE) {
		    track.duration = (double) avfc->duration / 1000.0;
		} else {
		    track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;
		}

		switch(stream->codec->codec_id) {
			case AUDIO_ENCODING_MPEG2:
				track.ac3flags = 9;
				break;
			case AV_CODEC_ID_MP3:
				track.ac3flags = 4;
				break;
			case AV_CODEC_ID_AC3:
				track.ac3flags = 1;
				break;
			case AV_CODEC_ID_EAC3:
				track.ac3flags = 7;
				break;
			case AV_CODEC_ID_DTS:
				track.ac3flags = 6;
				break;
			case AV_CODEC_ID_AAC:
				track.ac3flags = 5;
				break;
			default:
				track.ac3flags = 0;
		}
		player->manager.addAudioTrack(track);
		if (!audioTrack)
			audioTrack = player->manager.getAudioTrack(track.pid);

	    break;
	case AVMEDIA_TYPE_SUBTITLE:
	    {
		AVDictionaryEntry *lang;

		lang = av_dict_get(stream->metadata, "language", NULL, 0);

		track.Name = lang ? lang->value : "und";

		track.pid = stream->id;
		track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;

		if (stream->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
		    int i = 0;
		    AVDictionaryEntry *t = NULL;
		    do {
			char tmp[30];
			snprintf(tmp, sizeof(tmp), "teletext_%d", i);
			t = av_dict_get(stream->metadata, tmp, NULL, 0);
			if (t) {
			    char lang[strlen(t->value)];
			    if (5 == sscanf(t->value, "%d %s %d %d %d", &track.pid, lang, &track.type, &track.mag, &track.page)) {
				    track.Name = lang;
				    player->manager.addTeletextTrack(track);
			    }
			}
			i++;
		    } while (t);
		} else {
		    if (!stream->codec->codec) {
			stream->codec->codec = avcodec_find_decoder(stream->codec->codec_id);
			if (!stream->codec->codec)
			    fprintf(stderr, "avcodec_find_decoder failed for subtitle track %d\n", n);
			else if (avcodec_open2(stream->codec, stream->codec->codec, NULL)) {
			    fprintf(stderr, "avcodec_open2 failed for subtitle track %d\n", n);
			    stream->codec->codec = NULL;
			}
		    }
		    if (stream->codec->codec)
			player->manager.addSubtitleTrack(track);
	    	}

		break;
	    }
	default:
	    fprintf(stderr, "not handled or unknown codec_type %d\n", stream->codec->codec_type);
	    break;
	}

    }				/* for */

    return true;
}

bool Input::Stop()
{
	if (!isContainerRunning) {
		fprintf(stderr, "Container not running\n");
		return false;
	}
	player->abortRequested = 1;

	while (hasPlayThreadStarted != 0)
		usleep(100000);

	terminating = true;

	if (avfc)
		avformat_close_input(&avfc);

	isContainerRunning = false;

	avformat_network_deinit();

	return true;
}

bool Input::Seek(float sec, bool absolute)
{
    if (absolute)
	seek_sec_abs = sec, seek_sec_rel = 0.0;
    else
	seek_sec_abs = -1.0, seek_sec_rel = sec;
    return true;
}

bool Input::GetDuration(double &duration)
{
	duration = 0.0;

	if (videoTrack && videoTrack->duration != 0.0) {
		duration = videoTrack->duration / 1000.0;
		return true;
	}
	if (audioTrack && audioTrack->duration != 0.0) {
		duration = audioTrack->duration / 1000.0;
		return true;
	}
	if (subtitleTrack && subtitleTrack->duration != 0.0) {
		duration = subtitleTrack->duration / 1000.0;
		return true;
	}
	if (avfc && avfc->duration != 0.0) {
		duration = avfc->duration /1000.0;
		return true;
	}
	return false;
}

bool Input::SwitchAudio(Track *track)
{
	audioTrack = track;
	player->output.SwitchAudio(track ? track->stream : NULL);
	float sec = -5.0;
	player->Seek(sec, false);
	return true;
}

bool Input::SwitchSubtitle(Track *track)
{
	subtitleTrack = track;
	return true;
}

bool Input::SwitchTeletext(Track *track)
{
	teletextTrack = track;
	return true;
}

bool Input::SwitchVideo(Track *track)
{
	videoTrack = track;
	return true;
}

bool Input::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();

	if (avfc) {
		AVDictionaryEntry *tag = NULL;

		if (avfc->metadata)
			while ((tag = av_dict_get(avfc->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
				keys.push_back(tag->key);
				values.push_back(tag->value);
			}

		if (videoTrack)
			while ((tag = av_dict_get(videoTrack->stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
				keys.push_back(tag->key);
				values.push_back(tag->value);
			}

		if (audioTrack)
			while ((tag = av_dict_get(audioTrack->stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
				keys.push_back(tag->key);
				values.push_back(tag->value);
			}

		return true;
	}

	return false;
}

bool Input::GetReadCount(uint64_t &readcount)
{
	readcount = readCount;
	return true;
}

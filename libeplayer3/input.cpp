/*
 * input class
 *
 * based on libeplayer3 container_ffmpeg.c, konfetti 2010; based on code from crow
 *
 * Copyright (C) 2014  martii
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "player.h"
#include "misc.h"

#define averror(_err,_fun) ({										\
	if (_err < 0) {											\
		char _error[512];									\
		av_strerror(_err, _error, sizeof(_error));						\
		fprintf(stderr, "%s %d: %s: %d (%s)\n", __FILE__, __LINE__, #_fun, _err, _error);	\
	}												\
	_err;												\
})

Input::Input()
{
	videoTrack = NULL;
	audioTrack = NULL;
	subtitleTrack = NULL;
	teletextTrack = NULL;

	hasPlayThreadStarted = 0;
	seek_sec_abs = -1.0;
	seek_sec_rel = 0.0;
	abortPlayback = false;
}

Input::~Input()
{
	Stop();
}

int64_t calcPts(AVFormatContext *avfc, AVStream * stream, int64_t pts)
{
	if (!avfc || !stream) {
		fprintf(stderr, "context / stream null\n");
		return INVALID_PTS_VALUE;
	}

	if (pts == AV_NOPTS_VALUE)
		return INVALID_PTS_VALUE;

	pts = 90000.0 * (double) pts * av_q2d(stream->time_base);
	if (avfc->start_time != AV_NOPTS_VALUE)
		pts -= 90000.0 * avfc->start_time / AV_TIME_BASE;

	if (pts & 0x8000000000000000ll)
		return INVALID_PTS_VALUE;

	return pts;
}

// from neutrino-mp/lib/libdvbsubtitle/dvbsub.cpp
extern void dvbsub_write(AVSubtitle *, int64_t);
extern void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid);
extern void dvbsub_ass_clear(void);
// from neutrino-mp/lib/lib/libtuxtxt/tuxtxt_common.h
extern void teletext_write(int pid, uint8_t *data, int size);

bool Input::Play()
{
	hasPlayThreadStarted = 1;

	int64_t showtime = 0, bofcount = 0;
	bool restart_audio_resampling = false;
	int warnAudioWrite = 0;
	int warnVideoWrite = 0;

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
				int64_t pts;
				if(player->output.GetPts(pts))
					seek_target = (pts * AV_TIME_BASE)/ 90000.0 + seek_sec_rel * AV_TIME_BASE;
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
					float br = avfc->bit_rate ? avfc->bit_rate / 8.0 : 180000.0;
					seek_target_flag = AVSEEK_FLAG_BYTE;
					seek_target = pos + player->Speed * 8 * br;
				}
			} else {
				int64_t pts;
				if(player->output.GetPts(pts))
					seek_target = (pts * AV_TIME_BASE)/ 90000.0 + seek_sec_rel * AV_TIME_BASE;
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
			player->output.ClearVideo();
		}

		AVPacket packet;
		av_init_packet(&packet);

		int err = av_read_frame(avfc, &packet);
		if (err == AVERROR(EAGAIN)) {
			av_free_packet(&packet);
			continue;
		}
		if (averror(err, av_read_frame)) // EOF?
			break;		// while

		player->readCount += packet.size;

		AVStream *stream = avfc->streams[packet.stream_index];
		Track *_videoTrack = videoTrack;
		Track *_audioTrack = audioTrack;
		Track *_subtitleTrack = subtitleTrack;
		Track *_teletextTrack = teletextTrack;

		if (_videoTrack && (_videoTrack->stream == stream)) {
			int64_t pts = calcPts(avfc, stream, packet.pts);
			if (!player->output.Write(avfc, stream, &packet, pts)) {
				if (warnVideoWrite)
					warnVideoWrite--;
				else {
					fprintf(stderr, "writing data to %s device failed\n", "video");
					warnVideoWrite = 100;
				}
			}
		} else if (_audioTrack && (_audioTrack->stream == stream)) {
			if (restart_audio_resampling) {
				restart_audio_resampling = false;
				player->output.Write(avfc, stream, NULL, 0);
			}
			if (!player->isBackWard) {
				int64_t pts = calcPts(avfc, stream, packet.pts);
				if (!player->output.Write(avfc, stream, &packet, pts)) {
					if (warnAudioWrite)
						warnAudioWrite--;
					else {
						fprintf(stderr, "writing data to %s device failed\n", "audio");
						warnAudioWrite = 100;
					}
				}
			}
		} else if (_subtitleTrack && (_subtitleTrack->stream == stream)) {
			if (stream->codec->codec) {
				AVSubtitle sub;
				memset(&sub, 0, sizeof(sub));
				int got_sub_ptr = 0;

				err = avcodec_decode_subtitle2(stream->codec, &sub, &got_sub_ptr, &packet);
				averror(err, avcodec_decode_subtitle2);

				if (got_sub_ptr && sub.num_rects > 0) {
					switch (sub.rects[0]->type) {
						case SUBTITLE_TEXT: // FIXME?
						case SUBTITLE_ASS:
							dvbsub_ass_write(stream->codec, &sub, stream->id);
							break;
						case SUBTITLE_BITMAP: {
							int64_t pts = calcPts(avfc, stream, packet.pts);
							dvbsub_write(&sub, calcPts(avfc, stream, pts));
							// avsubtitle_free() will be called by handler
							break;
						}
						default:
							break;
					}
				}
			}
		} else if (_teletextTrack && (_teletextTrack->stream == stream)) {
			teletext_write(stream->id, packet.data, packet.size);
		}

		av_free_packet(&packet);
	}				/* while */

	if (player->abortRequested)
		player->output.Clear();
	else
		player->output.Flush();

	dvbsub_ass_clear();
	abortPlayback = true;
	hasPlayThreadStarted = false;

	return true;
}

/*static*/ int interrupt_cb(void *arg)
{
	Player *player = (Player *) arg;
	bool res = player->input.abortPlayback || player->abortRequested;
	if (res)
		fprintf(stderr, "%s %s %d: abort requested (%d/%d)\n", __FILE__, __func__, __LINE__, player->input.abortPlayback, player->abortRequested);
	return res;
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

	if (access(subfile, R_OK))
		return false;

	AVFormatContext *subavfc = avformat_alloc_context();
	int err = avformat_open_input(&subavfc, subfile, av_find_input_format(format), 0);
	if (averror(err, avformat_open_input)) {
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

	err = avcodec_open2(c, codec, NULL);
	if (averror(err, avcodec_open2)) {
		avformat_free_context(subavfc);
		return false;
	}

	AVPacket packet;
	av_init_packet(&packet);

	if (c->subtitle_header)
		fprintf(stderr, "%s\n", c->subtitle_header);

	while (av_read_frame(subavfc, &packet) > -1) {
		AVSubtitle sub;
		memset(&sub, 0, sizeof(sub));
		int got_sub = 0;
		avcodec_decode_subtitle2(c, &sub, &got_sub, &packet);
		if (got_sub)
			dvbsub_ass_write(c, &sub, pid);
		av_free_packet(&packet);
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
	abortPlayback = false;
	av_log_set_callback(log_callback);

	if (!filename) {
		fprintf(stderr, "filename NULL\n");
		return false;
	}
	fprintf(stderr, "%s %s %d: %s\n", __FILE__, __func__, __LINE__, filename);

	avcodec_register_all();
	av_register_all();
	avformat_network_init();

	videoTrack = NULL;
	audioTrack = NULL;
	subtitleTrack = NULL;
	teletextTrack = NULL;
	avfc = avformat_alloc_context();
	avfc->interrupt_callback.callback = interrupt_cb;
	avfc->interrupt_callback.opaque = (void *) player;

	int err = avformat_open_input(&avfc, filename, NULL, 0);
	if (averror(err, avformat_open_input))
		return false;

	avfc->iformat->flags |= AVFMT_SEEK_TO_PTS;
	avfc->flags = AVFMT_FLAG_GENPTS;
	if (player->noprobe) {
		avfc->max_analyze_duration = 1;
		avfc->probesize = 8192;
	}

	err = avformat_find_stream_info(avfc, NULL);
	if (averror(err, avformat_open_input)) {
		avformat_close_input(&avfc);
		return false;
	}

	bool res = UpdateTracks();

	if (!videoTrack && !audioTrack) {
		avformat_close_input(&avfc);
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
	if (abortPlayback)
		return true;

	std::vector<Chapter> chapters;
	for (unsigned int i = 0; i < avfc->nb_chapters; i++) {
		AVChapter *ch = avfc->chapters[i];
		AVDictionaryEntry* title = av_dict_get(ch->metadata, "title", NULL, 0);
		Chapter chapter;
		chapter.title = title ? title->value : "";
		chapter.start = (double) ch->start * av_q2d(ch->time_base) * 1000.0;
		chapter.end = (double) ch->end * av_q2d(ch->time_base) * 1000.0;
		chapters.push_back(chapter);
	}
	player->SetChapters(chapters);

	player->manager.initTrackUpdate();

	av_dump_format(avfc, 0, player->url.c_str(), 0);

	for (unsigned int n = 0; n < avfc->nb_streams; n++) {
		AVStream *stream = avfc->streams[n];

		if (!stream->id)
			stream->id = n + 1;

		Track track;
		track.avfc = avfc;
		track.stream = stream;
		AVDictionaryEntry *lang = av_dict_get(stream->metadata, "language", NULL, 0);
		track.Name = lang ? lang->value : "";
		track.pid = stream->id;
		if (stream->duration == AV_NOPTS_VALUE)
			track.duration = (double) avfc->duration / 1000.0;
		else
			track.duration = (double) stream->duration * av_q2d(stream->time_base) * 1000.0;

		switch (stream->codec->codec_type) {
			case AVMEDIA_TYPE_VIDEO: {
				player->manager.addVideoTrack(track);
				if (!videoTrack)
					videoTrack = player->manager.getVideoTrack(track.pid);
				break;
			}
			case AVMEDIA_TYPE_AUDIO: {
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
			}
			case AVMEDIA_TYPE_SUBTITLE: {
				if (stream->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
					std::string l = lang ? lang->value : "";
					uint8_t *data = stream->codec->extradata;
					int size = stream->codec->extradata_size;
					if (size > 0 && 2 * size - 1 == l.length())
						for (int i = 0; i < size; i += 2) {
							track.Name = l.substr(i * 2, 3);
							track.type = data[i] >> 3;
							track.mag = data[i] & 7;
							track.page = data[i + 1];
							player->manager.addTeletextTrack(track);
						}
				} else {
					if (!stream->codec->codec) {
						stream->codec->codec = avcodec_find_decoder(stream->codec->codec_id);
						if (!stream->codec->codec)
							fprintf(stderr, "avcodec_find_decoder failed for subtitle track %d\n", n);
						else {
							int err = avcodec_open2(stream->codec, stream->codec->codec, NULL);
							if (averror(err, avcodec_open2))
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
	}

	return true;
}

bool Input::Stop()
{
	abortPlayback = true;

	while (hasPlayThreadStarted != 0)
		usleep(100000);

	if (avfc)
		avformat_close_input(&avfc);

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

	Track *track = videoTrack;
	if (track && track->duration != 0.0) {
		duration = track->duration / 1000.0;
		return true;
	}
	track = audioTrack;
	if (track && track->duration != 0.0) {
		duration = track->duration / 1000.0;
		return true;
	}
	track = subtitleTrack;
	if (track && track->duration != 0.0) {
		duration = track->duration / 1000.0;
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
	}
	return true;
}

bool Input::GetReadCount(uint64_t &readcount)
{
	readcount = readCount;
	return true;
}

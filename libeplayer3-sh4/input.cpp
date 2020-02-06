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

#define ENABLE_LOGGING 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "player.h"
#include "misc.h"

static const char *FILENAME = "eplayer/input.cpp";

#define averror(_err,_fun) ({										\
	if (_err < 0) {											\
		char _error[512];									\
		av_strerror(_err, _error, sizeof(_error));						\
		fprintf(stderr, "%s %d: %s: %d (%s)\n", FILENAME, __LINE__, #_fun, _err, _error);	\
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
	seek_avts_abs = INT64_MIN;
	seek_avts_rel = 0;
	abortPlayback = false;
#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
	for (int n = 0;n < EPLAYER_MAX_CODECS;n++)
		codecs[n].codec = NULL;
#endif
}

Input::~Input()
{
}

int64_t Input::calcPts(AVStream * stream, int64_t pts)
{
	if (pts == AV_NOPTS_VALUE)
		return INVALID_PTS_VALUE;

	pts = 90000 * (double)pts * stream->time_base.num / stream->time_base.den;
	if (avfc->start_time != AV_NOPTS_VALUE)
		pts -= 90000 * avfc->start_time / AV_TIME_BASE;

	if (pts < 0)
		return INVALID_PTS_VALUE;

	return pts;
}

// from neutrino-mp/lib/libdvbsubtitle/dvbsub.cpp
extern void dvbsub_write(AVSubtitle *, int64_t);
extern void dvbsub_ass_write(AVCodecContext *c, AVSubtitle *sub, int pid);
extern void dvbsub_ass_clear(void);
// from neutrino-mp/lib/lib/libtuxtxt/tuxtxt_common.h
extern void teletext_write(int pid, uint8_t *data, int size);

static std::string lastlog_message;
static unsigned int lastlog_repeats;

static void log_callback(void *ptr __attribute__ ((unused)), int lvl __attribute__ ((unused)), const char *format, va_list ap)
{
	char m[1024];
	if (sizeof(m) - 1 > (unsigned int) vsnprintf(m, sizeof(m), format, ap)) {
		if (lastlog_message.compare(m) || lastlog_repeats > 999) {
			if (lastlog_repeats)
				fprintf(stderr, "last message repeated %u times\n", lastlog_repeats);
			lastlog_message = m;
			lastlog_repeats = 0;
			fprintf(stderr, "%s", m);
		} else
			lastlog_repeats++;
	}
}

static void logprintf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_callback(NULL, 0, format, ap);
	va_end(ap);
}

AVCodecContext *Input::GetCodecContext(unsigned int index)
{
#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
	if (codecs[index].codec) {
		return codecs[index].codec;
	}
	AVCodec *codec = avcodec_find_decoder(avfc->streams[index]->codecpar->codec_id);
	codecs[index].codec = avcodec_alloc_context3(codec);
	if (!codecs[index].codec) {
		fprintf(stderr, "context3 alloc for stream %d failed\n", (int)index);
		return NULL;
	}
	if (avcodec_parameters_to_context(codecs[index].codec, avfc->streams[index]->codecpar) < 0) {
		fprintf(stderr, "copy parameters to codec context for stream %d failed\n", (int)index);
		avcodec_free_context(&codecs[index].codec);
		return NULL;
	}
	if (!codec) {
		fprintf(stderr, "decoder for codec_id:(0x%X) stream:(%d) not found\n", avfc->streams[index]->codecpar->codec_id, (int)index);;
		return codecs[index].codec;
	}
	else
	{
		fprintf(stderr, "decoder for codec_id:(0x%X) stream:(%d) found\n", avfc->streams[index]->codecpar->codec_id, (int)index);;
	}
	int err = avcodec_open2(codecs[index].codec, codec, NULL);
	if (averror(err, avcodec_open2)) {
		fprintf(stderr, "open codec context for stream:(%d) failed}n", (int)index);
		avcodec_free_context(&codecs[index].codec);
		return NULL;
	}
	return codecs[index].codec;
#else
	return avfc->streams[index]->codec;
#endif
}

bool Input::Play()
{
	hasPlayThreadStarted = 1;

	int64_t showtime = 0;
	bool restart_audio_resampling = false;
	bool bof = false;

	// HACK: Dropping all video frames until the first audio frame was seen will keep player2 from stuttering.
	//       Oddly, this seems to be necessary for network streaming only ...
	bool audioSeen = !audioTrack || !player->isHttp;

	while (player->isPlaying && !player->abortRequested) {

		//IF MOVIE IS PAUSED, WAIT
		if (player->isPaused) {
			fprintf(stderr, "paused\n");
			usleep(100000);
			continue;
		}

		int seek_target_flag = 0;
		int64_t seek_target = INT64_MIN; // in AV_TIME_BASE units

		if (seek_avts_rel) {
			if (avfc->iformat->flags & AVFMT_TS_DISCONT) {
				if (avfc->bit_rate) {
					seek_target_flag = AVSEEK_FLAG_BYTE;
					seek_target = avio_tell(avfc->pb) + av_rescale(seek_avts_rel, avfc->bit_rate, 8 * AV_TIME_BASE);
				}
			} else {
				int64_t pts;
				if(player->output.GetPts(pts))
					seek_target = av_rescale(pts, AV_TIME_BASE, 90000ll) + seek_avts_rel;
			}
			seek_avts_rel = 0;
		} else if (seek_avts_abs != INT64_MIN) {
			if (avfc->iformat->flags & AVFMT_TS_DISCONT) {
				if (avfc->bit_rate) {
					seek_target_flag = AVSEEK_FLAG_BYTE;
					seek_target = av_rescale(seek_avts_abs, avfc->bit_rate, 8 * AV_TIME_BASE);
				}
			} else {
				seek_target = seek_avts_abs;
			}
			seek_avts_abs = INT64_MIN;
		} else if (player->isBackWard && av_gettime_relative() >= showtime) {
			player->output.ClearVideo();

			if (bof) {
				showtime = av_gettime_relative();
				usleep(100000);
				continue;
			}
			seek_avts_rel = player->Speed * AV_TIME_BASE;
			showtime = av_gettime_relative() + 300000;	//jump back every 300ms
			continue;
		} else {
			bof = false;
		}

		if (seek_target > INT64_MIN) {
			int res;
			if (seek_target < 0)
				seek_target = 0;
			res = avformat_seek_file(avfc, -1, INT64_MIN, seek_target, INT64_MAX, seek_target_flag);

			if (res < 0 && player->isBackWard)
				bof = true;

			seek_target = INT64_MIN;
			restart_audio_resampling = true;

			// clear streams
			for (unsigned int i = 0; i < avfc->nb_streams; i++) {
				AVCodecContext *avcctx = GetCodecContext(i);
				if (avcctx && avcctx->codec)
					avcodec_flush_buffers(avcctx);
			}
			player->output.ClearAudio();
			player->output.ClearVideo();
		}

		AVPacket packet;
		av_init_packet(&packet);

		int err = av_read_frame(avfc, &packet);
		if (err == AVERROR(EAGAIN)) {
			av_packet_unref(&packet);
			continue;
		}
		if (averror(err, av_read_frame)) { // EOF?
			av_packet_unref(&packet);
			break;		// while
		}

		player->readCount += packet.size;

		AVStream *stream = avfc->streams[packet.stream_index];
		AVCodecContext *avcctx = GetCodecContext((unsigned int)stream->index);
		Track *_videoTrack = videoTrack;
		Track *_audioTrack = audioTrack;
		Track *_subtitleTrack = subtitleTrack;
		Track *_teletextTrack = teletextTrack;

		if (_videoTrack && (_videoTrack->stream == stream)) {
			int64_t pts = calcPts(stream, packet.pts);
			if (audioSeen && !player->output.Write(stream, &packet, pts))
				logprintf("writing data to video device failed\n");
		} else if (_audioTrack && (_audioTrack->stream == stream)) {
			if (restart_audio_resampling) {
				restart_audio_resampling = false;
				player->output.Write(stream, NULL, 0);
			}
			if (!player->isBackWard) {
				int64_t pts = calcPts(stream, packet.pts);
				//if (!player->output.Write(stream, &packet, _videoTrack ? pts : 0))	// DBO: why pts only at video tracks ?
				if (!player->output.Write(stream, &packet, pts))
					logprintf("writing data to audio device failed\n");
			}
			audioSeen = true;
		} else if (_subtitleTrack && (_subtitleTrack->stream == stream)) {
			if (avcctx->codec) {
				AVSubtitle sub;
				memset(&sub, 0, sizeof(sub));
				int got_sub_ptr = 0;

				err = avcodec_decode_subtitle2(avcctx, &sub, &got_sub_ptr, &packet);
				averror(err, avcodec_decode_subtitle2);

				if (got_sub_ptr && sub.num_rects > 0) {
					switch (sub.rects[0]->type) {
						case SUBTITLE_TEXT: // FIXME?
						case SUBTITLE_ASS:
							dvbsub_ass_write(avcctx, &sub, _subtitleTrack->pid);
							break;
						case SUBTITLE_BITMAP: {
							int64_t pts = calcPts(stream, packet.pts);
							dvbsub_write(&sub, pts);
							// avsubtitle_free() will be called by handler
							break;
						}
						default:
							break;
					}
				}
			}
		} else if (_teletextTrack && (_teletextTrack->stream == stream)) {
			if (packet.data && packet.size > 1)
				teletext_write(_teletextTrack->pid, packet.data + 1, packet.size - 1);
		}

		av_packet_unref(&packet);

	} /* while */

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
		fprintf(stderr, "%s %s %d: abort requested (%d/%d)\n", FILENAME, __func__, __LINE__, player->input.abortPlayback, player->abortRequested);
	return res;
}

#if LIBAVCODEC_VERSION_MAJOR < 58
static int lock_callback(void **mutex, enum AVLockOp op)
{
	switch (op) {
		case AV_LOCK_CREATE:
			*mutex = (void *) new OpenThreads::Mutex;
			return !*mutex;
		case AV_LOCK_DESTROY:
			delete static_cast<OpenThreads::Mutex *>(*mutex);
			*mutex = NULL;
			return 0;
		case AV_LOCK_OBTAIN:
			static_cast<OpenThreads::Mutex *>(*mutex)->lock();
			return 0;
		case AV_LOCK_RELEASE:
			static_cast<OpenThreads::Mutex *>(*mutex)->unlock();
			return 0;
		default:
			return -1;
	}
}
#endif

bool Input::ReadSubtitle(const char *filename, const char *format, int pid)
{
	const char *lastDot = strrchr(filename, '.');
	if (!lastDot)
		return false;
	char subfile[strlen(filename) + strlen(format)];
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

	AVCodecContext *c = NULL;
	AVCodec *codec = NULL;
#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT( 57,25,101 ))
	c = subavfc->streams[0]->codec;
#else
	c = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(c, subavfc->streams[0]->codecpar) < 0) {
		avcodec_free_context(&c);
		avformat_close_input(&subavfc);
		avformat_free_context(subavfc);
		return false;
	}
#endif
	codec = avcodec_find_decoder(c->codec_id);
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

	while (av_read_frame(subavfc, &packet) > -1) {
		AVSubtitle sub;
		memset(&sub, 0, sizeof(sub));
		int got_sub = 0;
		avcodec_decode_subtitle2(c, &sub, &got_sub, &packet);
		if (got_sub)
			dvbsub_ass_write(c, &sub, pid);
		av_packet_unref(&packet);
	}
	avcodec_close(c);
#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
	avcodec_free_context(&c);
#endif
	avformat_close_input(&subavfc);
	avformat_free_context(subavfc);

	Track track;
	track.title = format;
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

bool Input::Init(const char *filename, std::string headers)
{
	bool find_info = true;
	abortPlayback = false;
#if LIBAVCODEC_VERSION_MAJOR < 58
	av_lockmgr_register(lock_callback);
#endif
#if ENABLE_LOGGING
	av_log_set_flags(AV_LOG_SKIP_REPEATED);
	av_log_set_level(AV_LOG_INFO);
	/* out commented here for using ffmpeg default: av_log_default_callback
	because of better log level handling */
	//av_log_set_callback(log_callback);
#else
	av_log_set_level(AV_LOG_PANIC);
#endif

	if (!filename) {
		fprintf(stderr, "filename NULL\n");
		return false;
	}

	if (!headers.empty())
	{
		fprintf(stderr, "%s %s %d: %s\n%s\n", FILENAME, __func__, __LINE__, filename, headers.c_str());
		headers += "\r\n";
	}
	else
	{
		fprintf(stderr, "%s %s %d: %s\n", FILENAME, __func__, __LINE__, filename);
	}

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	avcodec_register_all();
	av_register_all();
#endif
	avformat_network_init();

	videoTrack = NULL;
	audioTrack = NULL;
	subtitleTrack = NULL;
	teletextTrack = NULL;

#if 0
again:
#endif
	avfc = avformat_alloc_context();
	avfc->interrupt_callback.callback = interrupt_cb;
	avfc->interrupt_callback.opaque = (void *) player;

	AVDictionary *options = NULL;
	av_dict_set(&options, "auth_type", "basic", 0);
	if (!headers.empty())
	{
		av_dict_set(&options, "headers", headers.c_str(), 0);
	}
#if ENABLE_LOGGING
	av_log_set_level(AV_LOG_DEBUG);
#endif
	int err = avformat_open_input(&avfc, filename, NULL, &options);
#if ENABLE_LOGGING
	av_log_set_level(AV_LOG_INFO);
#endif
	av_dict_free(&options);
	if (averror(err, avformat_open_input)) {
		avformat_free_context(avfc);
		return false;
	}

	avfc->iformat->flags |= AVFMT_SEEK_TO_PTS;
	avfc->flags = AVFMT_FLAG_GENPTS;
	if (player->noprobe) {
#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(55, 43, 100)) || \
	(LIBAVFORMAT_VERSION_INT > AV_VERSION_INT(57, 25, 0))
		avfc->max_analyze_duration = 1;
#else
		avfc->max_analyze_duration2 = 1;
#endif
		avfc->probesize = 131072;
	}
#if 0
	if (!player->isHttp)
	{
		for (unsigned int i = 0; i < avfc->nb_streams; i++) {
			if (avfc->streams[i]->codec->codec_id == AV_CODEC_ID_AAC)
				find_info = false;
		}
	}
#endif
	if (find_info)
		err = avformat_find_stream_info(avfc, NULL);

#if 0
	if (averror(err, avformat_find_stream_info)) {
		avformat_close_input(&avfc);
		if (player->noprobe) {
			player->noprobe = false;
			goto again;
		}
		return false;
	}
#endif

	bool res = UpdateTracks();

	if (!videoTrack && !audioTrack) {
		avformat_close_input(&avfc);
		return false;
	}

	if (videoTrack)
		player->output.SwitchVideo(videoTrack);
	if (audioTrack)
		player->output.SwitchAudio(audioTrack);

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
		chapter.start = av_rescale(ch->time_base.num * AV_TIME_BASE, ch->start, ch->time_base.den);
		chapter.end = av_rescale(ch->time_base.num * AV_TIME_BASE, ch->end, ch->time_base.den);
		chapters.push_back(chapter);
	}
	player->SetChapters(chapters);

	player->manager.initTrackUpdate();

	av_dump_format(avfc, 0, player->url.c_str(), 0);

	bool use_index_as_pid = false;
	for (unsigned int n = 0; n < avfc->nb_streams; n++) {
		AVStream *stream = avfc->streams[n];

		AVCodecContext *avcctx = GetCodecContext(n);

		Track track;
		track.stream = stream;
		AVDictionaryEntry *lang = av_dict_get(stream->metadata, "language", NULL, 0);
		track.title = lang ? lang->value : "";

		if (!use_index_as_pid)
			switch (avcctx->codec_type) {
				case AVMEDIA_TYPE_VIDEO:
				case AVMEDIA_TYPE_AUDIO:
				case AVMEDIA_TYPE_SUBTITLE:
					if (!stream->id)
						use_index_as_pid = true;
				default:
					break;
			}

		track.pid = use_index_as_pid ? n + 1: stream->id;
		track.ac3flags = 0;

		switch (avcctx->codec_type) {
			case AVMEDIA_TYPE_VIDEO:
				player->manager.addVideoTrack(track);
				if (!videoTrack)
					videoTrack = player->manager.getVideoTrack(track.pid);
				break;
			case AVMEDIA_TYPE_AUDIO:
				switch(avcctx->codec_id) {
					case AV_CODEC_ID_MP2:
						track.ac3flags = 1;
						break;
					case AV_CODEC_ID_MP3:
						track.ac3flags = 2;
						break;
					case AV_CODEC_ID_AC3:
						track.ac3flags = 3;
						break;
					case AV_CODEC_ID_DTS:
						track.ac3flags = 4;
						break;
					case AV_CODEC_ID_AAC: {
						unsigned int extradata_size = avcctx->extradata_size;
						unsigned int object_type = 2;
						if(extradata_size >= 2)
							object_type = avcctx->extradata[0] >> 3;
						if (extradata_size <= 1 || object_type == 1 || object_type == 5) {
							fprintf(stderr, "use resampling for AAC\n");
							track.ac3flags = 6;
						}
						else
							track.ac3flags = 5;
						break;
					}
					case AV_CODEC_ID_FLAC:
						track.ac3flags = 8;
						break;
					case AV_CODEC_ID_WMAV1:
					case AV_CODEC_ID_WMAV2:
					case AV_CODEC_ID_WMAVOICE:
					case AV_CODEC_ID_WMAPRO:
					case AV_CODEC_ID_WMALOSSLESS:
						track.ac3flags = 9;
						break;
					default:
						track.ac3flags = 0;
				}
				player->manager.addAudioTrack(track);
				if (!audioTrack)
					audioTrack = player->manager.getAudioTrack(track.pid);
				break;
			case AVMEDIA_TYPE_SUBTITLE:
				if (avcctx->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
					std::string l = lang ? lang->value : "";
					uint8_t *data = avcctx->extradata;
					int size = avcctx->extradata_size;
					if (size > 0 && 2 * size - 1 == (int) l.length())
						for (int i = 0; i < size; i += 2) {
							track.title = l.substr(i * 2, 3);
							track.type = data[i] >> 3;
							track.mag = data[i] & 7;
							track.page = data[i + 1];
							player->manager.addTeletextTrack(track);
						}
				} else {
					if (!avcctx->codec) {
						avcctx->codec = avcodec_find_decoder(avcctx->codec_id);
						if (!avcctx->codec)
							fprintf(stderr, "avcodec_find_decoder failed for subtitle track %d\n", n);
						else {
							int err = avcodec_open2(avcctx, avcctx->codec, NULL);
							if (averror(err, avcodec_open2))
								avcctx->codec = NULL;
						}
					}
					if (avcctx->codec)
						player->manager.addSubtitleTrack(track);
				}
				break;
			default:
				fprintf(stderr, "not handled or unknown codec_type %d\n", avcctx->codec_type);
				break;
		}
	}

	for (unsigned int n = 0; n < avfc->nb_programs; n++) {
		AVProgram *p = avfc->programs[n];
		if (p->nb_stream_indexes) {
			AVDictionaryEntry *name = av_dict_get(p->metadata, "name", NULL, 0);
			Program program;
			program.title = name ? name->value : "";
			program.id = p->id;
			for (unsigned m = 0; m < p->nb_stream_indexes; m++)
				program.streams.push_back(avfc->streams[p->stream_index[m]]);
			player->manager.addProgram(program);
		}
	}

	return true;
}

bool Input::Stop()
{
	abortPlayback = true;

	while (hasPlayThreadStarted != 0)
		usleep(100000);

	av_log(NULL, AV_LOG_QUIET, "%s", "");

	if (avfc) {
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
		for (unsigned int i = 0; i < avfc->nb_streams; i++) {
#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
			if (codecs[i].codec)
				avcodec_free_context(&codecs[i].codec);
#else
			avcodec_close(avfc->streams[i]->codec);
#endif
		}
		avformat_close_input(&avfc);
	}

	avformat_network_deinit();

	return true;
}

AVFormatContext *Input::GetAVFormatContext()
{
	mutex.lock();
	if (avfc)
		return avfc;
	mutex.unlock();
	return NULL;
}

void Input::ReleaseAVFormatContext()
{
	if (avfc)
		mutex.unlock();
}

bool Input::Seek(int64_t avts, bool absolute)
{
	if (absolute)
		seek_avts_abs = avts, seek_avts_rel = 0;
	else
		seek_avts_abs = INT64_MIN, seek_avts_rel = avts;
	return true;
}

bool Input::GetDuration(int64_t &duration)
{
	if (avfc) {
		duration = avfc->duration;
		return true;
	}
	duration = 0;
	return false;
}

bool Input::SwitchAudio(Track *track)
{
	audioTrack = track;
	player->output.SwitchAudio(track ? track : NULL);
	// player->Seek(-5000, false);
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
	player->output.SwitchVideo(track ? track : NULL);
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

		// find the first attached picture, if available
		for(unsigned int i = 0; i < avfc->nb_streams; i++) {
			if (avfc->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC) {
			AVPacket *pkt = &avfc->streams[i]->attached_pic;
			FILE *cover_art = fopen("/tmp/.id3coverart", "wb");
			if (cover_art) {
				fwrite(pkt->data, pkt->size, 1, cover_art);
				fclose(cover_art);
			}
			av_packet_unref(pkt);
			break;
			}
		}
	}
	return true;
}

bool Input::GetReadCount(uint64_t &readcount)
{
	readcount = readCount;
	return true;
}

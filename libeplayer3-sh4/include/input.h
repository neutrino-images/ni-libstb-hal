/*
 * input class
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

#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
#define EPLAYER_MAX_CODECS 16
struct CodecList
{
	AVCodecContext *codec;
};
#endif

class Player;
class Track;

class Input
{
	friend class Player;
	friend class WriterPCM; // needs calcPts()
	friend int interrupt_cb(void *arg);

	private:
		OpenThreads::Mutex mutex;

		Track *videoTrack;
		Track *audioTrack;
		Track *subtitleTrack;
		Track *teletextTrack;

		int hasPlayThreadStarted;
		int64_t seek_avts_abs;
		int64_t seek_avts_rel;
		bool isContainerRunning;
		bool abortPlayback;

		Player *player;
		AVFormatContext *avfc;
#if (LIBAVFORMAT_VERSION_INT > AV_VERSION_INT( 57,25,100 ))
		CodecList codecs[EPLAYER_MAX_CODECS];
#endif
		uint64_t readCount;
		int64_t calcPts(AVStream * stream, int64_t pts);

	public:
		Input();
		~Input();

		bool ReadSubtitle(const char *filename, const char *format, int pid);
		bool ReadSubtitles(const char *filename);
		bool Init(const char *filename, std::string headers = "");
		bool UpdateTracks();
		bool Play();
		bool Stop();
		bool Seek(int64_t sec, bool absolute);
		bool GetDuration(int64_t &duration);
		bool SwitchAudio(Track *track);
		bool SwitchSubtitle(Track *track);
		bool SwitchTeletext(Track *track);
		bool SwitchVideo(Track *track);
		bool GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
		bool GetReadCount(uint64_t &readcount);
		AVFormatContext *GetAVFormatContext();
		AVCodecContext *GetCodecContext(unsigned int index);
		void ReleaseAVFormatContext();
};

#endif

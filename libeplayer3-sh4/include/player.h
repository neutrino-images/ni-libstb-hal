/*
 * player class
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

#ifndef __PLAYER_H__
#define __PLAYER_H__

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

#include <pthread.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <map>

#include "input.h"
#include "output.h"
#include "manager.h"

struct Chapter
{
	std::string title;
	int64_t start;
	int64_t end;
};

class Player
{
		friend class Input;
		friend class Output;
		friend class Manager;
		friend class cPlayback;
		friend class WriterPCM;
		friend int interrupt_cb(void *arg);

	private:
		Input input;
		Output output;
		Manager manager;
		OpenThreads::Mutex chapterMutex;
		std::vector<Chapter> chapters;
		pthread_t playThread;

		bool abortRequested;
		bool isHttp;
		bool isPaused;
		bool isSlowMotion;
		bool hasThreadStarted;
		bool isForwarding;
		bool isBackWard;
		bool isPlaying;

		int Speed;

		uint64_t readCount;

		std::string url;
		bool noprobe;   /* hack: only minimal probing in av_find_stream_info */

		void SetChapters(std::vector<Chapter> &Chapters);
		static void *playthread(void *);
	public:
		bool SwitchAudio(int pid);
		bool SwitchVideo(int pid);
		bool SwitchTeletext(int pid);
		bool SwitchSubtitle(int pid);

		int GetAudioPid();
		int GetVideoPid();
		int GetSubtitlePid();
		int GetTeletextPid();

		bool GetPts(int64_t &pts);
		bool GetFrameCount(int64_t &framecount);
		bool GetDuration(int64_t &duration);

		bool GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
		bool SlowMotion(int repeats);
		bool FastBackward(int speed);
		bool FastForward(int speed);

		bool Open(const char *Url, bool noprobe = false, std::string headers = "");
		bool Close();
		bool Play();
		bool Pause();
		bool Continue();
		bool Stop();
		bool Seek(int64_t pos, bool absolute);
		void RequestAbort();
		bool GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);

		AVFormatContext *GetAVFormatContext()
		{
			return input.GetAVFormatContext();
		}
		void ReleaseAVFormatContext()
		{
			input.ReleaseAVFormatContext();
		}

		bool GetPrograms(std::vector<std::string> &keys, std::vector<std::string> &values);
		bool SelectProgram(int key);
		bool SelectProgram(std::string &key);

		Player();
};
#endif

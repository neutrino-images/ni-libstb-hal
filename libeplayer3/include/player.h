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
#include "playback.h"
#include "player.h"

struct Chapter
{
        std::string title;
        double start;
        double end;
};

class Player {
	friend class Input;
	friend class Output;
	friend class Manager;
	friend class Playback;
	friend class cPlayback;
	private:
		Input input;
		Output output;
		Manager manager;
		Playback playback;
		OpenThreads::Mutex chapterMutex;
		std::vector<Chapter> chapters;
	public: //FIXME
		int64_t *currentAudioPtsP;
	public:
		Player()
		{
			input.context = this;
			output.context = this;
			playback.context = this;
			manager.context = this;
		}

		bool GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
		void SetChapters(std::vector<Chapter> &Chapters);
};
#endif

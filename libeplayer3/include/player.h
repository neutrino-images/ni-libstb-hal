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
	double start;
	double end;
};

class Player {
	friend class Input;
	friend class Output;
	friend class Manager;
	friend class cPlayback;
	friend int interrupt_cb(void *arg);

	private:
		Input input;
		Output output;
		Manager manager;
		OpenThreads::Mutex chapterMutex;
		std::vector<Chapter> chapters;
		bool abortPlayback;
		bool abortRequested;
		bool isHttp;
		bool isPaused;
		bool isCreationPhase;
		bool isSlowMotion;
		int Speed;
		int AVSync;

		bool isVideo;
		bool isAudio;

		std::string url;
		bool noprobe;	/* hack: only minimal probing in av_find_stream_info */

		int hasThreadStarted;

		static void* SupervisorThread(void*);
	public:
		bool isForwarding;
		bool isBackWard;
		bool isPlaying;
		uint64_t readCount;

		bool SwitchAudio(int pid);
		bool SwitchVideo(int pid);
		bool SwitchTeletext(int pid);
		bool SwitchSubtitle(int pid);
		bool GetPts(int64_t &pts);
		bool GetFrameCount(int64_t &framecount);
		bool GetDuration(double &duration);
		bool GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
		bool SlowMotion(int repeats);
		bool FastBackward(int speed);
		bool FastForward(int speed);
		bool Open(const char *Url);
		bool Close();
		bool Play();
		bool Pause();
		bool Continue();
		bool Stop();
		bool Seek(float pos, bool absolute);
		bool Terminate();
		bool GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
		void SetChapters(std::vector<Chapter> &Chapters);
		Player();

};
#endif

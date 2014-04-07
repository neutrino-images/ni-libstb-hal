#ifndef __PLAYBACK_H__
#define __PLAYBACK_H__

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

class Player;
class Input;

class Playback {
	friend class Player;
	friend class Input;

	private:
		Player *context;

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

	public:
		bool isForwarding;
		bool isBackWard;
		bool isPlaying;
		bool abortPlayback;
		bool abortRequested;
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

		static void* SupervisorThread(void*);

		Playback();
};



#endif

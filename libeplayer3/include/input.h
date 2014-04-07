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

class Player;
class Track;

class Input
{
	friend class Player;

	private:
		Track *videoTrack;
		Track *audioTrack;
		Track *subtitleTrack;
		Track *teletextTrack;

		int hasPlayThreadStarted;
		float seek_sec_abs;
		float seek_sec_rel;
		bool isContainerRunning;
		bool terminating;

		Player *player;
		AVFormatContext *avfc;
		uint64_t readCount;
	public:
		Input();
		~Input();

		bool ReadSubtitle(const char *filename, const char *format, int pid);
		bool ReadSubtitles(const char *filename);
		bool Init(const char *filename);
		bool UpdateTracks();
		bool Play();
		bool Stop();
		bool Seek(float sec, bool absolute);
		bool GetDuration(double &duration);
		bool SwitchAudio(Track *track);
		bool SwitchSubtitle(Track *track);
		bool SwitchTeletext(Track *track);
		bool SwitchVideo(Track *track);
		bool GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
		bool GetReadCount(uint64_t &readcount);
};

#endif

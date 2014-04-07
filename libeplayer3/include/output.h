#ifndef __OUTPUT_H__
#define __OUTPUT_H__

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

#include "writer.h"

class Player;

class Output
{
	friend class Player;

	private:
		int videofd;
		int audiofd;
		Writer *videoWriter, *audioWriter;
		OpenThreads::Mutex audioMutex, videoMutex;
		AVStream *audioStream, *videoStream;
		Player *player;
	public:
		Output();
		~Output();
		bool Open();
		bool Close();
		bool Play();
		bool Stop();
		bool Pause();
		bool Continue();
		bool Mute(bool);
		bool Flush();
		bool FastForward(int speed);
		bool SlowMotion(int speed);
		bool AVSync(bool);
		bool Clear();
		bool ClearAudio();
		bool ClearVideo();
		bool GetPts(int64_t &pts);
		bool GetFrameCount(int64_t &framecount);
		bool SwitchAudio(AVStream *stream);
		bool SwitchVideo(AVStream *stream);
		bool Write(AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t &Pts);
};

#endif

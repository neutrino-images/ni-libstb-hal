#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdio.h>
#include <stdint.h>


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

class Output
{
	private:
		int videofd;
		int audiofd;
		Writer *videoWriter, *audioWriter;
		OpenThreads::Mutex audioMutex, videoMutex;
		AVStream *audioStream, *videoStream;
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

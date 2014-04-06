#ifndef MANAGER_H_
#define MANAGER_H_

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <map>
#include <vector>

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

typedef enum {
    MANAGER_ADD,
    MANAGER_LIST,
    MANAGER_GET,
    MANAGER_GETNAME,
    MANAGER_SET,
    MANAGER_DEL,
    MANAGER_GET_TRACK,
    MANAGER_INIT_UPDATE
} ManagerCmd_t;

struct Track
{
	std::string Name;
	int pid;

	/* length of track */
	int64_t duration;

	/* context from ffmpeg */
	AVFormatContext *avfc;
	/* stream from ffmpeg */
	AVStream *stream;

	bool inactive;
	bool is_static;

	int ac3flags;
	int type, mag, page; // for teletext

	Track() : pid(-1), duration(-1), avfc(NULL), stream(NULL), inactive(0), is_static(0), ac3flags(-1) {}
};

class Manager
{
	private:
		OpenThreads::Mutex mutex;
		std::map<int,Track *> videoTracks, audioTracks, subtitleTracks, teletextTracks;
	public:
		void addVideoTrack(Track &track);
		void addAudioTrack(Track &track);
		void addSubtitleTrack(Track &track);
		void addTeletextTrack(Track &track);

		std::vector<Track> getVideoTracks();
		std::vector<Track> getAudioTracks();
		std::vector<Track> getSubtitleTracks();
		std::vector<Track> getTeletextTracks();

		Track *getVideoTrack(int pid);
		Track *getAudioTrack(int pid);
		Track *getSubtitleTrack(int pid);
		Track *getTeletextTrack(int pid);

		bool initTrackUpdate();
		void clearTracks();

		~Manager();
};
#endif

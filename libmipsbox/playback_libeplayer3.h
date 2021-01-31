#ifndef __PLAYBACK_LIBEPLAYER3_H__
#define __PLAYBACK_LIBEPLAYER3_H__

#include <string>
#include <vector>
#include <OpenThreads/Mutex>

typedef enum
{
	PLAYMODE_TS = 0,
	PLAYMODE_FILE
} playmode_t;

struct AVFormatContext;

class cPlayback
{
	friend class CStreamInfo2;

	private:
		static OpenThreads::Mutex mutex;
		bool enabled;
		bool playing, first;
		bool no_probe;
		bool got_vpts_ts;
		int nPlaybackSpeed;
		int mAudioStream;
		int mSubtitleStream;
		int mTeletextStream;
		int64_t vpts_ts;
		bool Stop(void);
		bool decoders_closed;
		playmode_t pm;
		std::string fn_ts;
		std::string fn_xml;
		off64_t last_size;
		int init_jump;
		AVFormatContext *avft;
	public:
		cPlayback(int num = 0);
		~cPlayback();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, int vpid, int vtype, int apid, int ac3, int duration, std::string headers = "", std::string filename2 = "");
		bool Start(std::string filename, std::string headers = "", std::string filename2 = "");
		bool SetAPid(int pid, bool ac3 = false);
		bool SetVPid(int /*pid*/);
		bool SetSubtitlePid(int pid);
		bool SetTeletextPid(int pid);
		int GetAPid(void) { return mAudioStream; }
		int GetVPid(void) { return 0; }
		int GetSubtitlePid(void) { return mSubtitleStream; }
		int GetTeletextPid(void);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		void GetPts(uint64_t &pts);
		bool SetPosition(int position, bool absolute = false);
		void FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language);
		void FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language);
		void FindAllTeletextsubtitlePids(int */*pids*/, unsigned int *numpidt, std::string */*tlanguage*/, int */*mags*/, int */*pages*/);
		void RequestAbort(void);
		bool IsPlaying(void);
		uint64_t GetReadCount(void);

		void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
		void GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);

		AVFormatContext *GetAVFormatContext();
		void ReleaseAVFormatContext();
#if 0
		void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language);
		bool SelectSubtitles(int pid);

		// Functions that are not used by movieplayer.cpp:
		bool GetOffset(off64_t &offset);
		bool IsPlaying(void) const;
		bool IsEnabled(void) const;
		void *GetHandle(void);
		void *GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const;
		void PlaybackNotify(int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
#endif
};

#endif // __PLAYBACK_LIBEPLAYER3_H__

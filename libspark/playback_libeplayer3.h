#ifndef __HAL_PLAYBACK_H
#define __HAL_PLAYBACK_H

#include <string>
#include <vector>

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE
} playmode_t;

class cPlayback
{
	private:
		bool enabled;
		bool playing;
		int nPlaybackSpeed;
		int mAudioStream;
		int mSubtitleStream;
		int mDvbsubtitleStream;
		int mTeletextStream;
		void (*framebuffer_callback)(uint32_t **, unsigned int *, unsigned int *, unsigned int *, void (**)(void));
		bool Stop(void);
	public:
		cPlayback(int num = 0, void (*fbcb)(uint32_t **, unsigned int *, unsigned int *, unsigned int *, void (**)(void)) = NULL);
		~cPlayback();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, int vpid, int vtype, int apid,
			   int ac3, unsigned int duration, bool no_probe = true);
		bool SetAPid(int pid, bool ac3);
		bool SetSubtitlePid(int pid);
		bool SetDvbsubtitlePid(int pid);
		bool SetTeletextPid(int pid);
		int GetAPid(void) { return mAudioStream; }
		int GetSubtitlePid(void) { return mSubtitleStream; }
		int GetDvbsubtitlePid(void) { return mDvbsubtitleStream; }
		int GetTeletextPid(void);
		void SuspendSubtitle(bool);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		void GetPts(uint64_t &pts);
		bool SetPosition(int position, bool absolute = false);
		void FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language);
		void FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language);
		void FindAllDvbsubtitlePids(int *pids, unsigned int *numpids, std::string *language);
		void FindAllTeletextsubtitlePids(int *pids, unsigned int *numpidt, std::string *tlanguage);
		void RequestAbort(void);
		bool isPlaying(void);
#if 0
		void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language);
		bool SelectSubtitles(int pid);
		void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
#endif
#if 0
		// Functions that are not used by movieplayer.cpp:
		bool GetOffset(off64_t &offset);
		bool IsPlaying(void) const;
		bool IsEnabled(void) const;
		void * GetHandle(void);
		void * GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const;
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
#endif
};
#endif

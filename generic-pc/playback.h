#ifndef __PLAYBACK_H
#define __PLAYBACK_H

#include <string>
#include <stdint.h>
#include <vector>

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t; 

class cPlayback
{
	private:
		bool playing;
		int mAudioStream;
		int mSubtitleStream;
		int mDvbsubtitleStream;
		int mTeletextStream;
		void (*framebuffer_callback)(uint32_t **, unsigned int *, unsigned int *, unsigned int *, void (**)(void));
	public:
		cPlayback(int, void (*)(uint32_t **, unsigned int *, unsigned int *, unsigned int *, void (**)(void)) = NULL) { };
		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char * filename, int vpid, int vtype, int apid, bool ac3, int duration, bool no_probe = true);
		bool Stop(void);
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
		void FindAllDvbsubtitlePids(int *pids, unsigned int *numpidd, std::string *language);
		void FindAllTeletextsubtitlePids(int *pids, unsigned int *numpidt, std::string *tlanguage);
		void RequestAbort(void);
		bool IsPlaying(void) { return false; }
		void FindAllSubs(int *pids, unsigned int *supported, unsigned int *numpida, std::string *language);
		bool SelectSubtitles(int pid);
		void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
		//
		cPlayback(int num = 0);
		~cPlayback();
};

#endif

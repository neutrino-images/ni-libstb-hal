#ifndef __HAL_PLAYBACK_H
#define __HAL_PLAYBACK_H

#include <string>

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
#ifdef MARTII
		int mSubtitleStream;
		int mDvbsubtitleStream;
		int mTeletextStream;
		void (*framebuffer_callback)(unsigned char **, unsigned int *, unsigned int *, unsigned int *, int *);
#endif
		bool Stop(void);
	public:
#ifdef MARTII
		cPlayback(int num = 0, void (*fbcb)(unsigned char **, unsigned int *, unsigned int *, unsigned int *, int *) = NULL);
#else
		cPlayback(int num = 0);
#endif
		~cPlayback();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid,
#ifdef MARTII
			   int ac3, unsigned int duration, bool no_probe = true);
#else
			   int ac3, unsigned int duration);
#endif
		bool SetAPid(unsigned short pid, bool ac3);
#ifdef MARTII
		bool SetSubtitlePid(unsigned short pid);
		bool SetDvbsubtitlePid(unsigned short pid);
		bool SetTeletextPid(unsigned short pid);
		unsigned short GetAPid(void) { return mAudioStream; }
		unsigned short GetSubtitlePid(void) { return mSubtitleStream; }
		unsigned short GetDvbsubtitlePid(void) { return mDvbsubtitleStream; }
		unsigned short GetTeletextPid(void) { return mTeletextStream; }
		void SuspendSubtitle(bool);
#endif
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		bool SetPosition(int position, bool absolute = false);
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
#ifdef MARTII
		void FindAllSubtitlePids(uint16_t *pids, uint16_t *numpids, std::string *language);
		void FindAllDvbsubtitlePids(uint16_t *pids, uint16_t *numpids, std::string *language);
		void FindAllTeletextsubtitlePids(uint16_t *pids, uint16_t *numpidt, std::string *tlanguage);

		void RequestAbort(void);
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

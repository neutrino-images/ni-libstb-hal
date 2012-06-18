#ifndef __PLAYBACK_H
#define __PLAYBACK_H

#include <string>
#include <stdint.h>

#ifndef CS_PLAYBACK_PDATA
typedef struct {
	int nothing;
} CS_PLAYBACK_PDATA;
#endif

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t; 

class cPlayback
{
	private:
		int timeout;
		pthread_cond_t read_cond;
		pthread_mutex_t mutex;
		CS_PLAYBACK_PDATA * privateData;

		pthread_t rua_thread;
		pthread_t event_thread;

		bool enabled;
		bool paused;
		bool playing;
		int unit;
		int nPlaybackFD;
		int video_type;
		int nPlaybackSpeed;
		int mSpeed;
		int mAudioStream;
		int mSubStream;
		char* mfilename;
		int thread_active;
		int eof_reached;
		int setduration;
		int mduration;
		
		playmode_t playMode;
		//
	public:
		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3, int duration);
		bool Stop(void);
		bool SetAPid(unsigned short pid, bool ac3);
		bool SetSPid(int pid);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		bool SetPosition(int position, bool absolute = false);
		bool IsEOF(void) const;
		int GetCurrPlaybackSpeed(void) const;
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
		void FindAllSPids(int *spids, uint16_t *numpids, std::string *language);
		//
		cPlayback(int num = 0);
		~cPlayback();
		void RuaThread();
		void EventThread();

#if 0
		/* not used */
		bool GetOffset(off64_t &offset);
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
		bool IsPlaying(void) const;
		bool IsEnabled(void) const;
		void * GetHandle(void);
		void * GetDmHandle(void);
#endif
};

#endif

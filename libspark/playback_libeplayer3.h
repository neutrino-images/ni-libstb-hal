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
		bool Stop(void);
	public:
		cPlayback(int num = 0);
		~cPlayback();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid,
			   int ac3, unsigned int duration);
		bool SetAPid(unsigned short pid, bool ac3);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		bool SetPosition(int position, bool absolute = false);
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
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

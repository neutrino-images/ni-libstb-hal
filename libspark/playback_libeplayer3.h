/*******************************************************************************/
/*                                                                             */
/* libcoolstream/cszapper/demux.h                                              */
/*   ZAP interface for neutrino frontend                                       */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
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
		int timeout;
		pthread_cond_t read_cond;
		pthread_mutex_t mutex;
		bool enabled;
		bool paused;
		bool playing;
		int unit;
		int nPlaybackFD;
		int video_type;
		int nPlaybackSpeed;
		int mSpeed;
		int mAudioStream;
		playmode_t playMode;
		//
		void Attach(void);
		void Detach(void);
		bool SetAVDemuxChannel(bool On, bool Video = true, bool Audio = true);
	public:
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid,
			   int ac3, unsigned int duration);
		bool Stop(void);
		bool SetAPid(unsigned short pid, bool ac3);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		bool GetOffset(off64_t &offset);
		bool SetPosition(int position, bool absolute = false);
		bool IsPlaying(void) const;
		bool IsEnabled(void) const;
		void * GetHandle(void);
		void * GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const;
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
		//
		cPlayback(int num = 0);
		~cPlayback();

};

#endif

#ifndef __PLAYBACK_LIB_H_
#define __PLAYBACK_LIB_H_

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
		pthread_mutex_t rmfp_cmd_mutex;
		int playing;
		bool eof_reached;
		int playback_speed;
		playmode_t playMode;
		bool open_success;
		uint16_t apid;
		uint16_t subpid;
		char *mfilename;
		int mduration;
		pthread_t thread;
		bool thread_started;
		/* private functions */
		bool rmfp_command(int cmd, int param, bool has_param, char *buf, int buflen);
	public:
		cPlayback(int num = 0);
		~cPlayback();

		void run_rmfp();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid,
			   int ac3, unsigned int duration);
		bool SetAPid(unsigned short pid, int ac3);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);	/* pos: current time in ms, dur: file length in ms */
		bool SetPosition(int position, bool absolute = false);	/* position: jump in ms */
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
		void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language);
		bool SelectSubtitles(int pid);
		void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
#if 0
		// Functions that are not used by movieplayer.cpp:
		bool Stop(void);
		bool GetOffset(off64_t &offset);
		bool IsPlaying(void) const { return playing; }
		bool IsEnabled(void) const { return enabled; }
		void * GetHandle(void);
		void * GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const { return nPlaybackSpeed; }
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
#endif
};
#endif // __PLAYBACK_LIB_H_

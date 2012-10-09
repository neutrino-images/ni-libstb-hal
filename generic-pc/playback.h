#ifndef __PLAYBACK_H
#define __PLAYBACK_H

#include <string>
#include <stdint.h>

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t; 

class cPlayback
{
	private:
		bool playing;
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
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
		void FindAllSPids(int *spids, uint16_t *numpids, std::string *language);
		//
		cPlayback(int num = 0);
		~cPlayback();
};

#endif

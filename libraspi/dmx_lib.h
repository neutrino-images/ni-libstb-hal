#ifndef __DMX_LIB_H__
#define __DMX_LIB_H__

#include <cstdlib>
#include <vector>
#include <inttypes.h>
#include <linux/dvb/dmx.h>
#include "../common/cs_types.h"

#define MAX_DMX_UNITS 4

typedef enum
{
	DMX_INVALID = 0,
	DMX_VIDEO_CHANNEL = 1,
	DMX_AUDIO_CHANNEL,
	DMX_PES_CHANNEL,
	DMX_PSI_CHANNEL,
	DMX_PIP_CHANNEL,
	DMX_TP_CHANNEL,
	DMX_PCR_ONLY_CHANNEL
} DMX_CHANNEL_TYPE;

typedef struct
{
	int fd;
	unsigned short pid;
} pes_pids;

class cRecord;
class cPlayback;
class cDemux
{
		friend class cRecord;
		friend class cPlayback;
	public:
		bool Open(DMX_CHANNEL_TYPE pes_type, void *x = NULL, int y = 0);
		void Close(void);
		bool Start(bool record = false);
		bool Stop(void);
		int Read(unsigned char *buff, int len, int Timeout = 0);
		bool sectionFilter(unsigned short pid, const unsigned char *const filter, const unsigned char *const mask, int len, int Timeout = 0, const unsigned char *const negmask = NULL);
		bool pesFilter(const unsigned short pid);
		void SetSyncMode(AVSYNC_TYPE mode);
		void *getBuffer();
		void *getChannel();
		DMX_CHANNEL_TYPE getChannelType(void)
		{
			return dmx_type;
		};
		bool addPid(unsigned short pid);
		void getSTC(int64_t *STC);
		int getUnit(void);
		static bool SetSource(int unit, int source);
		static int GetSource(int unit);
		int getFD(void)
		{
			return fd;
		};     /* needed by cPlayback class */
		cDemux(int num = 0);
		~cDemux();

	private:
		void removePid(unsigned short Pid); /* needed by cRecord class */
		int num;
		int fd;
		int buffersize;
		uint16_t pid;
		uint8_t flt;
		std::vector<pes_pids> pesfds;
		DMX_CHANNEL_TYPE dmx_type;
		void *pdata;
};

#endif // __DMX_LIB_H__

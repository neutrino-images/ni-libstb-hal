/*
 * (C) 2010-2013 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DMX_HAL_H__
#define __DMX_HAL_H__

#include <config.h>
#include <cstdlib>
#include <vector>
#include <inttypes.h>
/* at least on td, config.h needs to be included before... */
#ifndef HAVE_TRIPLEDRAGON
#include <linux/dvb/dmx.h>
#else /* TRIPLEDRAGON */
extern "C" {
#include <hardware/xp/xp_osd_user.h>
}
#if defined DMX_FILTER_SIZE
#undef DMX_FILTER_SIZE
#endif
#define DMX_FILTER_SIZE FILTER_LENGTH
#endif /* TRIPLEDRAGON */

#include <cs_types.h>

#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K
#define MAX_DMX_UNITS 16
#else
#define MAX_DMX_UNITS 4
#endif

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
		bool Open(DMX_CHANNEL_TYPE pes_type, void * x = NULL, int y = 0);
		void Close(void);
		bool Start(bool record = false);
		bool Stop(void);
		int Read(unsigned char *buff, int len, int Timeout = 0);
		bool sectionFilter(unsigned short pid, const unsigned char * const filter, const unsigned char * const mask, int len, int Timeout = 0, const unsigned char * const negmask = NULL);
		bool pesFilter(const unsigned short pid);
		void SetSyncMode(AVSYNC_TYPE mode);
		void * getBuffer();
		void * getChannel();
		DMX_CHANNEL_TYPE getChannelType(void) { return dmx_type; };
		bool addPid(unsigned short pid);
		void getSTC(int64_t * STC);
		int getUnit(void);
		static bool SetSource(int unit, int source);
		static int GetSource(int unit);
		int getFD(void) { return fd; };		/* needed by cPlayback class */
		cDemux(int num = 0);
		~cDemux();
	private:
		void removePid(unsigned short Pid);	/* needed by cRecord class */
		int num;
		int fd;
		int buffersize;
		uint16_t pid;
		uint8_t flt;
		std::vector<pes_pids> pesfds;
		DMX_CHANNEL_TYPE dmx_type;
		void *pdata;
};

#endif // __DMX_HAL_H__

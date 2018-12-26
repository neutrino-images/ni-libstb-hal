/*
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __PLAYBACK_GST_H__
#define __PLAYBACK_GST_H__

#include <string>
#include <stdint.h>
#include <vector>

#include <config.h>


typedef enum
{
	STATE_STOP,
	STATE_PLAY,
	STATE_PAUSE,
	STATE_FF,
	STATE_REW,
	STATE_SLOW
} playstate_t;

typedef enum
{
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t;

struct AVFormatContext;

class cPlayback
{
private:
	bool playing, first;
	bool decoders_closed;

	int mSpeed;
	int mAudioStream;
	int init_jump;

public:
	playstate_t playstate;

	cPlayback(int);
	bool Open(playmode_t PlayMode);
	void Close(void);
	bool Start(char *filename, int vpid, int vtype, int apid, int ac3, int duration, std::string headers = "");
	bool Start(std::string filename, std::string headers = "");
	bool Play(void);
	bool SyncAV(void);

	bool Stop(void);
	bool SetAPid(int pid, bool ac3);
	bool SetSubtitlePid(int pid);
	bool SetTeletextPid(int pid);

	void trickSeek(int ratio);
	bool SetSpeed(int speed);
	bool SetSlow(int slow);
	bool GetSpeed(int &speed) const;
	bool GetPosition(int &position, int &duration);
	void GetPts(uint64_t &pts);
	int GetAPid(void);
	int GetVPid(void);
	int GetSubtitlePid(void);
	bool SetPosition(int position, bool absolute = false);
	void FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language);
	void FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language);
	void FindAllTeletextsubtitlePids(int *pids, unsigned int *numpidt, std::string *tlanguage, int *mags, int *pages);
	void RequestAbort(void);
	void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language);
	bool SelectSubtitles(int pid);
	uint64_t GetReadCount(void);
	void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
	void GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
	AVFormatContext *GetAVFormatContext();
	void ReleaseAVFormatContext();
	std::string extra_headers;
	std::string user_agent;

	//
	~cPlayback();
	void getMeta();
};

#endif // __PLAYBACK_GST_H__

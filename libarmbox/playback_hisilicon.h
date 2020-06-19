/*
	Copyright (C) 2018 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __HAL_PLAYBACK_H
#define __HAL_PLAYBACK_H

#include <string>
#include <vector>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include "hisilicon.h"

typedef enum
{
	PLAYMODE_TS = 0,
	PLAYMODE_FILE
} playmode_t;

struct AVFormatContext;

struct errorInfo
{
	std::string error_message;
	std::string missing_codec;
};

class cPlayback
{
	friend class CStreamInfo2;
	friend class netlink_event;

private:
	int m_video_fd;
	int m_audio_fd;
	bool enabled;
	bool playing, first;
	bool no_probe;
	bool got_vpts_ts;
	int nPlaybackSpeed;
	int mAudioStream;
	int mSubtitleStream;
	int mTeletextStream;
	int64_t vpts_ts;
	bool Stop(void);
	bool decoders_closed;
	playmode_t pm;
	std::string fn_ts;
	std::string fn_xml;
	off64_t last_size;
	int init_jump;
	const char *getVidFormatStr(uint32_t format);
	const char *getAudFormatStr(uint32_t format);
	const char *getSubFormatStr(uint32_t format);


public:
	cPlayback(int num = 0);
	~cPlayback();

	bool Open(playmode_t PlayMode);
	void Close(void);
	bool Start(char *filename, int vpid, int vtype, int apid, int ac3, int duration, std::string headers = "", std::string filename2 = "");
	bool Start(std::string filename, std::string headers = "", std::string filename2 = "");
	bool SetAPid(int pid, bool ac3 = false);
	bool SetVPid(int /*pid*/);
	bool SetSubtitlePid(int pid);
	bool SetTeletextPid(int pid);
	int GetAPid(void)
	{
		return mAudioStream;
	}
	int GetVPid(void)
	{
		return 0;
	}
	int GetSubtitlePid(void)
	{
		return mSubtitleStream;
	}
	int GetTeletextPid(void);
	bool SetSpeed(int speed);
	bool GetSpeed(int &speed) const;
	bool GetPosition(int &position, int &duration);
	void GetPts(uint64_t &pts);
	bool SetPosition(int position, bool absolute = false);
	void FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language);
	void FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language);
	void FindAllTeletextsubtitlePids(int */*pids*/, unsigned int *numpidt, std::string */*tlanguage*/, int */*mags*/, int */*pages*/);
	void RequestAbort(void);
	bool IsPlaying(void);
	uint64_t GetReadCount(void);

	void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
	void GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);

	AVFormatContext *GetAVFormatContext();
	void ReleaseAVFormatContext();
};

class netlink_event : public OpenThreads::Thread
{
	friend class cPlayback;

protected:
	bool running;
private:
	netlink_event();
	~netlink_event();
	static netlink_event *netlink_event_instance;
	cPlayback *player;
	int m_player_state;
	enum
	{
		stIdle, stRunning, stStopped,
	};
	struct streamid
	{
		uint16_t programid;
		uint16_t videostreamid;
		uint16_t audiostreamid;
		uint16_t subtitlestreamid;
	} streamid;
	int m_state;
	bool m_paused;
	bool m_buffering;
	HI_FORMAT_FILE_INFO_S fileinfo;
	struct nlmsghdr *nlh;
	int m_bufferpercentage;
	uint32_t m_seekable;
	uint32_t m_download_progress;
	int netlink_socket;
	int receive_netlink_message();
	errorInfo m_errorInfo;
	void run();
	void Receive();
public:
	static netlink_event* getInstance();
	uint64_t getDuration()
	{
		return fileinfo.s64Duration;
	};
	bool Start(cPlayback *player);
	bool Stop();
};

#if 0 // for later use, maybe
class video_event : public OpenThreads::Thread
{
	friend class cPlayback;

protected:
	bool running;
private:
	int m_video_fd;
	void run();
	void Receive();
public:
	bool Start(int video_fd);
	bool Stop();
};
#endif
#endif

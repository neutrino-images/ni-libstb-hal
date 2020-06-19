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

#define __USE_FILE_OFFSET64 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>

#include <audio_lib.h>
#include <video_lib.h>

#include "playback_hisilicon.h"
#include "hal_debug.h"

#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYBACK, this, args)
#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_PLAYBACK, NULL, args)
#define hal_info(args...)  _hal_info(HAL_DEBUG_PLAYBACK, this, args)

#define MAX_PAYLOAD 4096

extern cAudio *audioDecoder;
extern cVideo *videoDecoder;

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] =
	{
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	pm = PlayMode;
	got_vpts_ts = false;
	vpts_ts = 0;
	fn_ts = "";
	fn_xml = "";
	last_size = 0;
	nPlaybackSpeed = 0;
	init_jump = -1;

	return 0;
}

void cPlayback::Close(void)
{
	hal_info("%s\n", __func__);

	//Dagobert: movieplayer does not call stop, it calls close ;)
	if(playing)
		Stop();

}

bool cPlayback::Start(std::string filename, std::string headers, std::string filename2)
{
	return Start((char *) filename.c_str(), 0, 0, 0, 0, 0, headers,filename2);
}

bool cPlayback::Start(char *filename, int vpid, int vtype, int apid, int ac3, int, std::string headers __attribute__((unused)), std::string filename2 __attribute__((unused)))
{
	bool ret = false;

	hal_info("%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d fd=%d\n", __func__, filename, vpid, vtype, apid, ac3, videoDecoder->fd);

	init_jump = -1;
	//create playback path
	mAudioStream = 0;
	mSubtitleStream = -1;
	mTeletextStream = -1;
	unlink("/tmp/.id3coverart");

	videoDecoder->closeDevice();
	videoDecoder->openDevice();

	netlink_event::getInstance()->Start(this);

#if 0 //for later use
	video_event::getInstance()->Start(videoDecoder->fd);
#endif

	if (videoDecoder->fd >= 0)
	{
		struct video_command cmd = {0};
		struct argdata
		{
			uint32_t size;
			void *arg;
		} *argdata = (struct argdata*)cmd.raw.data;
		cmd.cmd = 101; /* set url */
		argdata->arg = (void*)filename;
		argdata->size = strlen(filename) + 1;
		::ioctl(videoDecoder->fd, VIDEO_COMMAND, &cmd);
		cmd.cmd = 102; /* set useragent */
		argdata->arg = (void*)headers.c_str();
		argdata->size = headers.length() + 1;
		::ioctl(videoDecoder->fd, VIDEO_COMMAND, &cmd);
	}
	else return false;

	if (videoDecoder->fd >= 0)
	{
		::ioctl(videoDecoder->fd, VIDEO_FAST_FORWARD, 0);
		::ioctl(videoDecoder->fd, VIDEO_SLOWMOTION, 0);
		::ioctl(videoDecoder->fd, VIDEO_PLAY);
		::ioctl(videoDecoder->fd, VIDEO_CONTINUE);
	}
	else return false;
	
	if (audioDecoder->fd >= 0)
	{
		::ioctl(audioDecoder->fd, AUDIO_PLAY);
		::ioctl(audioDecoder->fd, AUDIO_CONTINUE);
	}
	else return false;

	playing = true;

	return true;
}

bool cPlayback::Stop(void)
{
	hal_info("%s playing %d\n", __func__, playing);

	if (videoDecoder->fd >= 0)
	{
		::ioctl(videoDecoder->fd, VIDEO_STOP);
	}
	if (audioDecoder->fd >= 0)
	{
		::ioctl(audioDecoder->fd, AUDIO_STOP);
	}

	playing = false;
	return true;
}

bool cPlayback::SetAPid(int pid, bool /* ac3 */)
{
	hal_info("%s\n", __func__);
	int i = pid;

	if (pid != mAudioStream)
	{
		if (videoDecoder->fd >= 0)
		{
			struct video_command cmd = {0};
			cmd.cmd = 105; /* set audio streamid */
			cmd.raw.data[0] = pid;
			::ioctl(videoDecoder->fd, VIDEO_COMMAND, &cmd);
		}
		mAudioStream = pid;
	}
	return true;
}

bool cPlayback::SetVPid(int /*pid*/)
{
	hal_info("%s\n", __func__);
	return true;
}

bool cPlayback::SetSubtitlePid(int pid)
{
	hal_info("%s\n", __func__);
	int i = pid;

	if (pid != mSubtitleStream)
	{
		if (videoDecoder->fd >= 0)
		{
			struct video_command cmd = {0};
			cmd.cmd = 106; /* set subtitle streamid */
			cmd.raw.data[0] = i;
			::ioctl(videoDecoder->fd, VIDEO_COMMAND, &cmd);
		}
		mSubtitleStream = pid;
	}
	return true;
}

bool cPlayback::SetTeletextPid(int pid)
{
	hal_info("%s\n", __func__);

	//int i = pid;

	if (pid != mTeletextStream)
	{
		mTeletextStream = pid;
	}
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	hal_info("%s playing %d speed %d\n", __func__, playing, speed);

	if (!playing)
		return false;

	if (videoDecoder->fd)
	{
		int result = 0;
		nPlaybackSpeed = speed;

		if (speed > 1)
		{
			::ioctl(videoDecoder->fd, VIDEO_FAST_FORWARD, speed);
			::ioctl(videoDecoder->fd, VIDEO_CONTINUE);
		}
		else if (speed < 0)
		{
			// trickseek
		}
		else if (speed == 0)
		{
			if (videoDecoder->fd >= 0)
			{
				::ioctl(videoDecoder->fd, VIDEO_FREEZE);
			}
			if (audioDecoder->fd >= 0)
			{
				::ioctl(audioDecoder->fd, AUDIO_PAUSE);
			}
		}
		else
		{
			if (videoDecoder->fd >= 0)
			{
				::ioctl(videoDecoder->fd, VIDEO_FAST_FORWARD, 0);
				::ioctl(videoDecoder->fd, VIDEO_SLOWMOTION, 0);
				::ioctl(videoDecoder->fd, VIDEO_CONTINUE);
			}
			if (audioDecoder->fd >= 0)
			{
				::ioctl(audioDecoder->fd, AUDIO_CONTINUE);
			}
		}

		if (init_jump > -1)
		{
			SetPosition(init_jump);
			init_jump = -1;
		}

		if (result != 0)
		{
			printf("returning false\n");
			return false;
		}
	}
	return true;
}

bool cPlayback::GetSpeed(int &speed) const
{
	hal_debug("%s\n", __func__);
	speed = nPlaybackSpeed;
	return true;
}

void cPlayback::GetPts(uint64_t &pts)
{
	pts = 0;
	pts = videoDecoder->GetPTS();
	/*
	if (videoDecoder->fd >= 0)
	{
		if (::ioctl(videoDecoder->fd, VIDEO_GET_PTS, &pts) >= 0)
		{
			return;
		}
	}
	if (audioDecoder->fd >= 0)
	{
		if (::ioctl(audioDecoder->fd, AUDIO_GET_PTS, &pts) >= 0)
		{
			return;
		}
	}*/
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	bool got_duration = false;
	hal_debug("%s %d %d\n", __func__, position, duration);

	/* hack: if the file is growing (timeshift), then determine its length
	 * by comparing the mtime with the mtime of the xml file */
	if (pm == PLAYMODE_TS)
	{
		struct stat64 s;
		if (!stat64(fn_ts.c_str(), &s))
		{
			if (!playing || last_size != s.st_size)
			{
				last_size = s.st_size;
				time_t curr_time = s.st_mtime;
				if (!stat64(fn_xml.c_str(), &s))
				{
					duration = (curr_time - s.st_mtime) * 1000;
					if (!playing)
						return true;
					got_duration = true;
				}
			}
		}
	}

	if (!playing)
		return false;

	int64_t vpts = 0;
	//::ioctl(videoDecoder->fd, VIDEO_GET_PTS, &vpts);
	vpts = videoDecoder->GetPTS();

	if (vpts <= 0)
	{
		printf("ERROR: vpts==0\n");
	}
	else
	{
		/* workaround for crazy vpts value during timeshift */
		if (!got_vpts_ts && pm == PLAYMODE_TS)
		{
			vpts_ts = vpts;
			got_vpts_ts = true;
		}
		if (got_vpts_ts)
			vpts -= vpts_ts;
		/* end workaround */
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts / 90;
	}

	if (got_duration)
		return true;

	int64_t length = 0;

	length = netlink_event::getInstance()->getDuration() * 1000;

	if (length <= 0)
	{
		duration = duration + 1000;
	}
	else
	{
		duration = length * 1000;
	}

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	hal_info("%s %d\n", __func__, position);

	if (playing && first)
	{
		/* the calling sequence is:
		 * Start()       - paused
		 * SetPosition() - which fails if not running
		 * SetSpeed()    - to start playing
		 * so let's remember the initial jump position and later jump to it
		 */
		init_jump = position;
		first = false;
		return false;
	}

	int64_t pos = (position / 1000.0);

	if (!absolute)
	{
		uint64_t pts;
		::ioctl(videoDecoder->fd, VIDEO_GET_PTS, &pts);
		pos = (pts / 90LL) + pos;
	}

	if (videoDecoder->fd >= 0)
	{
		struct video_command cmd = {0};
		cmd.cmd = 100; /* seek */
		cmd.stop.pts = pos * 90LL;
		::ioctl(videoDecoder->fd, VIDEO_COMMAND, &cmd);
	}


	return true;
}

void cPlayback::FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language)
{
	hal_info("%s\n", __func__);
	int max_numpida = *numpida;
	*numpida = 0;

}

void cPlayback::FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	hal_info("%s\n", __func__);

	int max_numpids = *numpids;
	*numpids = 0;

}

void cPlayback::FindAllTeletextsubtitlePids(int */*pids*/, unsigned int *numpids, std::string */*language*/, int */*mags*/, int */*pages*/)
{
	hal_info("%s\n", __func__);
	//int max_numpids = *numpids;
	*numpids = 0;

}

int cPlayback::GetTeletextPid(void)
{
	hal_info("%s\n", __func__);
	int pid = -1;

	printf("teletext pid id %d (0x%x)\n", pid, pid);
	return pid;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();
}

cPlayback::cPlayback(int num __attribute__((unused)))
{
	hal_info("%s\n", __func__);
	playing = false;
	first = false;
}

cPlayback::~cPlayback()
{
	hal_info("%s\n", __func__);
}

void cPlayback::RequestAbort()
{
	hal_info("%s\n", __func__);
	Stop();
}

bool cPlayback::IsPlaying()
{
	return (playing == true);
}

uint64_t cPlayback::GetReadCount()
{
	return 0;
}

AVFormatContext *cPlayback::GetAVFormatContext()
{
	return NULL;
}

void cPlayback::ReleaseAVFormatContext()
{
}

const char *cPlayback::getVidFormatStr(uint32_t format)
{
	switch (format)
	{
	case HI_FORMAT_VIDEO_MPEG2:
		return "MPEG2";
		break;

	case HI_FORMAT_VIDEO_MPEG4:
		return "MPEG4";
		break;

	case HI_FORMAT_VIDEO_AVS:
		return "AVS";
		break;

	case HI_FORMAT_VIDEO_H263:
		return "H263";
		break;

	case HI_FORMAT_VIDEO_H264:
		return "H264";
		break;

	case HI_FORMAT_VIDEO_REAL8:
		return "REAL8";
		break;

	case HI_FORMAT_VIDEO_REAL9:
		return "REAL9";
		break;

	case HI_FORMAT_VIDEO_VC1:
		return "VC1";
		break;

	case HI_FORMAT_VIDEO_VP6:
		return "VP6";
		break;

	case HI_FORMAT_VIDEO_DIVX3:
		return "DIVX3";
		break;

	case HI_FORMAT_VIDEO_RAW:
		return "RAW";
		break;

	case HI_FORMAT_VIDEO_JPEG:
		return "JPEG";
		break;

	case HI_FORMAT_VIDEO_MJPEG:
		return "MJPEG";
		break;
	case HI_FORMAT_VIDEO_MJPEGB:
		return "MJPEGB";
		break;
	case HI_FORMAT_VIDEO_SORENSON:
		return "SORENSON";
		break;

	case HI_FORMAT_VIDEO_VP6F:
		return "VP6F";
		break;

	case HI_FORMAT_VIDEO_VP6A:
		return "VP6A";
		break;

	case HI_FORMAT_VIDEO_VP8:
		return "VP8";
		break;
	case HI_FORMAT_VIDEO_MVC:
		return "MVC";
		break;
	case HI_FORMAT_VIDEO_SVQ1:
		return "SORENSON1";
		break;
	case HI_FORMAT_VIDEO_SVQ3:
		return "SORENSON3";
		break;
	case HI_FORMAT_VIDEO_DV:
		return "DV";
		break;
	case HI_FORMAT_VIDEO_WMV1:
		return "WMV1";
		break;
	case HI_FORMAT_VIDEO_WMV2:
		return "WMV2";
		break;
	case HI_FORMAT_VIDEO_MSMPEG4V1:
		return "MICROSOFT MPEG4V1";
		break;
	case HI_FORMAT_VIDEO_MSMPEG4V2:
		return "MICROSOFT MPEG4V2";
		break;
	case HI_FORMAT_VIDEO_CINEPAK:
		return "CINEPACK";
		break;
	case HI_FORMAT_VIDEO_RV10:
		return "RV10";
		break;
	case HI_FORMAT_VIDEO_RV20:
		return "RV20";
		break;
	case HI_FORMAT_VIDEO_INDEO4:
		return "INDEO4";
		break;
	case HI_FORMAT_VIDEO_INDEO5:
		return "INDEO5";
		break;
	case HI_FORMAT_VIDEO_HEVC:
		return "H265";
	case HI_FORMAT_VIDEO_VP9:
		return "VP9";
	default:
		return "UNKNOWN";
		break;
	}

	return "UNKNOWN";
}

const char *cPlayback::getAudFormatStr(uint32_t format)
{
	switch (format)
	{
	case HI_FORMAT_AUDIO_MP2:
		return "MPEG2";
		break;
	case HI_FORMAT_AUDIO_MP3:
		return "MPEG3";
		break;
	case HI_FORMAT_AUDIO_AAC:
		return "AAC";
		break;
	case HI_FORMAT_AUDIO_AC3:
		return "AC3";
		break;
	case HI_FORMAT_AUDIO_DTS:
		return "DTS";
		break;
	case HI_FORMAT_AUDIO_VORBIS:
		return "VORBIS";
		break;
	case HI_FORMAT_AUDIO_DVAUDIO:
		return "DVAUDIO";
		break;
	case HI_FORMAT_AUDIO_WMAV1:
		return "WMAV1";
		break;
	case HI_FORMAT_AUDIO_WMAV2:
		return "WMAV2";
		break;
	case HI_FORMAT_AUDIO_MACE3:
		return "MACE3";
		break;
	case HI_FORMAT_AUDIO_MACE6:
		return "MACE6";
		break;
	case HI_FORMAT_AUDIO_VMDAUDIO:
		return "VMDAUDIO";
		break;
	case HI_FORMAT_AUDIO_SONIC:
		return "SONIC";
		break;
	case HI_FORMAT_AUDIO_SONIC_LS:
		return "SONIC_LS";
		break;
	case HI_FORMAT_AUDIO_FLAC:
		return "FLAC";
		break;
	case HI_FORMAT_AUDIO_MP3ADU:
		return "MP3ADU";
		break;
	case HI_FORMAT_AUDIO_MP3ON4:
		return "MP3ON4";
		break;
	case HI_FORMAT_AUDIO_SHORTEN:
		return "SHORTEN";
		break;
	case HI_FORMAT_AUDIO_ALAC:
		return "ALAC";
		break;
	case HI_FORMAT_AUDIO_WESTWOOD_SND1:
		return "WESTWOOD_SND1";
		break;
	case HI_FORMAT_AUDIO_GSM:
		return "GSM";
		break;
	case HI_FORMAT_AUDIO_QDM2:
		return "QDM2";
		break;
	case HI_FORMAT_AUDIO_COOK:
		return "COOK";
		break;
	case HI_FORMAT_AUDIO_TRUESPEECH:
		return "TRUESPEECH";
		break;
	case HI_FORMAT_AUDIO_TTA:
		return "TTA";
		break;
	case HI_FORMAT_AUDIO_SMACKAUDIO:
		return "SMACKAUDIO";
		break;
	case HI_FORMAT_AUDIO_QCELP:
		return "QCELP";
		break;
	case HI_FORMAT_AUDIO_WAVPACK:
		return "WAVPACK";
		break;
	case HI_FORMAT_AUDIO_DSICINAUDIO:
		return "DSICINAUDIO";
		break;
	case HI_FORMAT_AUDIO_IMC:
		return "IMC";
		break;
	case HI_FORMAT_AUDIO_MUSEPACK7:
		return "MUSEPACK7";
		break;
	case HI_FORMAT_AUDIO_MLP:
		return "MLP";
		break;
	case HI_FORMAT_AUDIO_GSM_MS:
		return "GSM_MS";
		break;
	case HI_FORMAT_AUDIO_ATRAC3:
		return "ATRAC3";
		break;
	case HI_FORMAT_AUDIO_VOXWARE:
		return "VOXWARE";
		break;
	case HI_FORMAT_AUDIO_APE:
		return "APE";
		break;
	case HI_FORMAT_AUDIO_NELLYMOSER:
		return "NELLYMOSER";
		break;
	case HI_FORMAT_AUDIO_MUSEPACK8:
		return "MUSEPACK8";
		break;
	case HI_FORMAT_AUDIO_SPEEX:
		return "SPEEX";
		break;
	case HI_FORMAT_AUDIO_WMAVOICE:
		return "WMAVOICE";
		break;
	case HI_FORMAT_AUDIO_WMAPRO:
		return "WMAPRO";
		break;
	case HI_FORMAT_AUDIO_WMALOSSLESS:
		return "WMALOSSLESS";
		break;
	case HI_FORMAT_AUDIO_ATRAC3P:
		return "ATRAC3P";
		break;
	case HI_FORMAT_AUDIO_EAC3:
		return "EAC3";
		break;
	case HI_FORMAT_AUDIO_SIPR:
		return "SIPR";
		break;
	case HI_FORMAT_AUDIO_MP1:
		return "MP1";
		break;
	case HI_FORMAT_AUDIO_TWINVQ:
		return "TWINVQ";
		break;
	case HI_FORMAT_AUDIO_TRUEHD:
		return "TRUEHD";
		break;
	case HI_FORMAT_AUDIO_MP4ALS:
		return "MP4ALS";
		break;
	case HI_FORMAT_AUDIO_ATRAC1:
		return "ATRAC1";
		break;
	case HI_FORMAT_AUDIO_BINKAUDIO_RDFT:
		return "BINKAUDIO_RDFT";
		break;
	case HI_FORMAT_AUDIO_BINKAUDIO_DCT:
		return "BINKAUDIO_DCT";
		break;
	case HI_FORMAT_AUDIO_DRA:
		return "DRA";
		break;

	case HI_FORMAT_AUDIO_PCM: /* various PCM "codecs" */
		return "PCM";
		break;

	case HI_FORMAT_AUDIO_ADPCM: /* various ADPCM codecs */
		return "ADPCM";
		break;

	case HI_FORMAT_AUDIO_AMR_NB: /* AMR */
		return "AMR_NB";
		break;
	case HI_FORMAT_AUDIO_AMR_WB:
		return "AMR_WB";
		break;
	case HI_FORMAT_AUDIO_AMR_AWB:
		return "AMR_AWB";
		break;

	case HI_FORMAT_AUDIO_RA_144: /* RealAudio codecs*/
		return "RA_144";
		break;
	case HI_FORMAT_AUDIO_RA_288:
		return "RA_288";
		break;

	case HI_FORMAT_AUDIO_DPCM: /* various DPCM codecs */
		return "DPCM";
		break;

	case HI_FORMAT_AUDIO_G711:  /* various G.7xx codecs */
		return "G711";
		break;
	case HI_FORMAT_AUDIO_G722:
		return "G722";
		break;
	case HI_FORMAT_AUDIO_G7231:
		return "G7231";
		break;
	case HI_FORMAT_AUDIO_G726:
		return "G726";
		break;
	case HI_FORMAT_AUDIO_G728:
		return "G728";
		break;
	case HI_FORMAT_AUDIO_G729AB:
		return "G729AB";
		break;
	case HI_FORMAT_AUDIO_PCM_BLURAY:
		return "PCM_BLURAY";
		break;
	default:
		break;
	}

	return "UNKNOWN";
}

const char *cPlayback::getSubFormatStr(uint32_t format)
{
	switch (format)
	{
	case HI_FORMAT_SUBTITLE_ASS:
		return "ASS";
		break;
	case HI_FORMAT_SUBTITLE_LRC:
		return "LRC";
		break;
	case HI_FORMAT_SUBTITLE_SRT:
		return "SRT";
		break;
	case HI_FORMAT_SUBTITLE_SMI:
		return "SMI";
		break;
	case HI_FORMAT_SUBTITLE_SUB:
		return "SUB";
		break;
	case HI_FORMAT_SUBTITLE_TXT:
		return "TEXT";
		break;
	case HI_FORMAT_SUBTITLE_HDMV_PGS:
		return "HDMV_PGS";
		break;
	case HI_FORMAT_SUBTITLE_DVB_SUB:
		return "DVB_SUB_BMP";
		break;
	case HI_FORMAT_SUBTITLE_DVD_SUB:
		return "DVD_SUB_BMP";
		break;
	default:
		return "UNKNOWN";
		break;
	}

	return "UNKNOWN";
}

netlink_event * netlink_event::netlink_event_instance = NULL;

netlink_event::netlink_event()
{
	netlink_socket = -1;
}

netlink_event::~netlink_event()
{
	if (netlink_socket >= 0)
	{
		close(netlink_socket);
		netlink_socket = -1;
	}
}

netlink_event* netlink_event::getInstance()
{
	if (netlink_event_instance == NULL)
	{
		netlink_event_instance = new netlink_event();
		hal_debug_c("[HSP] new netlink instance created \n");
	}
	return netlink_event_instance;
}

bool netlink_event::Start(cPlayback * _player)
{
	if (running)
		return false;

	player = _player;

	memset(&fileinfo, 0, sizeof(fileinfo));
	memset(&streamid, 0, sizeof(streamid));

	struct sockaddr_nl src_addr;
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */
	netlink_socket = socket(PF_NETLINK, SOCK_RAW, 30);
	bind(netlink_socket, (struct sockaddr*)&src_addr, sizeof(src_addr));

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));

	running = true;
	return (OpenThreads::Thread::start() == 0);
}

bool netlink_event::Stop()
{
	if (!running)
		return false;

	running = false;

	OpenThreads::Thread::cancel();

	if (netlink_socket >= 0)
	{
		close(netlink_socket);
		netlink_socket = -1;
	}

	return (OpenThreads::Thread::join() == 0);
}

void netlink_event::run()
{
	OpenThreads::Thread::setCancelModeAsynchronous();
	while (running)
	{
		Receive();
	}
}

int netlink_event::receive_netlink_message()
{
	struct msghdr msg;
	struct iovec iov;
	struct sockaddr_nl dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0; /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */

	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(netlink_socket, &msg, 0) <= 0)
	{
		return 0;
	}
	return NLMSG_PAYLOAD(nlh, 0);
}

void netlink_event::Receive()
{
	if (netlink_socket >= 0)
	{
		int readlen = receive_netlink_message();
		if (readlen > 0)
		{
			int decoder = *((uint32_t*)NLMSG_DATA(nlh)) >> 24;
			int msgtype = *((uint32_t*)NLMSG_DATA(nlh)) & 0xffffff;
			switch (msgtype)
			{
#if 0
			case 2: /* subtitle data */
				if (m_currentSubtitleStream >= 0 && m_subtitle_widget && !m_paused)
				{
					struct subtitleheader
					{
						uint64_t pts;
						uint32_t duration;
						uint32_t datasize;
					} header;
					memcpy(&header, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(header));
					ePangoSubtitlePage pango_page;
					gRGB rgbcol(0xD0,0xD0,0xD0);
					pango_page.m_elements.push_back(ePangoSubtitlePageElement(rgbcol, (const char*)NLMSG_DATA(nlh) + sizeof(uint32_t) + sizeof(header)));
					pango_page.m_show_pts = header.pts; /* ignored by widget (TODO?) */
					pango_page.m_timeout = 8000;
					m_subtitle_widget->setPage(pango_page);
				}
				break;
			case 3: /* clear subtitles */
				if (m_currentSubtitleStream >= 0 && m_subtitle_widget)
				{
					ePangoSubtitlePage pango_page;
					pango_page.m_show_pts = 0;
					pango_page.m_timeout = 0;
					m_subtitle_widget->setPage(pango_page);
				}
				break;
#endif
			case 4: /* file info */
				memcpy(&fileinfo, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(fileinfo));
				fileinfo.pastProgramInfo = (HI_FORMAT_PROGRAM_INFO_S*)malloc(fileinfo.u32ProgramNum * sizeof(HI_FORMAT_PROGRAM_INFO_S));
				fileinfo.u32ProgramNum = 0;
				break;
			case 5: /* program info */
				memcpy(&fileinfo.pastProgramInfo[fileinfo.u32ProgramNum], (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(HI_FORMAT_PROGRAM_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].pastVidStream = (HI_FORMAT_VID_INFO_S*)malloc(fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32VidStreamNum * sizeof(HI_FORMAT_VID_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32VidStreamNum = 0;
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].pastAudStream = (HI_FORMAT_AUD_INFO_S*)malloc(fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32AudStreamNum * sizeof(HI_FORMAT_AUD_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32AudStreamNum = 0;
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].pastSubStream = (HI_FORMAT_SUB_INFO_S*)malloc(fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32SubStreamNum * sizeof(HI_FORMAT_SUB_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum].u32SubStreamNum = 0;
				fileinfo.u32ProgramNum++;
				break;
			case 6: /* video stream */
				memcpy(&fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].pastVidStream[fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32VidStreamNum], (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(HI_FORMAT_VID_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32VidStreamNum++;
				break;
			case 7: /* audio stream */
				memcpy(&fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].pastAudStream[fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32AudStreamNum], (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(HI_FORMAT_AUD_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32AudStreamNum++;
				break;
			case 8: /* subtitle stream */
				memcpy(&fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].pastSubStream[fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32SubStreamNum], (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(HI_FORMAT_SUB_INFO_S));
				fileinfo.pastProgramInfo[fileinfo.u32ProgramNum - 1].u32SubStreamNum++;
				break;
			case 9: /* stream id */
				memcpy(&streamid, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(streamid));
				hal_debug("[HSP] streamid: program %u, video %u, audio %u, subtitle %u", streamid.programid, streamid.videostreamid, streamid.audiostreamid, streamid.subtitlestreamid);
				player->mAudioStream = streamid.audiostreamid;
				player->mSubtitleStream = streamid.subtitlestreamid;
				break;
			case 10: /* player state */
			{
				int32_t state;
				memcpy(&state, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(state));
				hal_debug("[HSP] player state %d-->%d", m_player_state, state);
				switch (state)
				{
				case 0: /* init */
					break;
				case 1: /* deinit */
					break;
				case 2: /* play */
					if (m_state != stRunning)
					{
						m_state = stRunning;
					}
					m_paused = false;
					break;
				case 3: /* fast forward */
					break;
				case 4: /* rewind */
					break;
				case 5: /* pause */
					m_paused = true;
					break;
				case 6: /* stop */
					m_paused = false;
					break;
				case 7: /* preparing */
					break;
				}
				m_player_state = state;
				player->playing = (state < 6);
			}
			break;
			case 11: /* error */
			{
				int32_t error;
				memcpy(&error, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(error));
				switch (error)
				{
				case 0: /* no error */
					break;
				case 1: /* video playback failed */
					m_errorInfo.error_message = "video playback failed";
					break;
				case 2: /* audio playback failed */
					m_errorInfo.error_message = "audio playback failed";
					break;
				case 3: /* subtitle playback failed */
					m_errorInfo.error_message = "subtitle playback failed";
					break;
				case 4: /* media playback failed */
					m_errorInfo.error_message = "media playback failed";
					break;
				case 5: /* timeout */
					m_errorInfo.error_message = "timeout";
					break;
				case 6: /* file format not supported */
					m_errorInfo.error_message = "format not supported";
					break;
				case 7: /* unknown error */
					m_errorInfo.error_message = "unknown error";
					break;
				case 8: /* I-frame decoding error */
					break;
				}
				hal_debug("[HSP] error %s (%d)", m_errorInfo.error_message.c_str(), error);
			}
			break;
			case 12: /* buffering */
			{
				struct bufferinfo
				{
					uint32_t status;
					uint32_t percentage;
				} info;
				memcpy(&info, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(info));
				hal_debug("[HSP] buffering %u %u%%", info.status, info.percentage);
				m_bufferpercentage = info.percentage;
				switch (info.status)
				{
				case 0: /* empty */
				case 1: /* insufficient */
					if (!m_buffering)
					{
						m_buffering = true;
						hal_debug("[HSP] start buffering....pause playback");
						player->SetSpeed(0);
					}
					break;
				case 2: /* enough */
				case 3: /* full */
					if (m_buffering)
					{
						m_buffering = false;
						hal_debug("[HSP] end buffering....continue playback");
						player->SetSpeed(1);
					}
					break;
				default:
					break;
				}
			}
			break;
			case 13: /* network info */
			{
				struct networkinfo
				{
					uint32_t status;
					int32_t errorcode;
				} info;
				memcpy(&info, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(info));
				switch (info.status)
				{
				case 0: /* network: unknown error */
					m_errorInfo.error_message = "network: error";
					break;
				case 1: /* network: failed to connect */
					m_errorInfo.error_message = "network: connection failed";
					break;
				case 2: /* network: timeout */
					m_errorInfo.error_message = "network: timeout";
					break;
				case 3: /* network: disconnected */
					m_errorInfo.error_message = "network: disconnected";
					break;
				case 4: /* network: file not found */
					m_errorInfo.error_message = "network: file not found";
					break;
				case 5: /* network: status ok */
					m_errorInfo.error_message = "network: status ok";
					break;
				case 6: /* network: http errorcode */
					m_errorInfo.error_message = "network: http errorcode";
					break;
				case 7: /* network: bitrate adjusted */
					m_errorInfo.error_message = "network: bitrate adjusted";
					break;
				}
				hal_debug("[HSP] network info %s (%u) %d", m_errorInfo.error_message.c_str(), info.status, info.errorcode);
			}
			break;
			case 14: /* event */
			{
				int32_t event;
				memcpy(&event, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(event));
				switch (event)
				{
				case 0: /* SOF */
					hal_debug("[HSP] event: SOF");
					/* reached SOF while rewinding */
					break;
				case 1: /* EOF */
					hal_debug("[HSP] event: EOF");
					break;
				}
			}
			break;
			case 15: /* seekable */
				memcpy(&m_seekable, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(m_seekable));
				break;
			case 16: /* download progress */
				memcpy(&m_download_progress, (unsigned char*)NLMSG_DATA(nlh) + sizeof(uint32_t), sizeof(m_download_progress));
				hal_debug("[HSP] dl progress: %d", m_download_progress);
				if (m_download_progress >= 100)
				{
					player->SetSpeed(1);
				}
				break;
			default:
				break;
			}
		}
	}
}

#if 0 // for later use
void video_event::receive_video_msg()
{
	while (m_video_fd >= 0)
	{
		int retval;
		pollfd pfd[1];
		pfd[0].fd = m_video_fd;
		pfd[0].events = POLLPRI;
		retval = ::poll(pfd, 1, 0);
		if (retval < 0 && errno == EINTR)
			continue;
		if (retval <= 0)
			break;
		struct video_event evt;
		if (::ioctl(m_video_fd, VIDEO_GET_EVENT, &evt) < 0)
		{
			hal_debug("[HSP] VIDEO_GET_EVENT failed: %m");
			break;
		}
		else
		{
			if (evt.type == VIDEO_EVENT_SIZE_CHANGED)
			{
				m_aspect = evt.u.size.aspect_ratio == 0 ? 2 : 3;  // convert dvb api to etsi
				m_height = evt.u.size.h;
				m_width = evt.u.size.w;
				hal_debug("[HSP] SIZE_CHANGED %dx%d aspect %d", m_width, m_height, m_aspect);
			}
			else if (evt.type == VIDEO_EVENT_FRAME_RATE_CHANGED)
			{
				m_framerate = evt.u.frame_rate;
				hal_debug("[HSP] FRAME_RATE_CHANGED %d fps", m_framerate);
			}
			else if (evt.type == 16 /*VIDEO_EVENT_PROGRESSIVE_CHANGED*/)
			{
				m_progressive = evt.u.frame_rate;
				hal_debug("[HSP] PROGRESSIVE_CHANGED %d", m_progressive);
			}
			else
				hal_debug("[HSP] unhandled DVBAPI Video Event %d", evt.type);
		}
	}
}
#endif

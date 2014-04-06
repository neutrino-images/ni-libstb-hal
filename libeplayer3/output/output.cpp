/*
 * LinuxDVB Output handling.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/stm_ioctls.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include "player.h"
#include "output.h"
#include "writer.h"
#include "misc.h"
#include "pes.h"

#define dioctl(fd,req,arg) ({		\
	int _r = ioctl(fd,req,arg); \
	if (_r)				\
		fprintf(stderr, "%s %d: ioctl '%s' failed: %d (%s)\n", __FILE__, __LINE__, #req, errno, strerror(errno)); \
	_r; \
})

#define VIDEODEV "/dev/dvb/adapter0/video0"
#define AUDIODEV "/dev/dvb/adapter0/audio0"

Output::Output()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	videofd = audiofd = -1;
	videoWriter = audioWriter = NULL;
	videoStream = audioStream = NULL;
}

Output::~Output()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	Close();
}

bool Output::Open()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);
	
	if (videofd < 0)
		videofd = open(VIDEODEV, O_RDWR);

	if (videofd < 0)
		return false;

	dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);
	dioctl(videofd, VIDEO_SELECT_SOURCE, (void *) VIDEO_SOURCE_MEMORY);
	dioctl(videofd, VIDEO_SET_STREAMTYPE, (void *) STREAM_TYPE_PROGRAM);
	dioctl(videofd, VIDEO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);

	if (audiofd < 0)
		audiofd = open(AUDIODEV, O_RDWR);

	if (audiofd < 0 && videofd > -1) {
		close(videofd);
		videofd = -1;
		return false;
	}

	dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);
	dioctl(audiofd, AUDIO_SELECT_SOURCE, (void *) AUDIO_SOURCE_MEMORY);
	dioctl(audiofd, AUDIO_SET_STREAMTYPE, (void *) STREAM_TYPE_PROGRAM);

	return true;
}

bool Output::Close()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	Stop();

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videofd > -1) {
		close(videofd);
		videofd = -1;
	}
	if (audiofd > -1) {
		close(audiofd);
		audiofd = -1;
	}

	return true;
}

bool Output::Play()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool ret = true;

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videoStream && videofd > -1) {
		videoWriter = Writer::GetWriter(videoStream->codec->codec_id, videoStream->codec->codec_type);
		if (dioctl(videofd, VIDEO_SET_ENCODING, videoWriter->GetVideoEncoding(videoStream->codec->codec_id))
		||  dioctl(videofd, VIDEO_PLAY, NULL))
			ret = false;
	}

	if (audioStream && audiofd > -1) {
		audioWriter = Writer::GetWriter(audioStream->codec->codec_id, audioStream->codec->codec_type);
		if (dioctl(audiofd, AUDIO_SET_ENCODING, audioWriter->GetAudioEncoding(audioStream->codec->codec_id))
		||  dioctl(audiofd, AUDIO_PLAY, NULL))
			ret = false;
	}

	return ret;
}

bool Output::Stop()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool ret = true;

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videofd > -1) {
		dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);
		/* set back to normal speed (end trickmodes) */
		dioctl(videofd, VIDEO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);
		if (dioctl(videofd, VIDEO_STOP, NULL))
			ret = false;
	}

	if (audiofd > -1) {
		dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);
		/* set back to normal speed (end trickmodes) */
		dioctl(audiofd, AUDIO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);
		if (dioctl(audiofd, AUDIO_STOP, NULL))
			ret = false;
    	}

	return ret;
}

bool Output::Pause()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool ret = true;

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videofd > -1) {
		if (dioctl(videofd, VIDEO_FREEZE, NULL))
			ret = false;
	}

	if (audiofd > -1) {
		if (dioctl(audiofd, AUDIO_PAUSE, NULL))
			ret = false;
    	}

	return ret;
}

bool Output::Continue()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool ret = true;

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videofd > -1) {
		if (dioctl(videofd, VIDEO_CONTINUE, NULL))
			ret = false;
	}

	if (audiofd > -1) {
		if (dioctl(audiofd, AUDIO_CONTINUE, NULL))
			ret = false;
    	}

	return ret;
}

bool Output::Mute(bool b)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	//AUDIO_SET_MUTE has no effect with new player
	if (audiofd > -1 && dioctl(audiofd, b ? AUDIO_STOP : AUDIO_PLAY, NULL))
		return false;

	return true;
}


bool Output::Flush()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool ret = true;

	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);

	if (videofd > -1 && dioctl(videofd, VIDEO_FLUSH, NULL))
		ret = false;

	if (audiofd > -1 && dioctl(audiofd, AUDIO_FLUSH, NULL))
		ret = false;

	return ret;
}

bool Output::FastForward(int speed)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);

	if (videofd > -1 && dioctl(videofd, VIDEO_FAST_FORWARD, speed))
		return false;

	return true;
}

bool Output::SlowMotion(int speed)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);

	if (videofd > -1 && dioctl(videofd, VIDEO_SLOWMOTION, speed))
		return false;

	return true;
}

bool Output::AVSync(bool b)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);
	if (audiofd > -1 && dioctl(audiofd, AUDIO_SET_AV_SYNC, b))
		return false;

	return true;
}

bool Output::ClearAudio()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);
	if (audiofd > -1 && dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL))
		return false;

	return true;
}

bool Output::ClearVideo()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	if (videofd > -1 && dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL))
		return false;

	return true;
}

bool Output::Clear()
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	bool aret = ClearAudio();
	bool vret = ClearVideo();
	return aret && vret;
}

bool Output::GetPts(int64_t &pts)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	pts = 0;
	return ((videofd > -1 && !dioctl(videofd, VIDEO_GET_PTS, (void *) &pts)) ||
		(audiofd > -1 && !dioctl(audiofd, AUDIO_GET_PTS, (void *) &pts)));
}

bool Output::GetFrameCount(int64_t &framecount)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	dvb_play_info_t playInfo;

	if ((videofd > -1 && dioctl(videofd, VIDEO_GET_PLAY_INFO, (void *) &playInfo)) ||
	    (audiofd > -1 && dioctl(audiofd, AUDIO_GET_PLAY_INFO, (void *) &playInfo))) {
		framecount = playInfo.frame_count;
		return true;
	}
	return false;
}

bool Output::SwitchAudio(AVStream *stream)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);
	if (stream == audioStream)
		return true;
	if (audiofd > -1) {
		dioctl(audiofd, AUDIO_STOP, NULL);
		dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);
	}
	audioStream = stream;
	if (stream) {
		audioWriter = Writer::GetWriter(stream->codec->codec_id, stream->codec->codec_type);
		audioWriter->Init();
		if (audiofd > -1) {
			dioctl (audiofd, AUDIO_SET_ENCODING, Writer::GetAudioEncoding(stream->codec->codec_id));
			dioctl(audiofd, AUDIO_PLAY, NULL);
		}
	}
	return true;
}

bool Output::SwitchVideo(AVStream *stream)
{
fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
	if (stream == videoStream)
		return true;
	if (videofd > -1) {
		dioctl(videofd, VIDEO_STOP, NULL);
		dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);
	}
	videoStream = stream;
	if (stream) {
		videoWriter = Writer::GetWriter(stream->codec->codec_id, stream->codec->codec_type);
		videoWriter->Init();
		if (videofd > -1) {
			dioctl(videofd, VIDEO_SET_ENCODING, Writer::GetVideoEncoding(stream->codec->codec_id));
			dioctl(videofd, VIDEO_PLAY, NULL);
		}
	}
	return true;
}

bool Output::Write(AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t &Pts)
{
//fprintf(stderr, "%s %d %s\n", __FILE__,__LINE__,__func__);
	switch (stream->codec->codec_type) {
		case AVMEDIA_TYPE_VIDEO: {
			OpenThreads::ScopedLock<OpenThreads::Mutex> v_lock(videoMutex);
			if (videofd > -1 && videoWriter)
				return videoWriter->Write(videofd, avfc, stream, packet, Pts);
			return false;
		}
		case AVMEDIA_TYPE_AUDIO: {
			OpenThreads::ScopedLock<OpenThreads::Mutex> a_lock(audioMutex);
			if (audiofd > -1 && audioWriter)
				return audioWriter->Write(audiofd, avfc, stream, packet, Pts);
			return false;
		}
		default:
			return false;
	}
}

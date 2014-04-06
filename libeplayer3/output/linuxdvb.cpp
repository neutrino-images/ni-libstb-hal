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

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define LINUXDVB_DEBUG

static const char FILENAME[] = __FILE__;

#define cERR_LINUXDVB_NO_ERROR      0
#define cERR_LINUXDVB_ERROR        -1

#define dioctl(fd,req,arg) ({		\
	int _r = ioctl(fd,req,arg); \
	if (_r)				\
		fprintf(stderr, "%s %d: ioctl '%s' failed: %d (%s)\n", __FILE__, __LINE__, #req, errno, strerror(errno)); \
	_r; \
})

static const char VIDEODEV[] = "/dev/dvb/adapter0/video0";
static const char AUDIODEV[] = "/dev/dvb/adapter0/audio0";

static int videofd = -1;
static int audiofd = -1;

static Writer *videoWriter = NULL;
static Writer *audioWriter = NULL;
OpenThreads::Mutex audioMutex, videoMutex;

AVStream *audioStream = NULL;
AVStream *videoStream = NULL;

unsigned long long int sCURRENT_PTS = 0;

pthread_mutex_t LinuxDVBmutex;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
int LinuxDvbStop();

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

void getLinuxDVBMutex(const char *filename
		      __attribute__ ((unused)), const char *function
		      __attribute__ ((unused)), int line
		      __attribute__ ((unused)))
{


    pthread_mutex_lock(&LinuxDVBmutex);

}

void releaseLinuxDVBMutex(const char *filename
			  __attribute__ ((unused)), const char *function
			  __attribute__ ((unused)), int line
			  __attribute__ ((unused)))
{
    pthread_mutex_unlock(&LinuxDVBmutex);


}

int LinuxDvbOpen(Player * context __attribute__ ((unused)), char *)
{
    if (videofd < 0) {
	videofd = open(VIDEODEV, O_RDWR);

	if (videofd < 0) {
	    return cERR_LINUXDVB_ERROR;
	}

	dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);
	dioctl(videofd, VIDEO_SELECT_SOURCE, (void *) VIDEO_SOURCE_MEMORY);
	dioctl(videofd, VIDEO_SET_STREAMTYPE, (void *) STREAM_TYPE_PROGRAM);
	dioctl(videofd, VIDEO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);

    }
    if (audiofd < 0) {
	audiofd = open(AUDIODEV, O_RDWR);

	if (audiofd < 0) {

	    if (videofd < 0)
		close(videofd);
	    return cERR_LINUXDVB_ERROR;
	}

	dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);
	dioctl(audiofd, AUDIO_SELECT_SOURCE, (void *) AUDIO_SOURCE_MEMORY);
	dioctl(audiofd, AUDIO_SET_STREAMTYPE, (void *) STREAM_TYPE_PROGRAM);
    }

    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbClose(Player * context, char *)
{

    /* closing stand alone is not allowed, so prevent
     * user from closing and dont call stop. stop will
     * set default values for us (speed and so on).
     */
    LinuxDvbStop();

    getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    if (videofd > -1) {
	close(videofd);
	videofd = -1;
    }
    if (audiofd > -1) {
	close(audiofd);
	audiofd = -1;
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    return cERR_LINUXDVB_NO_ERROR;
}

int LinuxDvbPlay(Player * context, char *)
{
    int ret = cERR_LINUXDVB_NO_ERROR;

    AVStream *_videoStream = videoStream;
    AVStream *_audioStream = audioStream;
    if (_videoStream)
	    videoWriter = Writer::GetWriter(_videoStream->codec->codec_id, _videoStream->codec->codec_type);
    if (_audioStream)
	    audioWriter = Writer::GetWriter(_audioStream->codec->codec_id, _audioStream->codec->codec_type);

    if (_videoStream && videofd > -1) {

	if (dioctl(videofd, VIDEO_SET_ENCODING, videoWriter->GetVideoEncoding(_videoStream->codec->codec_id))
	||  dioctl(videofd, VIDEO_PLAY, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }
    if (_audioStream && audiofd > -1) {
	if (dioctl(audiofd, AUDIO_SET_ENCODING, audioWriter->GetAudioEncoding(_audioStream->codec->codec_id))
	||  dioctl(audiofd, AUDIO_PLAY, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }

    return ret;
}

int LinuxDvbStop()
{
    int ret = cERR_LINUXDVB_NO_ERROR;

    getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    if (videofd > -1) {
	dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);

	/* set back to normal speed (end trickmodes) */
	dioctl(videofd, VIDEO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);

	if (dioctl(videofd, VIDEO_STOP, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }
    if (audiofd > -1) {
	dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);

	/* set back to normal speed (end trickmodes) */
	dioctl(audiofd, AUDIO_SET_SPEED, DVB_SPEED_NORMAL_PLAY);

	if (dioctl(audiofd, AUDIO_STOP, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    return ret;
}

int LinuxDvbPause(Player * context __attribute__ ((unused)), char *)
{
    int ret = cERR_LINUXDVB_NO_ERROR;


    getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    if (videofd > -1) {
	if (dioctl(videofd, VIDEO_FREEZE, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }
    if (audiofd > -1) {
	if (dioctl(audiofd, AUDIO_PAUSE, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    return ret;
}

int LinuxDvbContinue(Player * context __attribute__ ((unused)), char *)
{
    int ret = cERR_LINUXDVB_NO_ERROR;


    if (videofd > -1) {
	if (dioctl(videofd, VIDEO_CONTINUE, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }
    if (audiofd > -1) {
	if (dioctl(audiofd, AUDIO_CONTINUE, NULL))
	    ret = cERR_LINUXDVB_ERROR;
    }



    return ret;
}

int LinuxDvbReverseDiscontinuity(Player * context
				 __attribute__ ((unused)), int *surplus)
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    int dis_type = VIDEO_DISCONTINUITY_CONTINUOUS_REVERSE | *surplus;


    dioctl(videofd, VIDEO_DISCONTINUITY, (void *) dis_type);


    return ret;
}

int LinuxDvbAudioMute(Player * context __attribute__ ((unused)), char *flag)
{
    int ret = cERR_LINUXDVB_NO_ERROR;


    if (audiofd != -1) {
	if (*flag == '1') {
	    //AUDIO_SET_MUTE has no effect with new player
	    //if (ioctl(audiofd, AUDIO_SET_MUTE, 1) == -1)
	    if (dioctl(audiofd, AUDIO_STOP, NULL))
		ret = cERR_LINUXDVB_ERROR;
	} else {
	    //AUDIO_SET_MUTE has no effect with new player
	    //if (ioctl(audiofd, AUDIO_SET_MUTE, 0) == -1)
	    if (dioctl(audiofd, AUDIO_PLAY, NULL))
		ret = cERR_LINUXDVB_ERROR;
	}
    }


    return ret;
}


int LinuxDvbFlush(Player * context __attribute__ ((unused)), char *)
{

	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	if (videofd > -1)
	    dioctl(videofd, VIDEO_FLUSH, NULL);

	if (audiofd > -1)
	    dioctl(audiofd, AUDIO_FLUSH, NULL);

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);


    return cERR_LINUXDVB_NO_ERROR;
}

#ifndef use_set_speed_instead_ff
int LinuxDvbFastForward(Player * context, char *)
{
    int ret = cERR_LINUXDVB_NO_ERROR;

    if (videofd > -1) {

	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	/* konfetti comment: speed is a value given in skipped frames */

	if (dioctl(videofd, VIDEO_FAST_FORWARD, context->playback->Speed))
	    ret = cERR_LINUXDVB_ERROR;

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    }


    return ret;
}
#else

static unsigned int SpeedList[] = { 1000, 1100, 1200, 1300, 1500, 2000, 3000, 4000, 5000, 8000, 12000, 16000, 125, 250, 500, 700, 800, 900 };

int LinuxDvbFastForward(Player * context, char *type)
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    int speedIndex;
    unsigned char video = !strcmp("video", type);
    unsigned char audio = !strcmp("audio", type);


    if (video && videofd != -1) {

	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	speedIndex = context->playback->Speed % (sizeof(SpeedList) / sizeof(int));


	if (dioctl(videofd, VIDEO_SET_SPEED, SpeedList[speedIndex]))
	    ret = cERR_LINUXDVB_ERROR;

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    }

    if (audio && audiofd != -1) {

	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	speedIndex =
	    context->playback->Speed % (sizeof(SpeedList) / sizeof(int));


	if (dioctl(audiofd, AUDIO_SET_SPEED, SpeedList[speedIndex])) {
	    ret = cERR_LINUXDVB_ERROR;
	}

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    }


    return ret;
}
#endif

int LinuxDvbSlowMotion(Player * context, char *type)
{
    int ret = cERR_LINUXDVB_NO_ERROR;


	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	if (videofd > -1) {
	    if (dioctl (videofd, VIDEO_SLOWMOTION, context->playback->SlowMotion)) {
		ret = cERR_LINUXDVB_ERROR;
	    }
	}

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);


    return ret;
}

int LinuxDvbAVSync(Player * context, char *type __attribute__ ((unused)))
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    /* konfetti: this one is dedicated to audiofd so we
     * are ignoring what is given by type! I think we should
     * remove this param. Therefor we should add a variable
     * setOn or something like that instead, this would remove
     * using a variable inside the structure.
     */
    if (audiofd > -1) {
	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	if (dioctl(audiofd, AUDIO_SET_AV_SYNC, context->playback->AVSync))
	    ret = cERR_LINUXDVB_ERROR;

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);
    }

    return ret;
}

int LinuxDvbClearAudio()
{
    int ret = cERR_LINUXDVB_NO_ERROR;

	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	if (audiofd > -1) {
	    if (dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL))
		ret = cERR_LINUXDVB_ERROR;
	}

	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    return ret;
}

int LinuxDvbClearVideo()
{
    int ret = cERR_LINUXDVB_NO_ERROR;


	getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

	if (videofd > -1) {
	    if (dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL))
		ret = cERR_LINUXDVB_ERROR;
	}
	releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);


    return ret;
}

int LinuxDvbClear()
{
    int ret = LinuxDvbClearAudio();
    if (ret == cERR_LINUXDVB_NO_ERROR)
    	ret = LinuxDvbClearVideo();
    else
    	LinuxDvbClearVideo();
    return ret;
}

int LinuxDvbPts(Player * context
		__attribute__ ((unused)), unsigned long long int *pts)
{
    int ret = cERR_LINUXDVB_ERROR;


    // pts is a non writting requests and can be done in parallel to other requests
    //getLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    if ((videofd > -1 && !dioctl(videofd, VIDEO_GET_PTS, (void *) &sCURRENT_PTS))
     || (audiofd > -1 && !dioctl(audiofd, AUDIO_GET_PTS, (void *) &sCURRENT_PTS)))
	ret = cERR_LINUXDVB_NO_ERROR;
    else
	sCURRENT_PTS = 0;

    *((unsigned long long int *) pts) = (unsigned long long int) sCURRENT_PTS;

    //releaseLinuxDVBMutex(FILENAME, __FUNCTION__,__LINE__);

    return ret;
}

int LinuxDvbGetFrameCount(Player * context
			  __attribute__ ((unused)),
			  unsigned long long int *frameCount)
{
    int ret = cERR_LINUXDVB_NO_ERROR;
    dvb_play_info_t playInfo;


    getLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    if (videofd > -1) {
	if (dioctl(videofd, VIDEO_GET_PLAY_INFO, (void *) &playInfo))
	    ret = cERR_LINUXDVB_ERROR;
    } else if (audiofd > -1) {
	if (dioctl(audiofd, AUDIO_GET_PLAY_INFO, (void *) &playInfo))
	    ret = cERR_LINUXDVB_ERROR;
    } else
	ret = cERR_LINUXDVB_ERROR;

    if (ret == cERR_LINUXDVB_NO_ERROR)
	*((unsigned long long int *) frameCount) = playInfo.frame_count;

    releaseLinuxDVBMutex(FILENAME, __FUNCTION__, __LINE__);

    return ret;
}

bool output_switch_audio(AVStream *stream)
{
	if (audiofd < 0)
		return false;
	audioMutex.lock();
	audioStream = stream;
	audioWriter = Writer::GetWriter(stream->codec->codec_id, stream->codec->codec_type);
	audio_encoding_t enc = Writer::GetAudioEncoding(stream->codec->codec_id);
	dioctl(audiofd, AUDIO_STOP, NULL);
	dioctl(audiofd, AUDIO_CLEAR_BUFFER, NULL);
	dioctl (audiofd, AUDIO_SET_ENCODING, (void *) enc);
	dioctl(audiofd, AUDIO_PLAY, NULL);
	audioMutex.unlock();
	return true;
}

bool output_switch_video(AVStream *stream)
{
	if (videofd < 0)
		return false;
	videoMutex.lock();
	videoStream = stream;
	videoWriter = Writer::GetWriter(stream->codec->codec_id, stream->codec->codec_type);
	video_encoding_t enc = Writer::GetVideoEncoding(stream->codec->codec_id);
	dioctl(videofd, VIDEO_STOP, NULL);
	dioctl(videofd, VIDEO_CLEAR_BUFFER, NULL);
	dioctl(videofd, VIDEO_SET_ENCODING, (void *) enc);
	dioctl(videofd, VIDEO_PLAY, NULL);
	videoMutex.unlock();
	return true;
}

static bool Write(AVFormatContext *avfc, AVStream *stream, AVPacket *packet, int64_t &Pts)
{
	switch (stream->codec->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
			return videoWriter->Write(videofd, avfc, stream, packet, Pts);
		case AVMEDIA_TYPE_AUDIO:
			return audioWriter->Write(audiofd, avfc, stream, packet, Pts);
		default:
			return false;
	}
}

static int reset(Player * context)
{
	if (videoWriter)
		videoWriter->Init();
	if (audioWriter)
		audioWriter->Init();
	return cERR_LINUXDVB_NO_ERROR;
}

static int Command(Player *context, OutputCmd_t command, const char *argument)
{
    int ret = cERR_LINUXDVB_NO_ERROR;


    switch (command) {
    case OUTPUT_OPEN:{
	    ret = LinuxDvbOpen(context, (char *) argument);
	    break;
	}
    case OUTPUT_CLOSE:{
	    ret = LinuxDvbClose(context, (char *) argument);
	    reset(context);
	    sCURRENT_PTS = 0;
	    break;
	}
    case OUTPUT_PLAY:{		// 4
	    sCURRENT_PTS = 0;
	    ret = LinuxDvbPlay(context, (char *) argument);
	    break;
	}
    case OUTPUT_STOP:{
	    reset(context);
	    ret = LinuxDvbStop();
	    sCURRENT_PTS = 0;
	    break;
	}
    case OUTPUT_FLUSH:{
	    ret = LinuxDvbFlush(context, (char *) argument);
	    reset(context);
	    sCURRENT_PTS = 0;
	    break;
	}
    case OUTPUT_PAUSE:{
	    ret = LinuxDvbPause(context, (char *) argument);
	    break;
	}
    case OUTPUT_CONTINUE:{
	    ret = LinuxDvbContinue(context, (char *) argument);
	    break;
	}
    case OUTPUT_FASTFORWARD:{
	    return LinuxDvbFastForward(context, (char *) argument);
	    break;
	}
    case OUTPUT_AVSYNC:{
	    ret = LinuxDvbAVSync(context, (char *) argument);
	    break;
	}
    case OUTPUT_CLEAR:{
	    ret = LinuxDvbClear();
	    reset(context);
	    sCURRENT_PTS = 0;
	    break;
	}
    case OUTPUT_PTS:{
	    unsigned long long int pts = 0;
	    ret = LinuxDvbPts(context, &pts);
	    *((unsigned long long int *) argument) =
		(unsigned long long int) pts;
	    break;
	}
    case OUTPUT_SLOWMOTION:{
	    return LinuxDvbSlowMotion(context, (char *) argument);
	    break;
	}
    case OUTPUT_AUDIOMUTE:{
	    return LinuxDvbAudioMute(context, (char *) argument);
	    break;
	}
    case OUTPUT_DISCONTINUITY_REVERSE:{
	    return LinuxDvbReverseDiscontinuity(context, (int *) argument);
	    break;
	}
    case OUTPUT_GET_FRAME_COUNT:{
	    unsigned long long int frameCount = 0;
	    ret = LinuxDvbGetFrameCount(context, &frameCount);
	    *((unsigned long long int *) argument) =
		(unsigned long long int) frameCount;
	    break;
	}
    default:
	ret = cERR_LINUXDVB_ERROR;
	break;
    }


    return ret;
}

static const char *LinuxDvbCapabilities[] = { "audio", "video", NULL };

struct Output_s LinuxDvbOutput = {
    "LinuxDvb",
    &Command,
    &Write,
    LinuxDvbCapabilities
};

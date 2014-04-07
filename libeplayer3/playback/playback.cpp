/*
 * GPL
 * duckbox 2010
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>

#include "playback.h"
#include "player.h"
#include "misc.h"

#define cMaxSpeed_ff   128	/* fixme: revise */
#define cMaxSpeed_fr   -320	/* fixme: revise */

static pthread_t supervisorThread;

Playback::Playback()
{
	hasThreadStarted = 0;
}

void *Playback::SupervisorThread(void *arg)
{
	Playback *me = (Playback *) arg;
	me->hasThreadStarted = 1;

	me->context->input.Play();
	me->hasThreadStarted = 2;
	me->Terminate();

	fprintf(stderr, "terminating\n");
	me->hasThreadStarted = 0;
	pthread_exit(NULL);
}

bool Playback::Open(const char *Url)
{
    if (context->playback.isPlaying)
	Stop(); // shouldn't happen. Most definitely a bug

    fprintf(stderr, "URL=%s\n", Url);

    if (context->playback.isPlaying) {	// shouldn't happen
	fprintf(stderr, "playback already running\n");
	return false;
    }

    context->playback.isHttp = 0;

    if (!strncmp("file://", Url, 7) || !strncmp("myts://", Url, 7)) {
	if (!strncmp("myts://", Url, 7)) {
	    context->playback.url = "file";
	    context->playback.url += (Url + 4);
	    context->playback.noprobe = 1;
	} else {
	    context->playback.noprobe = 0;
	    context->playback.url = Url;
	}
    } else if (strstr(Url, "://")) {
	context->playback.isHttp = 1;
	if (!strncmp("mms://", Url, 6)) {
	    context->playback.url = "mmst";
	    context->playback.url += (Url + 3);
	} else
	    context->playback.url = Url;
    } else {
	fprintf(stderr, "Unknown stream (%s)\n", Url);
	return false;
    }
    context->manager.clearTracks();

    if (!context->input.Init(context->playback.url.c_str()))
	return false;

    fprintf(stderr, "exiting with value 0\n");

    return true;
}

bool Playback::Close()
{
    bool ret = true;

    context->playback.isPaused = 0;
    context->playback.isPlaying = 0;
    context->playback.isForwarding = 0;
    context->playback.isBackWard = 0;
    context->playback.isSlowMotion = 0;
    context->playback.Speed = 0;
    context->playback.url.clear();

    return ret;
}

bool Playback::Play()
{
    pthread_attr_t attr;
    bool ret = true;

    if (!context->playback.isPlaying) {
	context->playback.AVSync = 1;
	context->output.AVSync(true);

	context->playback.isCreationPhase = 1;	// allows the created thread to go into wait mode
	ret = context->output.Play();

	if (!ret) {
	    context->playback.isCreationPhase = 0;	// allow thread to go into next state
	} else {
	    context->playback.isPlaying = 1;
	    context->playback.isPaused = 0;
	    context->playback.isForwarding = 0;
	    if (context->playback.isBackWard) {
		context->playback.isBackWard = 0;
		context->output.Mute(false);
	    }
	    context->playback.isSlowMotion = 0;
	    context->playback.Speed = 1;

	    if (hasThreadStarted == 0) {
		int error;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		if ((error = pthread_create(&supervisorThread, &attr, SupervisorThread, &context->playback))) {
		    fprintf(stderr, "Error creating thread, error:%d:%s\n", error, strerror(error));

		    ret = false;
		} else {
		    fprintf(stderr, "Created thread\n");
		}
	    }

	    fprintf(stderr, "clearing isCreationPhase!\n");

	    context->playback.isCreationPhase = 0;	// allow thread to go into next state
	}

    } else {
	fprintf(stderr,"playback already running\n");
	ret = false;
    }

    fprintf(stderr, "exiting with value %d\n", ret);

    return ret;
}

bool Playback::Pause()
{
    bool ret = true;

    if (context->playback.isPlaying && !context->playback.isPaused) {

	if (context->playback.isSlowMotion)
	    context->output.Clear();

	context->output.Pause();

	context->playback.isPaused = 1;
	//context->playback.isPlaying  = 1;
	context->playback.isForwarding = 0;
	if (context->playback.isBackWard) {
	    context->playback.isBackWard = 0;
	    context->output.Mute(false);
	}
	context->playback.isSlowMotion = 0;
	context->playback.Speed = 1;
    } else {
	fprintf(stderr,"playback not playing or already in pause mode\n");
	ret = false;
    }

    fprintf(stderr, "exiting with value %d\n", ret);

    return ret;
}

bool Playback::Continue()
{
    int ret = true;

    if (context->playback.isPlaying && (context->playback.isPaused || context->playback.isForwarding
	 || context->playback.isBackWard || context->playback.isSlowMotion)) {

	if (context->playback.isSlowMotion)
	    context->output.Clear();

	context->output.Continue();

	context->playback.isPaused = 0;
	//context->playback.isPlaying  = 1;
	context->playback.isForwarding = 0;
	if (context->playback.isBackWard) {
	    context->playback.isBackWard = 0;
	    context->output.Mute(false);
	}
	context->playback.isSlowMotion = 0;
	context->playback.Speed = 1;
    } else {
	fprintf(stderr,"continue not possible\n");
	ret = false;
    }

    return ret;
}

bool Playback::Stop()
{
    bool ret = true;
    int wait_time = 20;

    if (context->playback.isPlaying) {

	context->playback.isPaused = 0;
	context->playback.isPlaying = 0;
	context->playback.isForwarding = 0;
	if (context->playback.isBackWard) {
	    context->playback.isBackWard = 0;
	    context->output.Mute(false);
	}
	context->playback.isSlowMotion = 0;
	context->playback.Speed = 0;

	context->output.Stop();
	context->input.Stop();

    } else {
	fprintf(stderr,"stop not possible\n");
	ret = false;
    }

    while ((hasThreadStarted == 1) && (--wait_time) > 0) {
	fprintf(stderr, "Waiting for supervisor thread to terminate itself, will try another %d times\n", wait_time);

	usleep(100000);
    }

    if (wait_time == 0) {
	fprintf(stderr,"Timeout waiting for thread!\n");

	ret = false;
    }

    fprintf(stderr, "exiting with value %d\n", ret);

    return ret;
}

// FIXME
bool Playback::Terminate()
{
    bool ret = true;
    int wait_time = 20;

    fprintf(stderr, "\n");

    if (context->playback.isPlaying) {

	if (!context->playback.abortRequested && !context->output.Flush()) {
	    fprintf(stderr,"failed to flush output.\n");
	}

	ret = context->input.Stop();
	context->playback.isPaused = 0;
	context->playback.isPlaying = 0;
	context->playback.isForwarding = 0;
	context->playback.isBackWard = 0;
	context->playback.isSlowMotion = 0;
	context->playback.Speed = 0;

    } else {
	fprintf(stderr,"%p %d\n", context, context->playback.isPlaying);

	/* fixme: konfetti: we should return an error here but this seems to be a condition which
	 * can happen and is not a real error, which leads to a dead neutrino. should investigate
	 * here later.
	 */
    }

    while ((hasThreadStarted == 1) && (--wait_time) > 0) {
	fprintf(stderr, "Waiting for supervisor thread to terminate itself, will try another %d times\n", wait_time);

	usleep(100000);
    }

    if (wait_time == 0) {
	fprintf(stderr,"Timeout waiting for thread!\n");

	ret = false;
    }

    fprintf(stderr, "exiting with value %d\n", ret);

    return ret;
}

bool Playback::FastForward(int speed)
{
    int ret = true;

    /* Audio only forwarding not supported */
    if (context->playback.isVideo && !context->playback.isHttp && !context->playback.isBackWard
	&& (!context->playback.isPaused || context-> playback.isPlaying)) {

	if ((speed <= 0) || (speed > cMaxSpeed_ff)) {
	    fprintf(stderr, "speed %d out of range (1 - %d) \n", speed, cMaxSpeed_ff);
	    return false;
	}

	context->playback.isForwarding = 1;
	context->playback.Speed = speed;
	context->output.FastForward(speed);
    } else {
	fprintf(stderr,"fast forward not possible\n");
	ret = false;
    }

    return ret;
}

bool Playback::FastBackward(int speed)
{
    bool ret = true;

    /* Audio only reverse play not supported */
    if (context->playback.isVideo && !context->playback.isForwarding
	&& (!context->playback.isPaused || context->playback.isPlaying)) {

	if ((speed > 0) || (speed < cMaxSpeed_fr)) {
	    fprintf(stderr, "speed %d out of range (0 - %d) \n", speed, cMaxSpeed_fr);
	    return false;
	}

	if (speed == 0) {
	    context->playback.isBackWard = false;
	    context->playback.Speed = false;	/* reverse end */
	} else {
	    context->playback.Speed = speed;
	    context->playback.isBackWard = true;
	}

	context->output.Clear();
#if 0
	if (context->output->Command(context, OUTPUT_REVERSE, NULL) < 0) {
	    fprintf(stderr,"OUTPUT_REVERSE failed\n");
	    context->playback.isBackWard = 0;
	    context->playback.Speed = 1;
	    ret = false;
	}
#endif
    } else {
	fprintf(stderr,"fast backward not possible\n");
	ret = false;
    }

    if (context->playback.isBackWard)
	context->output.Mute(true);

    return ret;
}

bool Playback::SlowMotion(int repeats)
{
	if (context->playback.isVideo && !context->playback.isHttp && context->playback.isPlaying) {
		if (context->playback.isPaused)
			Continue();

		switch (repeats) {
		case 2:
		case 4:
		case 8:
			context->playback.isSlowMotion = true;
			break;
		default:
			repeats = 0;
		}

		context->output.SlowMotion(repeats);
		return true;
	}
	fprintf(stderr, "slowmotion not possible\n");
	return false;
}

bool Playback::Seek(float pos, bool absolute)
{
    context->output.Clear();
    return context->input.Seek(pos, absolute);
}

bool Playback::GetPts(int64_t &pts)
{
	if (context->playback.isPlaying)
		return context->output.GetPts(pts);
	return false;
}

bool Playback::GetFrameCount(int64_t &frameCount)
{
	if (context->playback.isPlaying)
		return context->output.GetFrameCount(frameCount);
	return false;
}

bool Playback::GetDuration(double &duration)
{
    duration = -1;
    if (context->playback.isPlaying)
	    return context->input.GetDuration(duration);
    return false;
}

bool Playback::SwitchVideo(int pid)
{
	Track *track = context->manager.getVideoTrack(pid);
	if (track)
		context->input.SwitchVideo(track);
	return !!track;
}

bool Playback::SwitchAudio(int pid)
{
	Track *track = context->manager.getAudioTrack(pid);
	if (track)
		context->input.SwitchAudio(track);
	return !!track;
}

bool Playback::SwitchSubtitle(int pid)
{
	Track *track = context->manager.getSubtitleTrack(pid);
	if (track)
		context->input.SwitchSubtitle(track);
	return !!track;
}

bool Playback::SwitchTeletext(int pid)
{
	Track *track = context->manager.getTeletextTrack(pid);
	if (track)
		context->input.SwitchTeletext(track);
	return !!track;
}

bool Playback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	return context->input.GetMetadata(keys, values);
}

bool Player::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
	input.UpdateTracks();
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(chapterMutex);
	for (std::vector<Chapter>::iterator it = chapters.begin(); it != chapters.end(); ++it) {
		positions.push_back(1000 * it->start);
		titles.push_back(it->title);
	}
	return true;
}

void Player::SetChapters(std::vector<Chapter> &Chapters)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(chapterMutex);
	chapters = Chapters;
}

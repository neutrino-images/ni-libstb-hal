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

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define PLAYBACK_DEBUG

static short debug_level = 20;

static const char *FILENAME = "playback.c";
#ifdef PLAYBACK_DEBUG
#define playback_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, FILENAME, __FUNCTION__, ## x); } while (0)
#else
#define playback_printf(level, fmt, x...)
#endif

#ifndef PLAYBACK_SILENT
#define playback_err(fmt, x...) do { printf("[%s:%s] " fmt, FILENAME, __FUNCTION__, ## x); } while (0)
#else
#define playback_err(fmt, x...)
#endif

#define cERR_PLAYBACK_NO_ERROR      0
#define cERR_PLAYBACK_ERROR        -1

#define cMaxSpeed_ff   128	/* fixme: revise */
#define cMaxSpeed_fr   -320	/* fixme: revise */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static pthread_t supervisorThread;
static int hasThreadStarted = 0;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
static int PlaybackTerminate(Player * context);

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

/* **************************** */
/* Supervisor Thread            */
/* **************************** */

static void *SupervisorThread(void *arg)
{
    Player *context = (Player *) arg;
    hasThreadStarted = 1;

    playback_printf(10, ">\n");

    while (context && context->playback && context->playback->isPlaying
	   && !(context->playback->abortRequested | context->playback->abortPlayback))
	usleep(100000);

    playback_printf(10, "<\n");

    hasThreadStarted = 2;
    PlaybackTerminate(context);

    playback_printf(0, "terminating\n");
    hasThreadStarted = 0;
    pthread_exit(NULL);
}

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int PlaybackStop(Player * context);

static int PlaybackOpen(Player * context, char *uri)
{
    if (context->playback->isPlaying)
	PlaybackStop(context);

    playback_printf(10, "URI=%s\n", uri);

    if (context->playback->isPlaying) {	// shouldn't happen
	playback_err("playback already running\n");
	return cERR_PLAYBACK_ERROR;
    }

    context->playback->isHttp = 0;

    if (!strncmp("file://", uri, 7) || !strncmp("myts://", uri, 7)) {
	if (!strncmp("myts://", uri, 7)) {
	    context->playback->uri = "file";
	    context->playback->uri += (uri + 4);
	    context->playback->noprobe = 1;
	} else {
	    context->playback->noprobe = 0;
	    context->playback->uri = uri;
	}
    } else if (strstr(uri, "://")) {
	context->playback->isHttp = 1;
	if (!strncmp("mms://", uri, 6)) {
	    context->playback->uri = "mmst";
	    context->playback->uri += (uri + 3);
	} else
	    context->playback->uri = uri;
    } else {
	playback_err("Unknown stream (%s)\n", uri);
	return cERR_PLAYBACK_ERROR;
    }
    context->manager.clearTracks();

    if ((context->container->Command(context, CONTAINER_ADD, NULL) < 0)
	|| (!context->container->selectedContainer)
	|| (context->container->selectedContainer->Command(context, CONTAINER_INIT, context->playback->uri.c_str()) < 0))
	return cERR_PLAYBACK_ERROR;

    playback_printf(10, "exiting with value 0\n");

    return cERR_PLAYBACK_NO_ERROR;
}

static int PlaybackClose(Player * context)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->container->Command(context, CONTAINER_DEL, NULL) < 0) {
	playback_err("container delete failed\n");
    }


    context->playback->isPaused = 0;
    context->playback->isPlaying = 0;
    context->playback->isForwarding = 0;
    context->playback->BackWard = 0;
    context->playback->SlowMotion = 0;
    context->playback->Speed = 0;
    context->playback->uri.clear();

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackPlay(Player * context)
{
    pthread_attr_t attr;
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (!context->playback->isPlaying) {
	context->playback->AVSync = 1;
	context->output.AVSync(true);

	context->playback->isCreationPhase = 1;	// allows the created thread to go into wait mode
	ret = context->output.Play();

	if (!ret) {
	    context->playback->isCreationPhase = 0;	// allow thread to go into next state
	} else {
	    context->playback->isPlaying = 1;
	    context->playback->isPaused = 0;
	    context->playback->isForwarding = 0;
	    if (context->playback->BackWard) {
		context->playback->BackWard = 0;
		context->output.Mute(false);
	    }
	    context->playback->SlowMotion = 0;
	    context->playback->Speed = 1;

	    if (hasThreadStarted == 0) {
		int error;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr,
					    PTHREAD_CREATE_DETACHED);

		if ((error =
		     pthread_create(&supervisorThread, &attr, SupervisorThread, context)) != 0) {
		    playback_printf(10,
				    "Error creating thread, error:%d:%s\n",
				    error, strerror(error));

		    ret = cERR_PLAYBACK_ERROR;
		} else {
		    playback_printf(10, "Created thread\n");
		}
	    }

	    playback_printf(10, "clearing isCreationPhase!\n");

	    context->playback->isCreationPhase = 0;	// allow thread to go into next state

	    ret =
		context->container->selectedContainer->Command(context,
							       CONTAINER_PLAY,
							       NULL);
	    if (ret != 0) {
		playback_err("CONTAINER_PLAY failed!\n");
	    }

	}

    } else {
	playback_err("playback already running\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackPause(Player * context)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->playback->isPlaying && !context->playback->isPaused) {

	if (context->playback->SlowMotion)
	    context->output.Clear();

	context->output.Pause();

	context->playback->isPaused = 1;
	//context->playback->isPlaying  = 1;
	context->playback->isForwarding = 0;
	if (context->playback->BackWard) {
	    context->playback->BackWard = 0;
	    context->output.Mute(false);
	}
	context->playback->SlowMotion = 0;
	context->playback->Speed = 1;
    } else {
	playback_err("playback not playing or already in pause mode\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackContinue(Player * context)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    if (context->playback->isPlaying &&
	(context->playback->isPaused || context->playback->isForwarding
	 || context->playback->BackWard
	 || context->playback->SlowMotion)) {

	if (context->playback->SlowMotion)
	    context->output.Clear();

	context->output.Continue();

	context->playback->isPaused = 0;
	//context->playback->isPlaying  = 1;
	context->playback->isForwarding = 0;
	if (context->playback->BackWard) {
	    context->playback->BackWard = 0;
	    context->output.Mute(false);
	}
	context->playback->SlowMotion = 0;
	context->playback->Speed = 1;
    } else {
	playback_err("continue not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackStop(Player * context)
{
    int ret = cERR_PLAYBACK_NO_ERROR;
    int wait_time = 20;

    playback_printf(10, "\n");

    if (context && context->playback && context->playback->isPlaying) {

	context->playback->isPaused = 0;
	context->playback->isPlaying = 0;
	context->playback->isForwarding = 0;
	if (context->playback->BackWard) {
	    context->playback->BackWard = 0;
	    context->output.Mute(false);
	}
	context->playback->SlowMotion = 0;
	context->playback->Speed = 0;

	context->output.Stop();
	context->container->selectedContainer->Command(context, CONTAINER_STOP, NULL);

    } else {
	playback_err("stop not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    while ((hasThreadStarted == 1) && (--wait_time) > 0) {
	playback_printf(10,
			"Waiting for supervisor thread to terminate itself, will try another %d times\n",
			wait_time);

	usleep(100000);
    }

    if (wait_time == 0) {
	playback_err("Timeout waiting for thread!\n");

	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackTerminate(Player * context)
{
    int ret = cERR_PLAYBACK_NO_ERROR;
    int wait_time = 20;

    playback_printf(20, "\n");

    if (context && context->playback && context->playback->isPlaying) {
	//First Flush and than delete container, else e2 cant read length of file anymore

	if (!context->playback->abortRequested && !context->output.Flush()) {
	    playback_err("failed to flush output.\n");
	}

	ret =
	    context->container->selectedContainer->Command(context,
							   CONTAINER_STOP,
							   NULL);
	context->playback->isPaused = 0;
	context->playback->isPlaying = 0;
	context->playback->isForwarding = 0;
	context->playback->BackWard = 0;
	context->playback->SlowMotion = 0;
	context->playback->Speed = 0;

    } else {
	playback_err("%p %p %d\n", context, context->playback,
		     context->playback->isPlaying);

	/* fixme: konfetti: we should return an error here but this seems to be a condition which
	 * can happen and is not a real error, which leads to a dead neutrino. should investigate
	 * here later.
	 */
    }

    while ((hasThreadStarted == 1) && (--wait_time) > 0) {
	playback_printf(10,
			"Waiting for supervisor thread to terminate itself, will try another %d times\n",
			wait_time);

	usleep(100000);
    }

    if (wait_time == 0) {
	playback_err("Timeout waiting for thread!\n");

	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackFastForward(Player * context, int *speed)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "speed %d\n", *speed);

    /* Audio only forwarding not supported */
    if (context->playback->isVideo && !context->playback->isHttp
	&& !context->playback->BackWard && (!context->playback->isPaused
					    || context->
					    playback->isPlaying)) {

	if ((*speed <= 0) || (*speed > cMaxSpeed_ff)) {
	    playback_err("speed %d out of range (1 - %d) \n", *speed,
			 cMaxSpeed_ff);
	    return cERR_PLAYBACK_ERROR;
	}

	context->playback->isForwarding = 1;
	context->playback->Speed = *speed;

	playback_printf(20, "Speed: %d x {%d}\n", *speed,
			context->playback->Speed);

	context->output.FastForward(*speed);
    } else {
	playback_err("fast forward not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackFastBackward(Player * context, int *speed)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "speed = %d\n", *speed);

    /* Audio only reverse play not supported */
    if (context->playback->isVideo && !context->playback->isForwarding
	&& (!context->playback->isPaused
	    || context->playback->isPlaying)) {

	if ((*speed > 0) || (*speed < cMaxSpeed_fr)) {
	    playback_err("speed %d out of range (0 - %d) \n", *speed,
			 cMaxSpeed_fr);
	    return cERR_PLAYBACK_ERROR;
	}

	if (*speed == 0) {
	    context->playback->BackWard = 0;
	    context->playback->Speed = 0;	/* reverse end */
	} else {
	    context->playback->Speed = *speed;
	    context->playback->BackWard = 1;

	    playback_printf(1, "S %d B %d\n", context->playback->Speed, context->playback->BackWard);
	}

	context->output.Clear();
#if 0
	if (context->output->Command(context, OUTPUT_REVERSE, NULL) < 0) {
	    playback_err("OUTPUT_REVERSE failed\n");
	    context->playback->BackWard = 0;
	    context->playback->Speed = 1;
	    ret = cERR_PLAYBACK_ERROR;
	}
#endif
    } else {
	playback_err("fast backward not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    if (context->playback->BackWard)
	context->output.Mute(true);
    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackSlowMotion(Player * context, int *speed)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "\n");

    //Audio only forwarding not supported
    if (context->playback->isVideo && !context->playback->isHttp
	&& context->playback->isPlaying) {
	if (context->playback->isPaused)
	    PlaybackContinue(context);

	switch (*speed) {
	case 2:
	    context->playback->SlowMotion = 2;
	    break;
	case 4:
	    context->playback->SlowMotion = 4;
	    break;
	case 8:
	    context->playback->SlowMotion = 8;
	    break;
	}

	playback_printf(20, "SlowMotion: %d x {%d}\n", *speed,
			context->playback->SlowMotion);

	context->output.SlowMotion(*speed);
    } else {
	playback_err("slowmotion not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackSeek(Player * context, float *pos, int absolute)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(10, "pos: %f\n", *pos);

    context->output.Clear();

    if (absolute)
	context->container->selectedContainer->Command(context, CONTAINER_SEEK_ABS, (const char *)pos);
    else
	context->container->selectedContainer->Command(context, CONTAINER_SEEK, (const char *)pos);

    playback_printf(10, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackPts(Player * context, int64_t &pts)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    pts = 0;

    if (context->playback->isPlaying) {
	ret = !context->output.GetPts(pts);
    } else {
	playback_err("not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackGetFrameCount(Player * context, int64_t &frameCount)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    frameCount = 0;

    if (context->playback->isPlaying) {
	ret = !context->output.GetFrameCount(frameCount);
    } else {
	playback_err("not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackLength(Player * context, double *length)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "\n");

    *length = -1;

    if (context->playback->isPlaying) {
	if (context->container && context->container->selectedContainer)
	    context->container->selectedContainer->Command(context,
							   CONTAINER_LENGTH,
							   (const char *)length);
    } else {
	playback_err("not possible\n");
	ret = cERR_PLAYBACK_ERROR;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}

static int PlaybackSwitchAudio(Player * context, int *pid)
{
	Track *track = context->manager.getAudioTrack(*pid);
	if (track)
		context->container->selectedContainer->Command(context, CONTAINER_SWITCH_AUDIO, (const char*) track);
	return 0;
}

static int PlaybackSwitchSubtitle(Player * context, int *pid)
{
	Track *track = context->manager.getSubtitleTrack(*pid);
	if (track)
		context->container->selectedContainer->Command(context, CONTAINER_SWITCH_SUBTITLE, (const char*) track);
	return 0;
}

static int PlaybackSwitchTeletext(Player * context, int *pid)
{
	Track *track = context->manager.getTeletextTrack(*pid);
	if (track)
		context->container->selectedContainer->Command(context, CONTAINER_SWITCH_TELETEXT, (const char*) track);
	return 0;
}

static int PlaybackMetadata(Player * context, char ***metadata)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    if (context->container && context->container->selectedContainer)
	context->container->selectedContainer->Command(context,
						       CONTAINER_METADATA,
						       (const char *)metadata);
    return ret;
}

static int Command(Player *context, PlaybackCmd_t command, void *argument)
{
    int ret = cERR_PLAYBACK_NO_ERROR;

    playback_printf(20, "Command %d\n", command);


    switch (command) {
    case PLAYBACK_OPEN:{
	    ret = PlaybackOpen(context, (char *) argument);
	    break;
	}
    case PLAYBACK_CLOSE:{
	    ret = PlaybackClose(context);
	    break;
	}
    case PLAYBACK_PLAY:{
	    ret = PlaybackPlay(context);
	    break;
	}
    case PLAYBACK_STOP:{
	    ret = PlaybackStop(context);
	    break;
	}
    case PLAYBACK_PAUSE:{	// 4
	    ret = PlaybackPause(context);
	    break;
	}
    case PLAYBACK_CONTINUE:{
	    ret = PlaybackContinue(context);
	    break;
	}
    case PLAYBACK_TERM:{
	    ret = PlaybackTerminate(context);
	    break;
	}
    case PLAYBACK_FASTFORWARD:{
	    ret = PlaybackFastForward(context, (int *) argument);
	    break;
	}
    case PLAYBACK_SEEK:{
	    ret = PlaybackSeek(context, (float *) argument, 0);
	    break;
	}
    case PLAYBACK_SEEK_ABS:{
	    ret = PlaybackSeek(context, (float *) argument, -1);
	    break;
	}
    case PLAYBACK_PTS:{	// 10
	    ret = PlaybackPts(context, *((int64_t *) argument));
	    break;
	}
    case PLAYBACK_LENGTH:{	// 11
	    ret = PlaybackLength(context, (double *) argument);
	    break;
	}
    case PLAYBACK_SWITCH_AUDIO:{
	    ret = PlaybackSwitchAudio(context, (int *) argument);
	    break;
	}
    case PLAYBACK_SWITCH_SUBTITLE:{
	    ret = PlaybackSwitchSubtitle(context, (int *) argument);
	    break;
	}
    case PLAYBACK_METADATA:{
	    ret = PlaybackMetadata(context, (char ***) argument);
	    break;
	}
    case PLAYBACK_SLOWMOTION:{
	    ret = PlaybackSlowMotion(context, (int *) argument);
	    break;
	}
    case PLAYBACK_FASTBACKWARD:{
	    ret = PlaybackFastBackward(context, (int *) argument);
	    break;
	}
    case PLAYBACK_GET_FRAME_COUNT:{
	    // 10
	    ret = PlaybackGetFrameCount(context, *((int64_t *) argument));
	    break;
	}
    case PLAYBACK_SWITCH_TELETEXT:{
	    ret = PlaybackSwitchTeletext(context, (int *) argument);
	    break;
	}
    default:
	playback_err("PlaybackCmd %d not supported!\n", command);
	ret = cERR_PLAYBACK_ERROR;
	break;
    }

    playback_printf(20, "exiting with value %d\n", ret);

    return ret;
}


PlaybackHandler_t PlaybackHandler = {
    "Playback",
    -1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &Command,
    "",
    0,
    0
};

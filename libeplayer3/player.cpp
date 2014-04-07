/*
 * GPL
 * duckbox 2010
 */

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

#include "player.h"
#include "misc.h"

#define cMaxSpeed_ff   128	/* fixme: revise */
#define cMaxSpeed_fr   -320	/* fixme: revise */

static pthread_t supervisorThread;

Player::Player()
{
	input.player = this;
	output.player = this;
	manager.player = this;
	hasThreadStarted = 0;
}

void *Player::SupervisorThread(void *arg)
{
	Player *player = (Player *) arg;
	player->hasThreadStarted = 1;

	player->input.Play();
	player->hasThreadStarted = 2;
	player->Terminate();

	fprintf(stderr, "terminating\n");
	player->hasThreadStarted = 0;
	pthread_exit(NULL);
}

bool Player::Open(const char *Url)
{
	if (isPlaying)
	Stop(); // shouldn't happen. Most definitely a bug

	fprintf(stderr, "URL=%s\n", Url);

	if (isPlaying) {	// shouldn't happen
		fprintf(stderr, "playback already running\n");
	return false;
	}

	isHttp = 0;

	if (!strncmp("file://", Url, 7) || !strncmp("myts://", Url, 7)) {
	if (!strncmp("myts://", Url, 7)) {
		url = "file";
		url += (Url + 4);
		noprobe = 1;
	} else {
		noprobe = 0;
		url = Url;
	}
	} else if (strstr(Url, "://")) {
	isHttp = 1;
	if (!strncmp("mms://", Url, 6)) {
		url = "mmst";
		url += (Url + 3);
	} else
		url = Url;
	} else {
		fprintf(stderr, "Unknown stream (%s)\n", Url);
	return false;
	}
	manager.clearTracks();

	if (!input.Init(url.c_str()))
		return false;

	fprintf(stderr, "exiting with value 0\n");

	return true;
}

bool Player::Close()
{
	bool ret = true;

	isPaused = 0;
	isPlaying = 0;
	isForwarding = 0;
	isBackWard = 0;
	isSlowMotion = 0;
	Speed = 0;
	url.clear();

	return ret;
}

bool Player::Play()
{
	pthread_attr_t attr;
	bool ret = true;

	if (!isPlaying) {
	AVSync = 1;
	output.AVSync(true);

	isCreationPhase = 1;	// allows the created thread to go into wait mode
	ret = output.Play();

	if (!ret) {
		isCreationPhase = 0;	// allow thread to go into next state
	} else {
		isPlaying = 1;
		isPaused = 0;
		isForwarding = 0;
		if (isBackWard) {
		isBackWard = 0;
		output.Mute(false);
		}
		isSlowMotion = 0;
		Speed = 1;

		if (hasThreadStarted == 0) {
			int error;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

			if ((error = pthread_create(&supervisorThread, &attr, SupervisorThread, this))) {
				fprintf(stderr, "Error creating thread, error:%d:%s\n", error, strerror(error));

				ret = false;
			} else {
				fprintf(stderr, "Created thread\n");
			}
		}

		fprintf(stderr, "clearing isCreationPhase!\n");

		isCreationPhase = 0;	// allow thread to go into next state
	}

	} else {
		fprintf(stderr,"playback already running\n");
		ret = false;
	}

	fprintf(stderr, "exiting with value %d\n", ret);

	return ret;
}

bool Player::Pause()
{
	bool ret = true;

	if (isPlaying && !isPaused) {

	if (isSlowMotion)
		output.Clear();

	output.Pause();

	isPaused = 1;
	//isPlaying  = 1;
	isForwarding = 0;
	if (isBackWard) {
		isBackWard = 0;
		output.Mute(false);
	}
		isSlowMotion = 0;
		Speed = 1;
	} else {
		fprintf(stderr,"playback not playing or already in pause mode\n");
		ret = false;
	}

	fprintf(stderr, "exiting with value %d\n", ret);

	return ret;
}

bool Player::Continue()
{
	int ret = true;

	if (isPlaying && (isPaused || isForwarding || isBackWard || isSlowMotion)) {

		if (isSlowMotion)
			output.Clear();

		output.Continue();

		isPaused = 0;
		//isPlaying  = 1;
		isForwarding = 0;
		if (isBackWard) {
			isBackWard = 0;
			output.Mute(false);
		}
		isSlowMotion = 0;
		Speed = 1;
	} else {
		fprintf(stderr,"continue not possible\n");
		ret = false;
	}

	return ret;
}

bool Player::Stop()
{
	bool ret = true;
	int wait_time = 20;

	if (isPlaying) {

	isPaused = 0;
	isPlaying = 0;
	isForwarding = 0;
	if (isBackWard) {
		isBackWard = 0;
		output.Mute(false);
	}
	isSlowMotion = 0;
	Speed = 0;

	output.Stop();
	input.Stop();

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
bool Player::Terminate()
{
	bool ret = true;
	int wait_time = 20;

	fprintf(stderr, "\n");

	if (isPlaying) {

	if (!abortRequested && !output.Flush()) {
		fprintf(stderr,"failed to flush output.\n");
	}

	ret = input.Stop();
	isPaused = 0;
	isPlaying = 0;
	isForwarding = 0;
	isBackWard = 0;
	isSlowMotion = 0;
	Speed = 0;

	} else {
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

bool Player::FastForward(int speed)
{
	int ret = true;

	/* Audio only forwarding not supported */
	if (isVideo && !isHttp && !isBackWard && (!isPaused ||  isPlaying)) {

		if ((speed <= 0) || (speed > cMaxSpeed_ff)) {
			fprintf(stderr, "speed %d out of range (1 - %d) \n", speed, cMaxSpeed_ff);
			return false;
		}

		isForwarding = 1;
		Speed = speed;
		output.FastForward(speed);
		} else {
		fprintf(stderr,"fast forward not possible\n");
		ret = false;
	}

	return ret;
}

bool Player::FastBackward(int speed)
{
	bool ret = true;

	/* Audio only reverse play not supported */
	if (isVideo && !isForwarding && (!isPaused || isPlaying)) {

	if ((speed > 0) || (speed < cMaxSpeed_fr)) {
		fprintf(stderr, "speed %d out of range (0 - %d) \n", speed, cMaxSpeed_fr);
		return false;
	}

	if (speed == 0) {
		isBackWard = false;
		Speed = false;	/* reverse end */
	} else {
		Speed = speed;
		isBackWard = true;
	}

	output.Clear();
#if 0
	if (output->Command(player, OUTPUT_REVERSE, NULL) < 0) {
		fprintf(stderr,"OUTPUT_REVERSE failed\n");
		isBackWard = 0;
		Speed = 1;
		ret = false;
	}
#endif
	} else {
		fprintf(stderr,"fast backward not possible\n");
		ret = false;
	}

	if (isBackWard)
		output.Mute(true);

	return ret;
}

bool Player::SlowMotion(int repeats)
{
	if (isVideo && !isHttp && isPlaying) {
		if (isPaused)
			Continue();

		switch (repeats) {
			case 2:
			case 4:
			case 8:
				isSlowMotion = true;
				break;
			default:
				repeats = 0;
		}

		output.SlowMotion(repeats);
		return true;
	}
	fprintf(stderr, "slowmotion not possible\n");
	return false;
}

bool Player::Seek(float pos, bool absolute)
{
	output.Clear();
	return input.Seek(pos, absolute);
}

bool Player::GetPts(int64_t &pts)
{
	if (isPlaying)
		return output.GetPts(pts);
	return false;
}

bool Player::GetFrameCount(int64_t &frameCount)
{
	if (isPlaying)
		return output.GetFrameCount(frameCount);
	return false;
}

bool Player::GetDuration(double &duration)
{
	duration = -1;
	if (isPlaying)
		return input.GetDuration(duration);
	return false;
}

bool Player::SwitchVideo(int pid)
{
	Track *track = manager.getVideoTrack(pid);
	if (track)
		input.SwitchVideo(track);
	return !!track;
}

bool Player::SwitchAudio(int pid)
{
	Track *track = manager.getAudioTrack(pid);
	if (track)
		input.SwitchAudio(track);
	return !!track;
}

bool Player::SwitchSubtitle(int pid)
{
	Track *track = manager.getSubtitleTrack(pid);
	if (track)
		input.SwitchSubtitle(track);
	return !!track;
}

bool Player::SwitchTeletext(int pid)
{
	Track *track = manager.getTeletextTrack(pid);
	if (track)
		input.SwitchTeletext(track);
	return !!track;
}

bool Player::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	return input.GetMetadata(keys, values);
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

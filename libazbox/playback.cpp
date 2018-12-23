/*
 * cPlayback implementation for azbox
 * this is actually just a wrapper around rmfp_player which does
 * all the heavy listing
 *
 * based on the original aztrino implementation, but almost
 * completely rewritten since then
 *
 * some of the methods and constants were found by stracing the
 * AZPlay enigma2 plugin...
 *
 * (C) 2012 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <pty.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_PLAYBACK, this, args)
#define lt_info(args...)  _lt_info(TRIPLE_DEBUG_PLAYBACK, this, args)
#define lt_info_c(args...) _lt_info(TRIPLE_DEBUG_PLAYBACK, NULL, args)

#include <proc_tools.h>

/* the file-based commands work better than the FIFOs... */
#define CMD_FILE "/tmp/rmfp.cmd2"
#define IN_FILE  "/tmp/rmfp.in2"
#define OUT_FILE "/tmp/rmfp.out2"

#include "playback_lib.h"

extern "C"{
#include "e2mruainclude.h"
}

#if 0
/* useful for debugging */
static time_t monotonic_ms(void)
{
	struct timespec t;
	time_t ret;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
	{
		perror("monotonic_ms clock_gettime");
		return -1;
	}
	ret = ((t.tv_sec + 604800)& 0x01FFFFF) * 1000; /* avoid overflow */
	ret += t.tv_nsec / 1000000;
	return ret;
}
#endif

/* the mutex makes sure that commands are not interspersed */
bool cPlayback::rmfp_command(int cmd, int param, bool has_param, char *buf, int buflen)
{
	lt_info("%s: %d %d %d %d\n", __func__, cmd, param, has_param, buflen);
	bool ret = true;
	int fd;
	if (cmd == 222)
	{
		if (pthread_mutex_trylock(&rmfp_cmd_mutex))
			return false;
	}
	else
		pthread_mutex_lock(&rmfp_cmd_mutex);
	unlink(OUT_FILE);
	if (has_param)
	{
		fd = open(IN_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		dprintf(fd, "%i", param);
		close(fd);
	}
	fd = open(CMD_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	dprintf(fd, "%i", cmd);
	close(fd);
	int n = 0, m = 0;
	if (buflen > 0)
	{
		while ((fd = open(OUT_FILE, O_RDONLY)) == -1) {
			if (++m > 500) { /* don't wait more than 5 seconds */
				lt_info("%s: timed out waiting for %s (cmd %d par %d buflen %d\n",
					__func__, OUT_FILE, cmd, param, buflen);
				ret = false;
				goto out;
			}
			usleep(10000);
		}
		unlink(OUT_FILE);
		n = read(fd, buf, buflen);
		close(fd);
		/* some commands (CUSTOM_COMMAND_GET_AUDIO_BY_ID for example) actually
		 * return the answer in two successive writes, as we are not using the
		 * FIFO, we need to make sure that the file is deleted immediately, because
		 * rmfp_player will not overwrite it if it exists */
		while ((fd = open(OUT_FILE, O_RDONLY)) == -1) {
			if (++m > 10)
				break;
			usleep(1000);
		}
		if (fd > -1)
		{
			read(fd, buf + n, buflen - n);
			unlink(OUT_FILE);
			close(fd);
		}
		buf[buflen - 1] = '0';
	}
 out:
	pthread_mutex_unlock(&rmfp_cmd_mutex);
	if (cmd != 222) /* called tooo often :-) */
		lt_info("%s: reply: '%s' ret: %d m:%d\n", __func__, buf?buf:"(none)", ret, m);
	return ret;
}

/*
 * runs the rmfp_player in a terminal
 * just doing popen() or similar does not work because then
 * the output will be buffered after starting up and we will only
 * see "Playback has started..." after the player exits
 */
void cPlayback::run_rmfp()
{
	lt_debug("%s: starting\n", __func__);
	thread_started = true;
	//Watch for the space at the end
	std::string base = "rmfp_player -dram 1 -ve 1 -waitexit ";
	std::string filename(mfilename);
	std::string file = '"' + filename + '"';
	std::string final = base + file;

	if (playMode == PLAYMODE_TS && mduration != 0)
	{
		std::stringstream duration;
		duration << (mduration /** 60000LL*/);
		final = base + "-duration " + duration.str() + " " + file;
	}

	pid_t pid = 0;
	int master;
	pid = forkpty(&master, NULL, NULL, NULL);
	if (! pid) {
		execl("/bin/sh", "sh", "-c", final.c_str(), (char *)0);
		lt_info("%s: execl returned: %m\n", __func__);
		exit(0);
	}

	if (pid > 0) {
		char *output=NULL;
		size_t n = 0;
		ssize_t len;
		FILE *f = fdopen(master, "r");
		while ((len = getline(&output, &n, f)) != -1)
		{
			while (len > 0)
			{
				len--;
				if (!isspace(output[len]))
					break;
				output[len] = '\0';
			}
			lt_info("%s out: '%s'\n", __func__, output);
			if (strstr(output, "Playback has started..."))
			{
				playing = 1;
				lt_info("%s: ===================> playing = true\n", __func__);
			}
			else if (strstr(output, "End of file..."))
			{
				playing = 1; /* this can happen without "Playback has started..." */
				eof_reached = true;
				lt_info("%s: ===================> eof_reached = true\n", __func__);
			}
		}
		fclose(f);
		int s;
		while (waitpid(pid, &s, WNOHANG) > 0) {};
		if (output)
			free(output);
	}

	lt_info("%s: terminating\n", __func__);
	if (playing == 0)	/* playback did not start */
		playing = 2;
	else
		playing = 0;
	eof_reached = true;
	pthread_exit(NULL);
}

/* helper function to call the cpp thread loop */
void *execute_rua_thread(void *c)
{
	cPlayback *obj = (cPlayback *)c;
	lt_info_c("%s\n", __func__);
	obj->run_rmfp();
	/* free(obj); // this is most likely wrong */

	return NULL;
}

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	static const char *aPLAYMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};
	playMode = PlayMode;
	if (playMode > 1)
	{
		lt_info("%s: PlayMode %d out of range!\n", __func__, PlayMode);
		playMode = PLAYMODE_FILE;
	}

	lt_info("%s: mode %d (%s)\n", __func__, PlayMode, aPLAYMODE[PlayMode]);
#if 0
	while (access("/tmp/continue", R_OK))
		sleep(1);
#endif

	char c[2] = "0";
	int i = 0;

	for(;;)
	{
		proc_get("/proc/player", c, 2);
		if (c[0] != '0')
			break;
		i++;
		if (i > 10)
		{
			lt_info("%s: ERROR - player is not idle after 10 seconds!\n", __func__);
			open_success = false;
			return false;
		}
		sleep(1);
	}

	proc_put("/proc/player", "2", 2);
	lt_info("%s: /proc/player switched to '2'\n", __func__);

	unlink(CMD_FILE);
	unlink(IN_FILE);
	unlink(OUT_FILE);

	open_success = true;
	return 0;
}

//Used by Fileplay
bool cPlayback::Start(char *filename, unsigned short vpid, int vtype, unsigned short _apid,
		      int ac3, unsigned int duration)
{
	bool ret = true;

	lt_info("%s: filename=%s\n", __func__, filename);
	lt_info("%s: vpid=%u vtype=%d apid=%u ac3=%d duration=%i open_success=%d\n",
		__func__, vpid, vtype, _apid, ac3, duration, open_success);

	if (!open_success)
		return false;

	eof_reached = false;
	//create playback path
	apid = 0;
	subpid = 0;
	mfilename = filename;
	mduration = duration;
	if (pthread_create(&thread, 0, execute_rua_thread, this) != 0)
	{
		lt_info("%s: error creating rmfp_player thread! (out of memory?)\n", __func__);
		ret = false;
	}
	while (! playing)
		sleep(1);
	if (playing == 2)
		playing = 0;
	return ret;
}

void cPlayback::Close(void)
{
	lt_info("%s: playing %d thread_started %d\n", __func__, playing, thread_started);

	if (thread_started)
	{
		rmfp_command(KEY_COMMAND_QUIT_ALL, 0, false, NULL, 0);

		if (pthread_join(thread, NULL))
			lt_info("%s: error joining rmfp thread (%m)\n", __func__);
		playing = 0;
		thread_started = false;
	}
	else
		lt_info("%s: Warning: thread_started == false!\n", __func__);

	if (open_success)
	{
		proc_put("/proc/player", "1", 2);
		open_success = false;
		lt_info("%s: /proc/player switched to '1'\n", __func__);
		usleep(1000000);
	}
}

bool cPlayback::SetAPid(unsigned short pid, int /*ac3*/)
{
	lt_info("%s: pid %i\n", __func__, pid);
	if (pid != apid) {
		rmfp_command(KEY_COMMAND_SWITCH_AUDIO, pid, true, NULL, 0);
		apid = pid;
	}
	return true;
}

bool cPlayback::SelectSubtitles(int pid)
{
	lt_info("%s: pid %i\n", __func__, pid);
	if (pid != subpid)
	{
		rmfp_command(KEY_COMMAND_SWITCH_SUBS, pid, true, NULL, 0);
		subpid = pid;
	}
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	lt_info("%s: playing %d speed %d\n", __func__, playing, speed);

	if (!playing)
		return false;

	playback_speed = speed;

	if (speed > 1 || speed < 0)
		rmfp_command(CUSTOM_COMMAND_TRICK_SEEK, speed, true, NULL, 0);
	else if (speed == 0)
		rmfp_command(KEY_COMMAND_PAUSE, 0, false, NULL, 0);
	else
		rmfp_command(KEY_COMMAND_PLAY, 0, false, NULL, 0);

	return true;
}

bool cPlayback::GetSpeed(int &/*speed*/) const
{
#if 0
	lt_info("%s:\n", __func__);
	speed = playback_speed;
#endif
	return true;
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	lt_debug("%s: playing %d\n", __func__, playing);

	if (eof_reached)
	{
		position = mduration;
		duration = mduration;
		return true;
	}

	if (!playing)
		return false;

	char buf[32];
	/* custom command 222 returns "12345\n1234\n",
	 * first line is duration, second line is position */
	if (! rmfp_command(222, 0, false, buf, 32))
		return false;
	duration = atoi(buf);
	char *p = strchr(buf, '\n');
	if (!p)
		return false;
	position = atoi(++p);
	/* some mpegs return length 0... which would lead to "eof" after 10 seconds */
	if (duration == 0)
		duration = position + 1000;

	if (playMode == PLAYMODE_TS)
	{
		if (position > mduration)
			mduration = position + 1000;
		duration = mduration;
		return true;
	}
	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	lt_info("%s: pos %d abs %d playing %d\n", __func__, position, absolute, playing);

	if (!playing)
		return false;

	int seconds = position / 1000;;

	if (absolute)
	{
		rmfp_command(KEY_COMMAND_SEEK_TO_TIME, seconds, true, NULL, 0);
		return true;
	}

	if (position > 0)
		rmfp_command(CUSTOM_COMMAND_SEEK_RELATIVE_FWD, seconds, true, NULL, 0);
	else if (position < 0)
		rmfp_command(CUSTOM_COMMAND_SEEK_RELATIVE_BWD, seconds, true, NULL, 0);
	return true;
}

void cPlayback::FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	lt_info("%s\n", __func__);
	char buf[32];
	rmfp_command(CUSTOM_COMMAND_AUDIO_COUNT, 0, false, buf, 3);
	unsigned int audio_count = atoi(buf);

	*numpida = audio_count;
	if (audio_count > 0)
	{
		for (unsigned int aid = 0; aid < audio_count; aid++)
		{
			char streamidstring[11];
			char audio_lang[21];
			memset(buf, 0, sizeof(buf));
			rmfp_command(CUSTOM_COMMAND_GET_AUDIO_BY_ID, aid, true, buf, 32);
			memcpy(streamidstring, buf, 10);
			streamidstring[10] = '\0';
			memcpy(audio_lang, buf + 10, 20);
			audio_lang[20] = '\0';
			apids[aid] = atoi(streamidstring);
			ac3flags[aid] = 0;
			language[aid] = audio_lang;
			lt_info("%s: #%d apid:%d lang: %s\n", __func__, aid, apids[aid], audio_lang);
		}
	}
}

void cPlayback::FindAllSubs(uint16_t *spids, unsigned short *supported, uint16_t *numpids, std::string *language)
{
	lt_info("%s\n", __func__);
	char buf[32];

	rmfp_command(CUSTOM_COMMAND_SUBS_COUNT, 0, false, buf, 3);
	unsigned int spu_count = atoi(buf);
	*numpids = spu_count;

	if (spu_count > 0)
	{
		for (unsigned int sid = 0; sid < spu_count; sid++)
		{
			char streamidstring[11];
			char spu_lang[21];
			memset(buf, 0, sizeof(buf));
			rmfp_command(CUSTOM_COMMAND_GET_SUB_BY_ID, sid, true, buf, 32);
			memcpy(streamidstring, buf, 10);
			streamidstring[10] = '\0';
			memcpy(spu_lang, buf + 10, 20);
			spu_lang[20] = '\0';
			spids[sid] = atoi(streamidstring);
			language[sid] = spu_lang;
			supported[sid] = 1;
			lt_info("%s: #%d apid:%d lang: %s\n", __func__, sid, spids[sid], spu_lang);
		}
	}
	//Add streamid -1 to be able to disable subtitles
	*numpids = spu_count + 1;
	spids[spu_count] = -1;
	language[spu_count] = "Disable";
}

/* DVD support is not yet ready... */
void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
}

cPlayback::cPlayback(int /*num*/)
{
	lt_info("%s: constructor\n", __func__);
	playing = 0;
	thread_started = false;
	eof_reached = false;
	open_success = false;
	pthread_mutex_init(&rmfp_cmd_mutex, NULL);
}

cPlayback::~cPlayback()
{
	lt_info("%s\n", __func__);
	pthread_mutex_destroy(&rmfp_cmd_mutex);
}

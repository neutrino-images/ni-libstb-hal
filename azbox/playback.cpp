#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>

#define FIFO_CMD "/tmp/rmfp.cmd"
#define FIFO_IN "/tmp/rmfp.in"
#define FIFO_OUT "/tmp/rmfp.out"
#define FIFO_EVENT "/tmp/rmfp.event"

#include "playback.h"

extern "C"{
#include "e2mruainclude.h"
}

static const char * FILENAME = "playback_cs.cpp";


void cPlayback::RuaThread()
{
printf("Starting RUA thread\n");



//Watch for the space at the end
std::string base = "/usr/bin/rmfp_player -dram 1 -ve 1 -waitexit ";
std::string filename(mfilename);
std::string file = '"' + filename + '"';
std::string final = base + file;

if ( setduration == 1 && mduration != 0)
{
	mduration *= 60000;
	std::stringstream duration;
	duration << mduration;
	final = base + "-duration " + duration.str() + " " + file;
}

system(final.c_str());

printf("Terminating RUA thread\n");
thread_active = 0;
playing = false;
eof_reached = 1;
pthread_exit(NULL);
}

/* helper function to call the cpp thread loop */
void* execute_rua_thread(void *c)
{
	cPlayback *obj=(cPlayback*)c;

	printf("Executing RUA Thread\n");

	obj->RuaThread();

	free(obj);

	return NULL;
}

void cPlayback::EventThread()
{

	printf("Starting Event thread\n");
	
	thread_active = 1;
	eof_reached = 0;
	while (thread_active == 1)
	{
		struct timeval tv;
		fd_set readfds;
		int retval;
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		FD_ZERO(&readfds);
		FD_SET(fd_event, &readfds);
		retval = select(fd_event + 1, &readfds, NULL, NULL, &tv);
		
		//printf("retval is %i\n", retval);
		if (retval)
		{
		
			char eventstring[4];
			int event;
		
			read(fd_event, &eventstring, 3);
			eventstring[3] = '\0';
			event = atoi(eventstring);
		
			printf("Got event message %i\n", event);
			
			switch(event)
			{
				case EVENT_MSG_FDOPEN:
					fd_cmd = open(FIFO_CMD, O_WRONLY);
					fd_in = open(FIFO_IN, O_WRONLY);
					printf("Message FD Opened %i", fd_in);
					break;
				case EVENT_MSG_PLAYBACK_STARTED:
					printf("Got playing event \n");
					playing = true;
					break;
				case EVENT_MSG_EOS:
					printf("Got EOF event \n");
					eof_reached = 1;
					break;
			}
		}
		usleep(100000);
	}
	
	printf("Terminating Event thread\n");
	playing = false;
	pthread_exit(NULL);
}

/* helper function to call the cpp thread loop */
void* execute_event_thread(void *c)
{
	cPlayback *obj=(cPlayback*)c;

	printf("Executing RUA Thread\n");

	obj->EventThread();

	return NULL;
}

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	setduration = 0;
	if (PlayMode == 0)
	{
		printf("RECORDING PLAYING BACK\n");
		setduration = 1;
	}

	printf("%s:%s - PlayMode=%s\n", FILENAME, __FUNCTION__, aPLAYMODE[PlayMode]);
	
	//Making Fifo's for message pipes
	mknod(FIFO_CMD, S_IFIFO | 0666, 0);
	mknod(FIFO_IN, S_IFIFO | 0666, 0);
	mknod(FIFO_OUT, S_IFIFO | 0666, 0);
	mknod(FIFO_EVENT, S_IFIFO | 0666, 0);
	
	//Open pipes we read from. The fd_in pipe will be created once test_rmfp has been started
	fd_out = open(FIFO_OUT, O_RDONLY | O_NONBLOCK);
	fd_event = open(FIFO_EVENT, O_RDONLY | O_NONBLOCK);

	return 0;
}

//Used by Fileplay
void cPlayback::Close(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

//Dagobert: movieplayer does not call stop, it calls close ;)
	Stop();
}

//Used by Fileplay
bool cPlayback::Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3, int duration)
{
	bool ret = true;
	
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d duration=%i\n",
		FILENAME, __FUNCTION__, filename, vpid, vtype, apid, ac3, duration);

	//create playback path
	mAudioStream=0;
	mfilename = filename;
	mduration = duration;
	if (pthread_create(&rua_thread, 0, execute_rua_thread, this) != 0)
	{
		printf("[movieplayer]: error creating file_thread! (out of memory?)\n"); 
		ret = false;
	}
	if (pthread_create(&event_thread, 0, execute_event_thread, this) != 0)
	{
		printf("[movieplayer]: error creating file_thread! (out of memory?)\n"); 
		ret = false;
	}

	return ret;
}

//Used by Fileplay
bool cPlayback::Stop(void)
{
	printf("%s:%s playing %d %d\n", FILENAME, __FUNCTION__, playing, thread_active);
	if(playing==false && thread_active == 0) return false;

	msg = KEY_COMMAND_QUIT_ALL;
	dprintf(fd_cmd, "%i", msg);

	if (pthread_join(rua_thread, NULL)) 
	{
	     printf("error joining rua thread\n");
	}
	
	if (pthread_join(event_thread, NULL)) 
	{
	     printf("error joining event thread\n");
	}

	playing = false;
	return true;
}

bool cPlayback::SetAPid(unsigned short pid, bool /*ac3*/)
{
	printf("%s:%s pid %i\n", FILENAME, __FUNCTION__, pid);
	if (pid != mAudioStream) {
		msg = KEY_COMMAND_SWITCH_AUDIO;
		dprintf(fd_cmd, "%i", msg);
		dprintf(fd_in, "%i", pid);
		mAudioStream = pid;
	}
	return true;
}

bool cPlayback::SetSPid(int pid)
{
	printf("%s:%s pid %i\n", FILENAME, __FUNCTION__, pid);
	if(pid!=mSubStream)
	{
		msg = KEY_COMMAND_SWITCH_SUBS;
		dprintf(fd_cmd, "%i", msg);
		dprintf(fd_in, "%i", pid);
		mSubStream=pid;
	}
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __FUNCTION__, playing, speed);

	if(playing==false) 
	   return false;

//	int result = 0;

	nPlaybackSpeed = speed;
	
	if (speed > 1 || speed < 0)
	{
		msg = CUSTOM_COMMAND_TRICK_SEEK;
		dprintf(fd_cmd, "%i", msg);
		dprintf(fd_in, "%i", speed);
	} 
	else if (speed == 0)
	{
		msg = KEY_COMMAND_PAUSE;
		dprintf(fd_cmd, "%i", msg);
	}
	else
	{
		msg = KEY_COMMAND_PLAY;
		dprintf(fd_cmd, "%i", msg);
	}

//	if (result != 0)
//	{
//		printf("returning false\n");
//		return false;
//	}

	return true;
}

bool cPlayback::GetSpeed(int &/*speed*/) const
{
/*	printf("%s:%s\n", FILENAME, __FUNCTION__);
        speed = nPlaybackSpeed;
*/	return true;
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	printf("%s:%s %d %d\n", FILENAME, __FUNCTION__, position, duration);
	
	//Azbox eof
	if (eof_reached == 1)
	{
		position = -5;
		duration = -5;
		return true;
	}
	
	if(playing==false) return false;

	//Position
	char timestring[11];
	
	msg = CUSTOM_COMMAND_GETPOSITION;
	dprintf(fd_cmd, "%i", msg);
	
	int n = 0;
	while ( n <= 0 ) {
		n = read(fd_out, &timestring, 100);
	}
	timestring[10] = '\0';
	position = atoi(timestring);
	
	//Duration
	int length;
	char durationstring[11];
	
	msg = CUSTOM_COMMAND_GETLENGTH;
	dprintf(fd_cmd, "%i", msg);
	
	n = 0;
	while ( n <= 0 ) {
		n = read(fd_out, &durationstring, 10);
	}
	durationstring[10] = '\0';
	length = atoi(durationstring);
	
	if(length <= 0) {
		duration = duration+1000;
	} else {
		duration = length;
	}
	
	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__,position);
	if(playing==false) return false;
	
	int seconds;
	
	if (absolute == true)
	{
		msg = KEY_COMMAND_SEEK_TO_TIME;
		seconds = position / 1000;
		dprintf(fd_cmd, "%i", msg);
		dprintf(fd_in, "%i", seconds);
	}
	else
	{
		if (position > 0 )
		{
			msg = CUSTOM_COMMAND_SEEK_RELATIVE_FWD;
			seconds = position / 1000;
			dprintf(fd_cmd, "%i", msg);
			dprintf(fd_in, "%i", seconds);
		}
		else if ( position < 0 )
		{
			msg = CUSTOM_COMMAND_SEEK_RELATIVE_BWD;
			seconds = position / 1000;
			seconds *= -1;
			printf("sending seconds %i\n", seconds);
			dprintf(fd_cmd, "%i", msg);
			dprintf(fd_in, "%i", seconds);
		}
	}
	return true;
}

void cPlayback::FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	unsigned int audio_count = 0;
	char audio_countstring[3];
	
	msg = CUSTOM_COMMAND_AUDIO_COUNT;
	dprintf(fd_cmd, "%i", msg);
	
	int n = 0;
	while ( n <= 0 ) {
	n = read(fd_out, &audio_countstring, 2);
	}
	audio_countstring[2] = '\0';
	audio_count = atoi(audio_countstring);
	
	*numpida = audio_count;
	if (audio_count > 0 )
	{
		for ( unsigned int audio_id = 0; audio_id < audio_count; audio_id++ )
		{
			char streamidstring[11];
			char audio_lang[21];
			
			msg = CUSTOM_COMMAND_GET_AUDIO_BY_ID;
			dprintf(fd_cmd, "%i", msg);
			dprintf(fd_in, "%i", audio_id);
			
			n = 0;
			while ( n <= 0 ) {
				n = read(fd_out, &streamidstring, 10);
			}
			read(fd_out, &audio_lang, 20);
			streamidstring[10] = '\0';
			audio_lang[20] = '\0';
			
			apids[audio_id] = atoi(streamidstring);
			ac3flags[audio_id] = 0;
			language[audio_id] = audio_lang;
		}
	}
}

void cPlayback::FindAllSPids(int *spids, uint16_t *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	unsigned int spu_count = 0;
	char spu_countstring[3];

	msg = CUSTOM_COMMAND_SUBS_COUNT;
	dprintf(fd_cmd, "%i", msg);

	int n = 0;
	while ( n <= 0 ) {
		n = read(fd_out, &spu_countstring, 2);
	}
	spu_countstring[2] = '\0';
	spu_count = atoi(spu_countstring);

	*numpids = spu_count;

	if (spu_count > 0 )
	{
		
		for ( unsigned int spu_id = 0; spu_id < spu_count; spu_id++ )
		{
			//int streamid;
			char streamidstring[11];
			char spu_lang[21];
			
			msg = CUSTOM_COMMAND_GET_SUB_BY_ID;
			dprintf(fd_cmd, "%i", msg);
			dprintf(fd_in, "%i", spu_id);

			n = 0;
			while ( n <= 0 ) {
				n = read(fd_out, &streamidstring, 10);
			}
			read(fd_out, &spu_lang, 20);
			streamidstring[10] = '\0';
			spu_lang[20] = '\0';

			spids[spu_id] = atoi(streamidstring);
			language[spu_id] = spu_lang;
		}
	}
		//Add streamid -1 to be able to disable subtitles
		*numpids = spu_count + 1;
		spids[spu_count] = -1;
		language[spu_count] = "Disable";
	
}


cPlayback::cPlayback(int /*num*/)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	playing=false;
	thread_active = 0;
	eof_reached=0;
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

bool cPlayback::IsEOF(void) const
{
//	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return eof_reached;
}

int cPlayback::GetCurrPlaybackSpeed(void) const
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return nPlaybackSpeed;
}

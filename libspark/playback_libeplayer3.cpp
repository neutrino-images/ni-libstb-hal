#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <audio_lib.h>
#include <video_lib.h>

#include <common.h>
extern OutputHandler_t		OutputHandler;
extern PlaybackHandler_t	PlaybackHandler;
extern ContainerHandler_t	ContainerHandler;
extern ManagerHandler_t		ManagerHandler;

#include "playback_libeplayer3.h"

static Context_t *player = NULL;

extern cAudio *audioDecoder;
extern cVideo *videoDecoder;
static bool decoders_closed = false;

static const char * FILENAME = "playback_libeplayer3.cpp";
static playmode_t pm;
static std::string fn_ts;
static std::string fn_xml;
static off_t last_size;
static int init_jump;

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	if (PlayMode != PLAYMODE_TS)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
	}

	printf("%s:%s - PlayMode=%s\n", FILENAME, __FUNCTION__, aPLAYMODE[PlayMode]);
	pm = PlayMode;
	fn_ts = "";
	fn_xml = "";
	last_size = 0;
	nPlaybackSpeed = 0;
	init_jump = -1;
	
	player = (Context_t*) malloc(sizeof(Context_t));

	if(player) {
		player->playback	= &PlaybackHandler;
		player->output		= &OutputHandler;
		player->container	= &ContainerHandler;
		player->manager		= &ManagerHandler;

		fprintf(stderr, "player output name: %s\n", player->output->Name);
	}

	//Registration of output devices
	if(player && player->output) {
		player->output->Command(player,OUTPUT_ADD, (void*)"audio");
		player->output->Command(player,OUTPUT_ADD, (void*)"video");
	}

	return 0;
}

//Used by Fileplay
void cPlayback::Close(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

//Dagobert: movieplayer does not call stop, it calls close ;)
	Stop();
	if (decoders_closed)
	{
		audioDecoder->openDevice();
		videoDecoder->openDevice();
		decoders_closed = false;
	}
}

//Used by Fileplay
bool cPlayback::Start(char *filename, int vpid, int vtype, int apid, int ac3, unsigned int, bool no_probe)
{
	bool ret = false;
	bool isHTTP = false;
	
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d\n",
		FILENAME, __FUNCTION__, filename, vpid, vtype, apid, ac3);

	init_jump = -1;
	//create playback path
	mAudioStream=0;
	mSubtitleStream=-1;
	mTeletextStream=-1;
	char file[strlen(filename) + 1];
	*file = 0;

	if (!strncmp("file://", filename, 7))
		filename += 7;

	if(strstr(filename, "://"))
            isHTTP = true;
	else if (pm == PLAYMODE_TS && no_probe)
	    strcat(file, "myts://");
	else
	    strcat(file, "file://");

	strcat(file, filename);

	//try to open file
	if(player && player->playback && player->playback->Command(player, PLAYBACK_OPEN, file) >= 0) {
		//AUDIO
		if(player && player->manager && player->manager->audio) {
			char ** TrackList = NULL;
			player->manager->audio->Command(player, MANAGER_LIST, &TrackList);
			if (TrackList != NULL) {
				printf("AudioTrack List\n");
				int i = 0;
				for (i = 0; TrackList[i] != NULL; i+=2) {
					printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
					free(TrackList[i]);
					free(TrackList[i+1]);
				}
				free(TrackList);
			}
		}

		//SUB
		if(player && player->manager && player->manager->subtitle) {
			char ** TrackList = NULL;
			player->manager->subtitle->Command(player, MANAGER_LIST, &TrackList);
			if (TrackList != NULL) {
				printf("SubtitleTrack List\n");
				int i = 0;
				for (i = 0; TrackList[i] != NULL; i+=2) {
					printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
					free(TrackList[i]);
					free(TrackList[i+1]);
				}
				free(TrackList);
			}
		}

		//Teletext
		if(player && player->manager && player->manager->teletext) {
			char ** TrackList = NULL;
			player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
			if (TrackList != NULL) {
				printf("TeletextTrack List\n");
				int i = 0;
				for (i = 0; TrackList[i] != NULL; i+=2) {
					printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
					free(TrackList[i]);
					free(TrackList[i+1]);
				}
				free(TrackList);
			}
		}
		//Chapters
		if(player && player->manager && player->manager->chapter) {
			char ** TrackList = NULL;
			player->manager->chapter->Command(player, MANAGER_LIST, &TrackList);
			if (TrackList != NULL) {
				printf("Chapter List\n");
				int i = 0;
				for (i = 0; TrackList[i] != NULL; i+=2) {
					printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
					free(TrackList[i]);
					free(TrackList[i+1]);
				}
				free(TrackList);
			}
		}

		if (pm != PLAYMODE_TS && player && player->output && player->playback) {
			player->output->Command(player, OUTPUT_OPEN, NULL);
			
			SetAPid(apid, 0);
			if ( player->playback->Command(player, PLAYBACK_PLAY, NULL) == 0 ) // playback.c uses "int = 0" for "true"
				ret = true;
		}
	}

/* konfetti: in case of upnp playing mp4 often leads to a 
 * paused playback but data is already injected which leads
 * to errors ... 
 * and I don't see any sense of pausing direct after starting
 * with the exception of timeshift. but this should be handled
 * outside this lib or with another function!
 */
      if (pm != PLAYMODE_TS)
      {
        if ((ret) && (!isHTTP))
	{
	   //pause playback in case of timeshift
	   //FIXME: no picture on tv
	   if (!player || !player->playback || player->playback->Command(player, PLAYBACK_PAUSE, NULL) < 0)
	   {
	      ret = false;
	      printf("failed to pause playback\n");
	   } else
  	      playing = true;
        } else
  	      playing = true;
      }
		
	printf("%s:%s - return=%d\n", FILENAME, __FUNCTION__, ret);

	fn_ts = std::string(filename);
	if (fn_ts.rfind(".ts") == fn_ts.length() - 3)
		fn_xml = fn_ts.substr(0, fn_ts.length() - 3) + ".xml";

	if (pm == PLAYMODE_TS)
	{
		struct stat s;
		if (!stat(filename, &s))
			last_size = s.st_size;
		if (player && player->output && player->playback)
		{
			ret = true;
			videoDecoder->Stop(false);
			audioDecoder->Stop();
		}
	}
	return ret;
}

//Used by Fileplay
bool cPlayback::Stop(void)
{
	printf("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);
	//if(playing==false) return false;

	if(player && player->playback)
		player->playback->Command(player, PLAYBACK_STOP, NULL);

	if(player && player->output)
		player->output->Command(player, OUTPUT_CLOSE, NULL);

	if(player && player->output) {
		player->output->Command(player,OUTPUT_DEL, (void*)"audio");
		player->output->Command(player,OUTPUT_DEL, (void*)"video");
	}

	if(player && player->playback)
		player->playback->Command(player,PLAYBACK_CLOSE, NULL);
	if(player)
		free(player);
	player = NULL;

	playing=false;
	return true;
}

bool cPlayback::SetAPid(int pid, bool ac3 __attribute__((unused)))
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int i=pid;
	if(pid!=mAudioStream){
		if(player && player->playback)
				player->playback->Command(player, PLAYBACK_SWITCH_AUDIO, (void*)&i);
		mAudioStream=pid;
	}
	return true;
}

bool cPlayback::SetSubtitlePid(int pid)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int i=pid;
	if(pid!=mSubtitleStream){
		if(player && player->playback)
				player->playback->Command(player, PLAYBACK_SWITCH_SUBTITLE, (void*)&i);
		mSubtitleStream = pid;
	}
	return true;
}

bool cPlayback::SetTeletextPid(int pid)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int i=pid;
	if(pid!=mTeletextStream){
		if(player && player->playback)
				player->playback->Command(player, PLAYBACK_SWITCH_TELETEXT, (void*)&i);
		mTeletextStream=pid;
	}
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __FUNCTION__, playing, speed);

	if (! decoders_closed)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
		usleep(500000);
		if (player && player->output && player->playback) {
			player->output->Command(player, OUTPUT_OPEN, NULL);
			if (player->playback->Command(player, PLAYBACK_PLAY, NULL) == 0) // playback.c uses "int = 0" for "true"
				playing = true;
		}
	}

	if(playing==false) 
	   return false;
	
	if(player && player->playback) 
	{
		int result = 0;
		
                nPlaybackSpeed = speed;
		
		if (speed > 1)
		{
                    /* direction switch ? */
		    if (player->playback->BackWard)
		    {
		        int r = 0;
                        result = player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void*)&r);
		    
		        printf("result = %d\n", result);
		    }
                    result = player->playback->Command(player, PLAYBACK_FASTFORWARD, (void*)&speed);
		} else
		if (speed < 0)
		{
                    /* direction switch ? */
		    if (player->playback->isForwarding)
		    {
                        result = player->playback->Command(player, PLAYBACK_CONTINUE, NULL);
		    
		        printf("result = %d\n", result);
		    }
		    
		    result = player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void*)&speed);
		} 
		else
		if (speed == 0)
		{
		     /* konfetti: hmmm accessing the member isn't very proper */
		     if ((player->playback->isForwarding) || (!player->playback->BackWard))
                        player->playback->Command(player, PLAYBACK_PAUSE, NULL);
                     else
                     {
		            int _speed = 0; /* means end of reverse playback */
			    player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void*)&_speed);
		     }
		} else
		{
			result = player->playback->Command(player, PLAYBACK_CONTINUE, NULL);
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
	//printf("%s:%s\n", FILENAME, __FUNCTION__);
        speed = nPlaybackSpeed;
	return true;
}

void cPlayback::GetPts(uint64_t &pts)
{
	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_PTS, (void*)&pts);
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	bool got_duration = false;

	/* hack: if the file is growing (timeshift), then determine its length
	 * by comparing the mtime with the mtime of the xml file */
	if (pm == PLAYMODE_TS)
	{
		struct stat s;
		if (!stat(fn_ts.c_str(), &s))
		{
			if (! playing || last_size != s.st_size)
			{
				last_size = s.st_size;
				time_t curr_time = s.st_mtime;
				if (!stat(fn_xml.c_str(), &s))
				{
					duration = (curr_time - s.st_mtime) * 1000;
					if (! playing)
						return true;
					got_duration = true;
				}
			}
		}
	}

	if(playing==false) return false;

	if (player && player->playback && !player->playback->isPlaying) {
		printf("cPlayback::%s !!!!EOF!!!! < -1\n", __func__);
		position = duration + 1000;
		return false;
	}

	unsigned long long int vpts = 0;
	if(player && player->playback)
		player->playback->Command(player, PLAYBACK_PTS, &vpts);

	if(vpts <= 0) {
		//printf("ERROR: vpts==0");
	} else {
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts/90;
	}

	if (got_duration)
		return true;

	double length = 0;

	if(player && player->playback)
		player->playback->Command(player, PLAYBACK_LENGTH, &length);

	if(length <= 0) {
		duration = duration+1000;
	} else {
		duration = length*1000.0;
	}

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__,position);
	if (playing == false)
	{
		/* the calling sequence is:
		 * Start()       - paused
		 * SetPosition() - which fails if not running
		 * SetSpeed()    - to start playing
		 * so let's remember the initial jump position and later jump to it
		 */
		init_jump = position;
		return false;
	}
	float pos = (position/1000.0);
	if(player && player->playback)
		player->playback->Command(player, absolute ? PLAYBACK_SEEK_ABS : PLAYBACK_SEEK, (void*)&pos);
	return true;
}

void cPlayback::FindAllPids(int *apids, unsigned int *ac3flags, unsigned int *numpida, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int max_numpida = *numpida;
	*numpida = 0;
	if(player && player->manager && player->manager->audio) {
		char ** TrackList = NULL;
		player->manager->audio->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("AudioTrack List\n");
			int i = 0,j=0;
			for (i = 0,j=0; TrackList[i] != NULL; i+=2,j++) {
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				if (j < max_numpida) {
					int _pid;
					char _lang[strlen(TrackList[i])];
					if (2 == sscanf(TrackList[i], "%d %s\n", &_pid, _lang)) {
						apids[j]=_pid;
						// atUnknown, atMPEG, atMP3, atAC3, atDTS, atAAC, atPCM, atOGG, atFLAC
						if(     !strncmp("A_MPEG/L3",   TrackList[i+1], 9))
							ac3flags[j] = 3;
						if(     !strncmp("A_MP3",       TrackList[i+1], 5))
							ac3flags[j] = 4;
						else if(!strncmp("A_AC3",       TrackList[i+1], 5))
							ac3flags[j] = 1;
						else if(!strncmp("A_EAC3",      TrackList[i+1], 6))
							ac3flags[j] = 7;
						else if(!strncmp("A_DTS",       TrackList[i+1], 5))
							ac3flags[j] = 6;
						else if(!strncmp("A_AAC",       TrackList[i+1], 5))
							ac3flags[j] = 5;
						else if(!strncmp("A_PCM",       TrackList[i+1], 5))
							ac3flags[j] = 0; 	//todo
						else if(!strncmp("A_VORBIS",    TrackList[i+1], 8))
							ac3flags[j] = 0;	//todo
						else if(!strncmp("A_FLAC",      TrackList[i+1], 6))
							ac3flags[j] = 0;	//todo
						else
							ac3flags[j] = 0;	//todo
						language[j]=std::string(_lang);
					}
				}
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
			*numpida=j;
		}
	}
}

void cPlayback::FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int max_numpids = *numpids;
	*numpids = 0;
	if(player && player->manager && player->manager->subtitle) {
		char ** TrackList = NULL;
		player->manager->subtitle->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("SubtitleTrack List\n");
			int i = 0,j=0;
			for (i = 0,j=0; TrackList[i] != NULL; i+=2,j++) {
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				if (j < max_numpids) {
					int _pid;
					char _lang[strlen(TrackList[i])];
					if (2 == sscanf(TrackList[i], "%d %s\n", &_pid, _lang)) {
						pids[j]=_pid;
						language[j]=std::string(_lang);
					}
				}
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
			*numpids=j;
		}
	}
}

void cPlayback::FindAllTeletextsubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int max_numpids = *numpids;
	*numpids = 0;
	if(player && player->manager && player->manager->teletext) {
		char ** TrackList = NULL;
		player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("Teletext List\n");
			int i = 0,j=0;
			for (i = 0,j=0; TrackList[i] != NULL; i+=2) {
				int type = 0;
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				if (j < max_numpids) {
					int _pid;
					if (2 != sscanf(TrackList[i], "%*d %d %*s %d %*d %*d", &_pid, &type))
						continue;
					if (type != 2 && type != 5) // return subtitles only
						continue;
					pids[j]=_pid;
					language[j]=std::string(TrackList[i]);
					j++;
				}
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
			*numpids=j;
		}
	}
}

int cPlayback::GetTeletextPid(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	int pid = -1;
	if(player && player->manager && player->manager->teletext) {
		char ** TrackList = NULL;
		player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("Teletext List\n");
			int i = 0;
			for (i = 0; TrackList[i] != NULL; i+=2) {
				int type = 0;
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				if (pid < 0) {
					if (2 != sscanf(TrackList[i], "%*d %d %*s %d %*d %*d", &pid, &type))
						continue;
					if (type != 1)
						pid = -1;
				}
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
		}
	}
	printf("teletext pid id %d (0x%x)\n", pid, pid);
	return pid;
}

#if 0
/* dummy functions for subtitles */
void cPlayback::FindAllSubs(uint16_t * /*pids*/, unsigned short * /*supp*/, uint16_t *num, std::string * /*lang*/)
{
	*num = 0;
}

bool cPlayback::SelectSubtitles(int pid)
{
	printf("%s:%s pid %d\n", FILENAME, __func__, pid);
	return false;
}
#endif

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();

	if(player && player->manager && player->manager->chapter) {
		char ** TrackList = NULL;
		player->manager->chapter->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("%s: Chapter List\n", __func__);
			int i = 0;
			for (i = 0; TrackList[i] != NULL; i+=2) {
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				int pos = atoi(TrackList[i]);
				std::string title(TrackList[i + 1]);
				positions.push_back(pos);
				titles.push_back(title);
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
		}
	}
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();
	char **metadata = NULL;
	if (player && player->playback) {
		player->playback->Command(player, PLAYBACK_METADATA, &metadata);
		if (metadata) {
			for (char **m = metadata; *m;) {
				keys.push_back(*m);
				free(*m++);
				values.push_back(*m);
				free(*m++);
			}
			free(metadata);
		}
	}
}

//
cPlayback::cPlayback(int num __attribute__((unused)))
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	playing=false;
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

void cPlayback::RequestAbort() {
	if (player && player->playback) {
		player->playback->abortRequested = 1;
		while (player->playback->isPlaying)
			usleep(100000);
	}
}

bool cPlayback::IsPlaying() {
	if (player && player->playback)
		return player->playback->isPlaying;
	return false;
}

unsigned long long cPlayback::GetReadCount() {
	if (player && player->playback) {
		return player->playback->readCount;
	}
	return 0;
}

#if 0
bool cPlayback::IsPlaying(void) const
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	/* konfetti: there is no event/callback mechanism in libeplayer2
	 * so in case of ending playback we have no information on a 
	 * terminated stream currently (or did I oversee it?).
	 * So let's ask the player the state.
	 */
	if (playing)
	{
	   return player->playback->isPlaying;
	}

	return playing;
}
#endif

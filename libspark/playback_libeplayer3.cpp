#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playback_cs.h"

static const char * FILENAME = "playback_cs.cpp";

//
void cPlayback::Attach(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

void cPlayback::Detach(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

bool cPlayback::SetAVDemuxChannel(bool On, bool Video, bool Audio)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	return 0;
}

void cPlayback::PlaybackNotify (int  Event, void *pData, void *pTag)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

void cPlayback::DMNotify(int Event, void *pTsBuf, void *Tag)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	printf("%s:%s - PlayMode=%s\n", FILENAME, __FUNCTION__, aPLAYMODE[PlayMode]);
	
	player = (Context_t*) malloc(sizeof(Context_t));

	if(player) {
		player->playback	= &PlaybackHandler;
		player->output		= &OutputHandler;
		player->container	= &ContainerHandler;
		player->manager		= &ManagerHandler;

		printf("%s\n", player->output->Name);
	}

	//Registration of output devices
	if(player && player->output) {
		player->output->Command(player,OUTPUT_ADD, (void*)"audio");
		player->output->Command(player,OUTPUT_ADD, (void*)"video");
		player->output->Command(player,OUTPUT_ADD, (void*)"subtitle");
	}

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
bool cPlayback::Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3)
{
	bool ret = false;
	bool isHTTP = false;
	
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d\n",
		FILENAME, __FUNCTION__, filename, vpid, vtype, apid, ac3);

	//create playback path
	mAudioStream=0;
	char file[400] = {""};

	if(!strncmp("http://", filename, 7))
	{
	    printf("http://\n");
            isHTTP = true;
	}
	else if(!strncmp("file://", filename, 7))
	{
	    printf("file://\n");
	}
	else if(!strncmp("upnp://", filename, 7))
	{
	    printf("upnp://\n");
            isHTTP = true;
	}
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

		if(player && player->output && player->playback) {
			player->output->Command(player, OUTPUT_OPEN, NULL);
			
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
        if ((ret) && (!isHTTP))
	{
	   //pause playback in case of timeshift
	   //FIXME: no picture on tv
	   if (player->playback->Command(player, PLAYBACK_PAUSE, NULL) < 0)
	   {
	      ret = false;
	      printf("failed to pause playback\n");
	   } else
  	      playing = true;
        } else
  	      playing = true;
		
	printf("%s:%s - return=%d\n", FILENAME, __FUNCTION__, ret);

	return ret;
}

//Used by Fileplay
bool cPlayback::Stop(void)
{
	printf("%s:%s playing %d\n", FILENAME, __FUNCTION__, playing);
	if(playing==false) return false;

	if(player && player->playback && player->output) {
		player->playback->Command(player, PLAYBACK_STOP, NULL);
		player->output->Command(player, OUTPUT_CLOSE, NULL);
	}

	if(player && player->output) {
		player->output->Command(player,OUTPUT_DEL, (void*)"audio");
		player->output->Command(player,OUTPUT_DEL, (void*)"video");
		player->output->Command(player,OUTPUT_DEL, (void*)"subtitle");
	}

	if(player && player->playback)
		player->playback->Command(player,PLAYBACK_CLOSE, NULL);
	if(player)
		free(player);
	if(player != NULL)
		player = NULL;

	playing=false;
	return true;
}

bool cPlayback::SetAPid(unsigned short pid, bool ac3)
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

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __FUNCTION__, playing, speed);

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
	printf("%s:%s\n", FILENAME, __FUNCTION__);
        speed = nPlaybackSpeed;
	return true;
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	printf("%s:%s %d %d\n", FILENAME, __FUNCTION__, position, duration);
	if(playing==false) return false;

	if (player && player->playback && !player->playback->isPlaying) {
		printf("cPlayback::%s !!!!EOF!!!! < -1\n", __func__);
		position = duration + 1000;
		// duration = 0;

		// this is stupid
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

bool cPlayback::GetOffset(off64_t &offset)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__,position);
	if(playing==false) return false;
	float pos = (position/1000.0);
	if(player && player->playback)
		player->playback->Command(player, PLAYBACK_SEEK, (void*)&pos);
	return true;
}

void * cPlayback::GetHandle(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return NULL;
}

void * cPlayback::GetDmHandle(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return NULL;
}

void cPlayback::FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if(player && player->manager && player->manager->audio) {
		char ** TrackList = NULL;
		player->manager->audio->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL) {
			printf("AudioTrack List\n");
			int i = 0,j=0;
			for (i = 0,j=0; TrackList[i] != NULL; i+=2,j++) {
				printf("\t%s - %s\n", TrackList[i], TrackList[i+1]);
				apids[j]=j;
				// atUnknown, atMPEG, atMP3, atAC3, atDTS, atAAC, atPCM, atOGG, atFLAC
				if(     !strncmp("A_MPEG/L3",   TrackList[i+1], 9))
					ac3flags[j] = 4;
				else if(!strncmp("A_AC3",       TrackList[i+1], 5))
					ac3flags[j] = 1;
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
				language[j]=TrackList[i];
				free(TrackList[i]);
				free(TrackList[i+1]);
			}
			free(TrackList);
			*numpida=j;
		}
	}
}

//
cPlayback::cPlayback(int num)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	playing=false;
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}

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

bool cPlayback::IsEnabled(void) const
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return enabled;
}

int cPlayback::GetCurrPlaybackSpeed(void) const
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return nPlaybackSpeed;
}

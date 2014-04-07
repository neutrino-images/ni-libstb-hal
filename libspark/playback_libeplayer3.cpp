#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <audio_lib.h>
#include <video_lib.h>

#include "player.h"

#include "playback_libeplayer3.h"

static Player *player = NULL;

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
	
	player = new Player();

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
	
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d, no_probe=%d\n",
		FILENAME, __FUNCTION__, filename, vpid, vtype, apid, ac3, no_probe);

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
	else if (no_probe)
	    strcat(file, "myts://");
	else
	    strcat(file, "file://");

	strcat(file, filename);

	//try to open file
	if(player && player->Open(file)) {
		if (pm != PLAYMODE_TS) {
			player->output.Open();
			
			SetAPid(apid, 0);
			ret = player->Play();
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
	   if (!player || !player->Pause())
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
		if (player)
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

	if(player)
		player->Stop();

	if(player)
		player->output.Close();

	if(player)
		player->Close();
	if(player) {
		delete player;
		player = NULL;
	}

	playing=false;
	return true;
}

bool cPlayback::SetAPid(int pid, bool ac3 __attribute__((unused)))
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if(pid!=mAudioStream){
		if(player)
			player->SwitchAudio(pid);
		mAudioStream=pid;
	}
	return true;
}

bool cPlayback::SetSubtitlePid(int pid)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if(pid!=mSubtitleStream){
		if(player)
			player->SwitchSubtitle(pid);
		mSubtitleStream = pid;
	}
	return true;
}

bool cPlayback::SetTeletextPid(int pid)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if(pid!=mTeletextStream){
		if(player)
			player->SwitchTeletext(pid);
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
		if (player) {
			player->output.Open();
			playing = player->Play();
		}
	}

	if(playing==false) 
	   return false;
	
	if(player)
	{
		int result = 0;
		
                nPlaybackSpeed = speed;
		
		if (speed > 1)
		{
                    /* direction switch ? */
		    if (player->isBackWard)
		    {
                        result = player->FastBackward(0);
		    
		        printf("result = %d\n", result);
		    }
                    result = player->FastForward(speed);
		} else
		if (speed < 0)
		{
                    /* direction switch ? */
		    if (player->isForwarding)
		    {
                        result = player->Continue();
		    
		        printf("result = %d\n", result);
		    }
		    
		    result = player->FastBackward(speed);
		} 
		else
		if (speed == 0)
		{
		     /* konfetti: hmmm accessing the member isn't very proper */
		     if ((player->isForwarding) || (!player->isBackWard))
                        player->Pause();
                     else
                     {
			    player->FastForward(0);
		     }
		} else
		{
			result = player->Continue();
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
	if (player)
		player->GetPts((int64_t &) pts);
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

	if (player && !player->isPlaying) {
		printf("cPlayback::%s !!!!EOF!!!! < -1\n", __func__);
		position = duration + 1000;
		return false;
	}

	int64_t vpts = 0;
	if(player)
		player->GetPts(vpts);

	if(vpts <= 0) {
		//printf("ERROR: vpts==0");
	} else {
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts/90;
	}

	if (got_duration)
		return true;

	double length = 0;

	if(player)
		player->GetDuration(length);

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
	if(player)
		player->Seek(pos, absolute);
	return true;
}

void cPlayback::FindAllPids(int *pids, unsigned int *ac3flags, unsigned int *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	if(player) {
		std::vector<Track> tracks = player->manager.getAudioTracks();
		for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
			pids[i] = it->pid;
			ac3flags[i] = it->ac3flags;
			language[i] = it->Name;
			i++;
		}
	}
	*numpids = i;
}

void cPlayback::FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	if(player) {
		std::vector<Track> tracks = player->manager.getSubtitleTracks();
		for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
			pids[i] = it->pid;
			language[i] = it->Name;
			i++;
		}
	}
	*numpids = i;
}

void cPlayback::FindAllTeletextsubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	if(player) {
		std::vector<Track> tracks = player->manager.getTeletextTracks();
		for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
			pids[i] = it->pid;
			if (it->type != 2 && it->type != 5) // return subtitles only
				continue;
			char tmp[80];
			snprintf(tmp, sizeof(tmp), "%d %d %s %d %d %d", it->pid, it->pid, it->Name.c_str(), it->type, it->mag, it->page); //FIXME
			language[i] = std::string(tmp);
			i++;
		}
	}
	*numpids = i;
}

int cPlayback::GetTeletextPid(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if(player) {
		std::vector<Track> tracks = player->manager.getTeletextTracks();
		for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end(); ++it) {
			if (it->type == 1)
				return it->pid;
		}
	}
	return -1;
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
	player->GetChapters(positions, titles);
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	if (!player)
		return;
	player->input.GetMetadata(keys, values);
}

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
	if (player) {
		player->abortRequested = 1;
		while (player->isPlaying)
			usleep(100000);
	}
}

bool cPlayback::IsPlaying() {
	if (player)
		return player->isPlaying;
	return false;
}

unsigned long long cPlayback::GetReadCount() {
	if (player)
		return player->readCount;
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
	   return player->isPlaying;
	}

	return playing;
}
#endif

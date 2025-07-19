#define __USE_FILE_OFFSET64 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

#include <audio_lib.h>
#include <video_lib.h>

extern "C" {
#include <common.h>
	extern OutputHandler_t OutputHandler;
	extern PlaybackHandler_t PlaybackHandler;
	extern ContainerHandler_t ContainerHandler;
	extern ManagerHandler_t ManagerHandler;
	extern int32_t ffmpeg_av_dict_set(const char *key, const char *value, int32_t flags);
}

#include "playback_libeplayer3.h"
#include "hal_debug.h"

#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYBACK, this, args)
#define hal_info(args...)  _hal_info(HAL_DEBUG_PLAYBACK, this, args)

static Context_t *player = NULL;

extern cAudio *audioDecoder;
extern cVideo *videoDecoder;
OpenThreads::Mutex cPlayback::mutex;

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] =
	{
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	if (PlayMode != PLAYMODE_TS)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
	}

	pm = PlayMode;
	got_vpts_ts = false;
	vpts_ts = 0;
	fn_ts = "";
	fn_xml = "";
	last_size = 0;
	nPlaybackSpeed = 0;
	init_jump = -1;
	avft = avformat_alloc_context();

	if (!player)
	{
		player = (Context_t *) malloc(sizeof(Context_t));
	}

	if (player)
	{
		player->playback = &PlaybackHandler;
		player->output = &OutputHandler;
		player->container = &ContainerHandler;
		player->manager = &ManagerHandler;

		hal_info("%s - player output name: %s PlayMode: %s\n", __func__, player->output->Name, aPLAYMODE[PlayMode]);
	}

	//Registration of output devices
	if (player && player->output)
	{
		player->output->Command(player, OUTPUT_ADD, (void *)"audio");
		player->output->Command(player, OUTPUT_ADD, (void *)"video");
		player->output->Command(player, OUTPUT_ADD, (void *)"subtitle");
	}

	return 0;
}

void cPlayback::Close(void)
{
	hal_info("%s\n", __func__);

	//Dagobert: movieplayer does not call stop, it calls close ;)
	mutex.lock();
	if (playing)
		Stop();
	mutex.unlock();

	if (decoders_closed)
	{
		audioDecoder->openDevice();
		videoDecoder->openDevice();
		decoders_closed = false;
	}
}

std::string cPlayback::extractParam(const std::string &hdrs, const std::string &paramName)
{
	size_t paramPos = hdrs.find(paramName);
	if (paramPos != std::string::npos) {
		size_t valuePos = paramPos + paramName.length();
		size_t valueEndPos = hdrs.find('&', valuePos);
		if (valueEndPos == std::string::npos) {
		    valueEndPos = hdrs.length();
		}
		std::string value = hdrs.substr(valuePos, valueEndPos - valuePos);

		size_t trailingSpacePos = value.find_last_not_of(" \t\r\n");
		if (trailingSpacePos != std::string::npos) {
		    value.erase(trailingSpacePos + 1);
		}
		return value;
	}
	return "";
}

bool cPlayback::Start(std::string filename, std::string headers, std::string filename2)
{
	return Start((char *) filename.c_str(), 0, 0, 0, 0, 0, headers, filename2);
}

bool cPlayback::Start(char *filename, int vpid, int vtype, int apid, int ac3, int, std::string headers, std::string filename2)
{
	bool ret = false;
	bool isHTTP = false;

	hal_info("%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d\n", __func__, filename, vpid, vtype, apid, ac3);

	init_jump = -1;
	//create playback path
	mAudioStream = 0;
	mSubtitleStream = -1;
	mTeletextStream = -1;
	std::string file;

	if (*filename == '/')
		file = "file://";
	file += filename;

	if (file.substr(0, 7) == "file://")
	{
		if (file.substr(file.length() - 3) == ".ts")
		{
			fn_ts = file.substr(7);
			fn_xml = file.substr(7, file.length() - 9);
			fn_xml += "xml";
		}
	}
	else
		isHTTP = true;

	if (isHTTP && headers.empty())
	{
		size_t pos = file.rfind('#');
		if (pos != std::string::npos) {
			std::string val;
			std::string hdrs = file.substr(pos + 1);

			val = extractParam(hdrs, "User-Agent=");
			if (!val.empty()) {
				headers += "User-Agent: " + val + "\n";
			}
			val = extractParam(hdrs, "Referer=");
			if (!val.empty()) {
				headers += "Referer: " + val + "\n";
			}
			if (!headers.empty()) {
				file = file.substr(0, pos);
			}
		}
	}
	if (!headers.empty())
	{
		printf("Headers List\n%s", headers.c_str());
		const char hkey[] = "headers";
		ffmpeg_av_dict_set(hkey, headers.c_str(), 0);
	}

	std::string szSecondFile;
	char *file2 = NULL;
	if (!filename2.empty())
	{
		szSecondFile = filename2;
		file2 = (char *) szSecondFile.c_str();
	}

	PlayFiles_t playbackFiles = { (char *) file.c_str(), file2, NULL, NULL, 0, 0, 0, 0};
	if (player->playback->Command(player, PLAYBACK_OPEN, &playbackFiles) == 0)
	{
		if (pm == PLAYMODE_TS)
		{
			struct stat64 s;
			if (!stat64(file.c_str(), &s))
				last_size = s.st_size;
			ret = true;
			videoDecoder->Stop(false);
			audioDecoder->Stop();
		}
		else
		{
			//AUDIO
			if (player && player->manager && player->manager->audio)
			{
				char **TrackList = NULL;
				player->manager->audio->Command(player, MANAGER_LIST, &TrackList);
				if (TrackList != NULL)
				{
					printf("AudioTrack List\n");
					int i = 0;
					for (i = 0; TrackList[i] != NULL; i += 2)
					{
						printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
						free(TrackList[i]);
						free(TrackList[i + 1]);
					}
					free(TrackList);
				}
			}

			//SUB
			if (player && player->manager && player->manager->subtitle)
			{
				char **TrackList = NULL;
				player->manager->subtitle->Command(player, MANAGER_LIST, &TrackList);
				if (TrackList != NULL)
				{
					printf("SubtitleTrack List\n");
					int i = 0;
					for (i = 0; TrackList[i] != NULL; i += 2)
					{
						printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
						free(TrackList[i]);
						free(TrackList[i + 1]);
					}
					free(TrackList);
				}
			}

			/*
			//Teletext
			if (player && player->manager && player->manager->teletext)
			{
				char ** TrackList = NULL;
				player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
				if (TrackList != NULL)
				{
					printf("TeletextTrack List\n");
					int i = 0;
					for (i = 0; TrackList[i] != NULL; i += 2)
					{
						printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
						free(TrackList[i]);
						free(TrackList[i + 1]);
					}
					free(TrackList);
				}
			}
			*/

			//Chapters
			if (player && player->manager && player->manager->chapter)
			{
				char **TrackList = NULL;
				player->manager->chapter->Command(player, MANAGER_LIST, &TrackList);
				if (TrackList != NULL)
				{
					printf("Chapter List\n");
					int i = 0;
					for (i = 0; TrackList[i] != NULL; i += 2)
					{
						printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
						free(TrackList[i]);
						free(TrackList[i + 1]);
					}
					free(TrackList);
				}
			}

			playing = true;
			first = true;
			player->output->Command(player, OUTPUT_OPEN, NULL);
			ret = (player->playback->Command(player, PLAYBACK_PLAY, NULL) == 0);

			if (ret && !isHTTP)
				playing = ret = (player->playback->Command(player, PLAYBACK_PAUSE, NULL) == 0);
		}
	}
	return ret;
}

bool cPlayback::Stop(void)
{
	hal_info("%s playing %d\n", __func__, playing);

	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_STOP, NULL);

	if (player && player->output)
		player->output->Command(player, OUTPUT_CLOSE, NULL);

	if (player && player->output)
	{
		player->output->Command(player, OUTPUT_DEL, (void *)"audio");
		player->output->Command(player, OUTPUT_DEL, (void *)"video");
		player->output->Command(player, OUTPUT_DEL, (void *)"subtitle");
	}

	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_CLOSE, NULL);

	playing = false;
	return true;
}

bool cPlayback::SetAPid(int pid, bool /* ac3 */)
{
	hal_info("%s\n", __func__);
	int i = pid;

	if (pid != mAudioStream)
	{
		if (player && player->playback)
			player->playback->Command(player, PLAYBACK_SWITCH_AUDIO, (void *)&i);
		mAudioStream = pid;
	}
	return true;
}

bool cPlayback::SetVPid(int /*pid*/)
{
	hal_info("%s\n", __func__);
	return true;
}

bool cPlayback::SetSubtitlePid(int pid)
{
	hal_info("%s\n", __func__);
	int i = pid;

	if (pid != mSubtitleStream)
	{
		if (player && player->playback)
			player->playback->Command(player, PLAYBACK_SWITCH_SUBTITLE, (void *)&i);
		mSubtitleStream = pid;
	}
	return true;
}

bool cPlayback::SetTeletextPid(int pid)
{
	hal_info("%s\n", __func__);

	//int i = pid;

	if (pid != mTeletextStream)
	{
		//if (player && player->playback)
		//	player->playback->Command(player, PLAYBACK_SWITCH_TELETEXT, (void*)&i);
		mTeletextStream = pid;
	}
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	hal_info("%s playing %d speed %d\n", __func__, playing, speed);

	if (!decoders_closed)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
		usleep(500000);
		if (player && player->output && player->playback)
		{
			player->output->Command(player, OUTPUT_OPEN, NULL);
			if (player->playback->Command(player, PLAYBACK_PLAY, NULL) == 0)
				playing = true;
		}
	}

	if (!playing)
		return false;

	if (player && player->playback)
	{
		int result = 0;
		if (nPlaybackSpeed == 0 && speed > 1)
		{
			result = player->playback->Command(player, PLAYBACK_CONTINUE, NULL);
		}

		nPlaybackSpeed = speed;

		if (speed > 1)
		{
			/* direction switch ? */
			if (player->playback->BackWard)
			{
				int r = 0;
				result = player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void *)&r);
				printf("result = %d\n", result);
			}
			result = player->playback->Command(player, PLAYBACK_FASTFORWARD, (void *)&speed);
		}
		else if (speed < 0)
		{
			/* direction switch ? */
			if (player->playback->isForwarding)
			{
				result = player->playback->Command(player, PLAYBACK_CONTINUE, NULL);
				printf("result = %d\n", result);
			}
			result = player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void *)&speed);
		}
		else if (speed == 0)
		{
			/* konfetti: hmmm accessing the member isn't very proper */
			if ((player->playback->isForwarding) || (!player->playback->BackWard))
				player->playback->Command(player, PLAYBACK_PAUSE, NULL);
			else
			{
				int _speed = 0; /* means end of reverse playback */
				player->playback->Command(player, PLAYBACK_FASTBACKWARD, (void *)&_speed);
			}
		}
		else
		{
			result = player->playback->Command(player, PLAYBACK_CONTINUE, NULL);
		}

		if (init_jump > -1)
		{
			SetPosition(init_jump, true);
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
	hal_debug("%s\n", __func__);
	speed = nPlaybackSpeed;
	return true;
}

void cPlayback::GetPts(uint64_t &pts)
{
	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_PTS, (void *)&pts);
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration, bool isWebChannel)
{
	bool got_duration = false;
	hal_debug("%s %d %d\n", __func__, position, duration);

	/* hack: if the file is growing (timeshift), then determine its length
	 * by comparing the mtime with the mtime of the xml file */
	if (pm == PLAYMODE_TS)
	{
		struct stat64 s;
		if (!stat64(fn_ts.c_str(), &s))
		{
			if (!playing || last_size != s.st_size)
			{
				last_size = s.st_size;
				time_t curr_time = s.st_mtime;
				if (!stat64(fn_xml.c_str(), &s))
				{
					duration = (curr_time - s.st_mtime) * 1000;
					if (!playing)
						return true;
					got_duration = true;
				}
			}
		}
	}

	if (!playing)
		return false;

	if (player && player->playback && !player->playback->isPlaying)
	{
		hal_info("%s !!!!EOF!!!! < -1\n", __func__);
		if (isWebChannel)
		{
			position = duration - 1000;
			return true;
		}
		else
		{
			position = duration + 1000;
			return false;
		}
	}

	int64_t vpts = 0;
	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_PTS, &vpts);

	if (vpts <= 0)
	{
		//printf("ERROR: vpts==0");
	}
	else
	{
		/* workaround for crazy vpts value during timeshift */
		if (!got_vpts_ts && pm == PLAYMODE_TS)
		{
			vpts_ts = vpts;
			got_vpts_ts = true;
		}
		if (got_vpts_ts)
			vpts -= vpts_ts;
		/* end workaround */
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts / 90;
	}

	if (got_duration)
		return true;

	int64_t length = 0;

	if (player && player->playback)
		player->playback->Command(player, PLAYBACK_LENGTH, &length);

	if (length <= 0)
	{
		duration = duration + 1000;
	}
	else
	{
		duration = length * 1000;
	}

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	hal_info("%s %d\n", __func__, position);

	if (playing && first)
	{
		/* the calling sequence is:
		 * Start()       - paused
		 * SetPosition() - which fails if not running
		 * SetSpeed()    - to start playing
		 * so let's remember the initial jump position and later jump to it
		 */
		init_jump = position;
		first = false;
		return false;
	}

	int64_t pos = (position / 1000.0);

	if (player && player->playback)
		player->playback->Command(player, absolute ? PLAYBACK_SEEK_ABS : PLAYBACK_SEEK, (void *)&pos);

	return true;
}

void cPlayback::FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	hal_info("%s\n", __func__);
	const int max_numpida = 40;//MAX_PLAYBACK_PIDS defined in neutrino movieplayer.h
	*numpida = 0;

	if (player && player->playback && player->playback->isPlaying && player->manager && player->manager->audio)
	{
		char **TrackList = NULL;
		player->manager->audio->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL)
		{
			printf("AudioTrack List\n");
			int i = 0, j = 0;
			for (i = 0, j = 0; TrackList[i] != NULL; i += 2, j++)
			{
				printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
				if (j < max_numpida)
				{
					int _pid;
					char _lang[strlen(TrackList[i])];
					if (sscanf(TrackList[i], "%d %s\n", &_pid, _lang) == 2)
					{
						apids[j] = _pid;
						// atUnknown, atMPEG, atMP3, atAC3, atDTS, atAAC, atPCM, atOGG, atFLAC
						if (!strncmp("A_MPEG/L3", TrackList[i + 1], 9))
							ac3flags[j] = 3;
						if (!strncmp("A_MP3", TrackList[i + 1], 5))
							ac3flags[j] = 4;
						else if (!strncmp("A_AC3", TrackList[i + 1], 5))
							ac3flags[j] = 1;
						else if (!strncmp("A_EAC3", TrackList[i + 1], 6))
							ac3flags[j] = 7;
						else if (!strncmp("A_DTS", TrackList[i + 1], 5))
							ac3flags[j] = 6;
						else if (!strncmp("A_AAC", TrackList[i + 1], 5))
							ac3flags[j] = 5;
						else if (!strncmp("A_PCM", TrackList[i + 1], 5))
							ac3flags[j] = 0; // todo
						else if (!strncmp("A_VORBIS", TrackList[i + 1], 8))
							ac3flags[j] = 0; // todo
						else if (!strncmp("A_FLAC", TrackList[i + 1], 6))
							ac3flags[j] = 0; // todo
						else
							ac3flags[j] = 0; // todo
						std::string _language = "";
						_language += std::string(_lang);
						if (_language.compare("und") == 0)
							_language = "Stream " + std::to_string((int)i);
						//_language += " - ";
						//_language += "(";
						//_language += TrackList[i + 1];
						//_language += ")";
						language[j] = _language;
					}
				}
				free(TrackList[i]);
				free(TrackList[i + 1]);
			}
			free(TrackList);
			*numpida = j;
		}
	}
}

void cPlayback::FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	hal_info("%s\n", __func__);

	int max_numpids = *numpids;
	*numpids = 0;

	if (player && player->manager && player->manager->subtitle)
	{
		char **TrackList = NULL;
		player->manager->subtitle->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL)
		{
			printf("SubtitleTrack List\n");
			int i = 0, j = 0;
			for (i = 0, j = 0; TrackList[i] != NULL; i += 2, j++)
			{
				printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
				if (j < max_numpids)
				{
					int _pid;
					char _lang[strlen(TrackList[i])];
					if (sscanf(TrackList[i], "%d %s\n", &_pid, _lang) == 2)
					{
						pids[j] = _pid;
						language[j] = std::string(_lang);
					}
				}
				free(TrackList[i]);
				free(TrackList[i + 1]);
			}
			free(TrackList);
			*numpids = j;
		}
	}
}

void cPlayback::FindAllTeletextsubtitlePids(int */*pids*/, unsigned int *numpids, std::string */*language*/, int */*mags*/, int */*pages*/)
{
	hal_info("%s\n", __func__);
	//int max_numpids = *numpids;
	*numpids = 0;

	/*
	if (player && player->manager && player->manager->teletext)
	{
		char **TrackList = NULL;
		player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL)
		{
			printf("Teletext List\n");
			int i = 0, j = 0;
			for (i = 0, j = 0; TrackList[i] != NULL; i += 2)
			{
				int type = 0;
				printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
				if (j < max_numpids)
				{
					int _pid;
					if (2 != sscanf(TrackList[i], "%d %*s %d %*d %*d", &_pid, &type))
						continue;
					if (type != 2 && type != 5) // return subtitles only
						continue;
					pids[j] = _pid;
					language[j] = std::string(TrackList[i]);
					j++;
				}
				free(TrackList[i]);
				free(TrackList[i + 1]);
			}
			free(TrackList);
			*numpids = j;
		}
	}
	*/
}

int cPlayback::GetTeletextPid(void)
{
	hal_info("%s\n", __func__);
	int pid = -1;

	/*
	if (player && player->manager && player->manager->teletext)
	{
		char **TrackList = NULL;
		player->manager->teletext->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL)
		{
			printf("Teletext List\n");
			int i = 0;
			for (i = 0; TrackList[i] != NULL; i += 2)
			{
				int type = 0;
				printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
				if (pid < 0)
				{
					if (2 != sscanf(TrackList[i], "%*d %d %*s %d %*d %*d", &pid, &type))
						continue;
					if (type != 1)
						pid = -1;
				}
				free(TrackList[i]);
				free(TrackList[i + 1]);
			}
			free(TrackList);
		}
	}
	*/

	printf("teletext pid id %d (0x%x)\n", pid, pid);
	return pid;
}

/* dummy functions for subtitles */
void cPlayback::FindAllSubs(short unsigned int * /*pids*/, short unsigned int * /*supp*/, short unsigned int *num, std::string * /*lang*/)
{
	*num = 0;
}

bool cPlayback::SelectSubtitles(int /*pid*/, std::string /*charset*/)
{
	return false;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();

	if (player && player->manager && player->manager->chapter)
	{
		char **TrackList = NULL;
		player->manager->chapter->Command(player, MANAGER_LIST, &TrackList);
		if (TrackList != NULL)
		{
			printf("%s: Chapter List\n", __func__);
			int i = 0;
			for (i = 0; TrackList[i] != NULL; i += 2)
			{
				printf("\t%s - %s\n", TrackList[i], TrackList[i + 1]);
				int pos = atoi(TrackList[i]);
				std::string title(TrackList[i + 1]);
				positions.push_back(pos);
				titles.push_back(title);
				free(TrackList[i]);
				free(TrackList[i + 1]);
			}
			free(TrackList);
		}
	}
}

void cPlayback::GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current)
{
	playlists.clear();
	titles.clear();
	current = 0;
}

void cPlayback::SetTitle(int /*title*/)
{
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();
	char **metadata = NULL;
	if (player && player->playback)
	{
		player->playback->Command(player, PLAYBACK_METADATA, &metadata);
		if (metadata)
		{
			for (char **m = metadata; *m;)
			{
				keys.push_back(*m);
				free(*m++);
				values.push_back(*m);
				free(*m++);
			}
			free(metadata);
		}
	}
}

cPlayback::cPlayback(int num __attribute__((unused)))
{
	hal_info("%s\n", __func__);
	playing = false;
	decoders_closed = false;
	first = false;
	player = NULL;
}

cPlayback::~cPlayback()
{
	hal_info("%s\n", __func__);

	RequestAbort();
	mutex.lock();
	if (player)
	{
		free(player);
		player = NULL;
	}
	mutex.unlock();
}

void cPlayback::RequestAbort()
{
	if (player && player->playback)
	{
		hal_info("%s\n", __func__);
		mutex.lock();

		if (player && player->playback && player->playback->isPlaying)
		{
			Stop();
			player->playback->abortRequested = 1;
		}
		else if (player->playback->isHttp && !player->playback->isPlaying && !player->playback->abortRequested)
		{
			player->playback->abortRequested = 1;
		}

		mutex.unlock();

	}
}

bool cPlayback::IsPlaying()
{
	if (player && player->playback)
		return player->playback->isPlaying;
	return false;
}

uint64_t cPlayback::GetReadCount()
{
	if (player && player->playback)
	{
		return player->playback->readCount;
	}
	return 0;
}

AVFormatContext *cPlayback::GetAVFormatContext()
{
	if (player && player->container && player->container->selectedContainer)
	{
		player->container->selectedContainer->Command(player, CONTAINER_GET_AVFCONTEXT, avft);
	}
	return avft;
}

void cPlayback::ReleaseAVFormatContext()
{
	avft->streams = NULL;
	avft->nb_streams = 0;
}

#if 0
bool cPlayback::IsPlaying(void) const
{
	hal_info("%s\n", __func__);

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

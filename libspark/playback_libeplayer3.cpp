#define __USE_FILE_OFFSET64 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <audio_lib.h>
#include <video_lib.h>

#include "player.h"
#include "playback_libeplayer3.h"
#include "hal_debug.h"

#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYBACK, this, args)
#define hal_info(args...)  _hal_info(HAL_DEBUG_PLAYBACK, this, args)

extern cAudio *audioDecoder;
extern cVideo *videoDecoder;

//Used by Fileplay
bool cPlayback::Open(playmode_t PlayMode)
{
	if (PlayMode != PLAYMODE_TS)
	{
		audioDecoder->closeDevice();
		videoDecoder->closeDevice();
		decoders_closed = true;
	}

	pm = PlayMode;
	fn_ts = "";
	fn_xml = "";
	last_size = 0;
	nPlaybackSpeed = 0;
	init_jump = -1;

	return 0;
}

void cPlayback::Close(void)
{
	hal_info("%s\n", __func__);

	//Dagobert: movieplayer does not call stop, it calls close ;)
	Stop();
	if (decoders_closed)
	{
		audioDecoder->openDevice();
		videoDecoder->openDevice();
		decoders_closed = false;
	}
}

bool cPlayback::Start(std::string filename, std::string headers)
{
	return Start((char*) filename.c_str(),0,0,0,0,0, headers);
}

bool cPlayback::Start(char *filename, int vpid, int vtype, int apid, int ac3, int, std::string headers)
{
	bool ret = false;
	bool isHTTP = false;
	no_probe = false;

	hal_info("%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d\n", __func__, filename, vpid, vtype, apid, ac3);

	init_jump = -1;

	unlink("/tmp/.id3coverart");

	std::string file;
	if (*filename == '/')
		file = "file://";
	file += filename;

	if ((file.find(":31339/id=") != std::string::npos) || (file.find(":10000") != std::string::npos) || (file.find(":8001/") != std::string::npos)) // for LocalTV and Entertain-TV streaming
		no_probe = true;

	if (file.substr(0, 7) == "file://") {
		if (file.substr(file.length() - 3) ==  ".ts") {
			fn_ts = file.substr(7);
			fn_xml = file.substr(7, file.length() - 9);
			fn_xml += "xml";
			no_probe = true;
		}
	} else
		isHTTP = true;

	if (player->Open(file.c_str(), no_probe, headers)) {
		if (pm == PLAYMODE_TS) {
			struct stat64 s;
			if (!stat64(file.c_str(), &s))
				last_size = s.st_size;
			ret = true;
			videoDecoder->Stop(false);
			audioDecoder->Stop();
		} else {
			std::vector<std::string> keys, values;
			int selected_program = 0;
			if (vpid || apid) {
				;
			} else if (GetPrograms(keys, values) && (keys.size() > 1) && ProgramSelectionCallback) {
				const char *key = ProgramSelectionCallback(ProgramSelectionCallbackData, keys, values);
				if (!key) {
					player->Close();
					return false;
				}
				selected_program = atoi(key);
			} else if (keys.size() > 0)
				selected_program = atoi(keys[0].c_str());

			if (!keys.size() || !player->SelectProgram(selected_program)) {
				if (apid)
					SetAPid(apid);
				if (vpid)
					SetVPid(vpid);
			}
			playing = true;
			player->output.Open();
			ret = player->Play();
			if (ret && !isHTTP)
				playing = ret = player->Pause();
		}
	}

	return ret;
}

bool cPlayback::Stop(void)
{
	hal_info("%s playing %d\n", __func__, playing);

	player->Stop();
	player->output.Close();
	player->Close();

	playing = false;
	return true;
}

bool cPlayback::SetAPid(int pid, bool /* ac3 */)
{
	hal_info("%s:(%d)\n", __func__, pid);
	return player->SwitchAudio(pid);
}

bool cPlayback::SetVPid(int pid)
{
	return player->SwitchVideo(pid);
}

bool cPlayback::SetSubtitlePid(int pid)
{
	return player->SwitchSubtitle(pid);
}

bool cPlayback::SetTeletextPid(int pid)
{
	return player->SwitchTeletext(pid);
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
		player->output.Open();
		playing = player->Play();
	}

	if (!playing)
		return false;

	bool res = true;

	nPlaybackSpeed = speed;

	if (speed > 1) {
		/* direction switch ? */
		if (player->isBackWard)
			player->FastBackward(0);
		res = player->FastForward(speed);
	} else if (speed < 0) {
		/* direction switch ? */
		if (player->isForwarding)
			player->Continue();
		res = player->FastBackward(speed);
	} else if (speed == 0) {
		/* konfetti: hmmm accessing the member isn't very proper */
		if ((player->isForwarding) || (!player->isBackWard))
			/* res = */ player->Pause();
		else
			/* res = */ player->FastForward(0);
	} else /* speed == 1 */ {
		res = player->Continue();
	}

	if (init_jump > -1) {
		SetPosition(init_jump);
		init_jump = -1;
	}

	return res;
}

bool cPlayback::GetSpeed(int &speed) const
{
	hal_debug("%s\n", __func__);
	speed = nPlaybackSpeed;
	return true;
}

void cPlayback::GetPts(uint64_t &pts)
{
	player->GetPts((int64_t &) pts);
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
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

	if (!player->isPlaying) {
		hal_info("%s !!!!EOF!!!! < -1\n", __func__);
		position = duration + 1000;
		// duration = 0;
		// this is stupid
		return false;
	}

	int64_t vpts = 0;
	player->GetPts(vpts);

	if(vpts <= 0) {
		//printf("ERROR: vpts==0");
	} else {
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts/90;
	}

	if (got_duration)
		return true;

	int64_t length = 0;

	player->GetDuration(length);

	if(length <= 0)
		duration = position + AV_TIME_BASE / 1000;
	else
		duration = length * 1000 / AV_TIME_BASE;

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	hal_info("%s %d\n", __func__, position);
	if (!playing)
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
	player->Seek((int64_t)position * (AV_TIME_BASE / 1000), absolute);
	return true;
}

void cPlayback::FindAllPids(int *pids, unsigned int *ac3flags, unsigned int *numpids, std::string *language)
{
	hal_info("%s\n", __func__);
	unsigned int i = 0;

	if (IsPlaying()) {
		std::vector<Track> tracks = player->manager.getAudioTracks();
		for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
			pids[i] = it->pid;
			ac3flags[i] = it->ac3flags;
			language[i] = it->title;
			i++;
		}
	}
	*numpids = i;
}

void cPlayback::FindAllSubtitlePids(int *pids, unsigned int *numpids, std::string *language)
{
	hal_info("%s\n", __func__);
	unsigned int i = 0;

	std::vector<Track> tracks = player->manager.getSubtitleTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
		pids[i] = it->pid;
		language[i] = it->title;
		i++;
	}

	*numpids = i;
}

void cPlayback::FindAllTeletextsubtitlePids(int *pids, unsigned int *numpids, std::string *language, int *mags, int *pages)
{
	hal_info("%s\n", __func__);
	unsigned int i = 0;

	std::vector<Track> tracks = player->manager.getTeletextTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
		if (it->type != 2 && it->type != 5) // return subtitles only
			continue;
		pids[i] = it->pid;
		language[i] = it->title;
		mags[i] = it->mag;
		pages[i] = it->page;
		i++;
	}

	*numpids = i;
}

int cPlayback::GetFirstTeletextPid(void)
{
	std::vector<Track> tracks = player->manager.getTeletextTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end(); ++it) {
		if (it->type == 1)
			return it->pid;
	}
	return -1;
}

/* dummy functions for subtitles */
void cPlayback::FindAllSubs(uint16_t * /*pids*/, unsigned short * /*supp*/, uint16_t *num, std::string * /*lang*/)
{
	*num = 0;
}

bool cPlayback::SelectSubtitles(int /*pid*/)
{
	return false;
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

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	player->GetChapters(positions, titles);
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	player->input.GetMetadata(keys, values);
}

cPlayback::cPlayback(int num __attribute__((unused)))
{
	hal_info("%s\n", __func__);
	playing = false;
	decoders_closed = false;
	ProgramSelectionCallback = NULL;
	ProgramSelectionCallbackData = NULL;

	player = new Player();
}

cPlayback::~cPlayback()
{
	hal_info("%s\n", __func__);
	delete player;
}

void cPlayback::RequestAbort() {
	player->RequestAbort();
	while (player->isPlaying)
		usleep(100000);
}

bool cPlayback::IsPlaying() {
	return player->isPlaying;
}

uint64_t cPlayback::GetReadCount() {
	return player->readCount;
}

int cPlayback::GetAPid(void)
{
	hal_info("%s\n", __func__);
	return player->GetAudioPid();
}

int cPlayback::GetVPid(void)
{
	return player->GetVideoPid();
}

int cPlayback::GetSubtitlePid(void)
{
	return player->GetSubtitlePid();
}

int cPlayback::GetTeletextPid(void)
{
	return player->GetTeletextPid();
}

AVFormatContext *cPlayback::GetAVFormatContext()
{
	return player ? player->GetAVFormatContext() : NULL;
}

void cPlayback::ReleaseAVFormatContext()
{
	if (player)
		player->ReleaseAVFormatContext();
}

bool cPlayback::GetPrograms(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	return player->GetPrograms(keys, values);
}

bool cPlayback::SelectProgram(std::string &key)
{
	return player->SelectProgram(key);
}

void cPlayback::SetProgramSelectionCallback(const char *(*fun)(void *, std::vector<std::string> &keys, std::vector<std::string> &values), void *opaque)
{
	ProgramSelectionCallback = fun;
	ProgramSelectionCallbackData = opaque;
}

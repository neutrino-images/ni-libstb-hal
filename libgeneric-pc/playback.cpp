#include <stdio.h>

#include "playback_lib.h"

static const char *FILENAME = "playback-dummy";

bool cPlayback::Open(playmode_t)
{
	return 0;
}

void cPlayback::Close(void)
{
}

bool cPlayback::Start(std::string filename, std::string headers)
{
	return Start((char *) filename.c_str(), 0, 0, 0, 0, 0, headers);
}

bool cPlayback::Start(char *filename, int vpid, int vtype, int apid, int ac3, int duration, std::string /*headers*/)
{
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d duration=%i\n",
		FILENAME, __func__, filename, vpid, vtype, apid, ac3, duration);
	return true;
}

bool cPlayback::SetAPid(int pid, bool /*ac3*/)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}

bool cPlayback::SelectSubtitles(int pid, std::string charset)
{
	printf("%s:%s pid %i, charset: %s\n", FILENAME, __func__, pid, charset.c_str());
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __func__, playing, speed);
	return true;
}

bool cPlayback::GetSpeed(int &/*speed*/) const
{
	return true;
}

bool cPlayback::GetPosition(int &position, int &duration, bool /*isWebChannel*/)
{
	printf("%s:%s %d %d\n", FILENAME, __func__, position, duration);
	position = 0;
	duration = 0;
	return true;
}

bool cPlayback::SetPosition(int position, bool)
{
	printf("%s:%s %d\n", FILENAME, __func__, position);
	return true;
}

void cPlayback::FindAllPids(int *, unsigned int *, unsigned int *numpida, std::string *)
{
	printf("%s:%s\n", FILENAME, __func__);
	*numpida = 0;
}

void cPlayback::FindAllSubtitlePids(int * /*pids*/, unsigned int *numpids, std::string * /*language*/)
{
	*numpids = 0;
}

bool cPlayback::SetSubtitlePid(int /*pid*/)
{
	return true;
}

void cPlayback::GetPts(uint64_t &/*pts*/)
{
}

bool cPlayback::SetTeletextPid(int /*pid*/)
{
	return true;
}

void cPlayback::FindAllTeletextsubtitlePids(int *, unsigned int *numpids, std::string *, int *, int *)
{
	*numpids = 0;
}

void cPlayback::SuspendSubtitle(bool /*b*/)
{
}

void cPlayback::RequestAbort()
{
}

int cPlayback::GetTeletextPid(void)
{
	return -1;
}

void cPlayback::FindAllSubs(int * /*pids*/, unsigned int * /*supp*/, unsigned int *num, std::string * /*lang*/)
{
	printf("%s:%s\n", FILENAME, __func__);
	*num = 0;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
}

void cPlayback::GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values)
{
	keys.clear();
	values.clear();
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

uint64_t cPlayback::GetReadCount(void)
{
	return 0;
}

cPlayback::cPlayback(int /*num*/)
{
	printf("%s:%s\n", FILENAME, __func__);
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __func__);
}

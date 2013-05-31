#include <stdio.h>

#include "playback.h"

static const char * FILENAME = "playback-dummy";

bool cPlayback::Open(playmode_t)
{
	return 0;
}

void cPlayback::Close(void)
{
}

#ifdef MARTII
bool cPlayback::Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3, int duration, bool /*noprobe*/)
#else
bool cPlayback::Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3, int duration)
#endif
{
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d duration=%i\n",
		FILENAME, __func__, filename, vpid, vtype, apid, ac3, duration);
	return true;
}

bool cPlayback::SetAPid(unsigned short pid, bool /*ac3*/)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}

#ifndef MARTII
bool cPlayback::SetSPid(int pid)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}
#endif

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __func__, playing, speed);
	return true;
}

bool cPlayback::GetSpeed(int &/*speed*/) const
{
	return true;
}

bool cPlayback::GetPosition(int &position, int &duration)
{
	printf("%s:%s %d %d\n", FILENAME, __func__, position, duration);
	position = 0;
	duration = 0;
	return true;
}

bool cPlayback::SetPosition(int position, bool)
{
	printf("%s:%s %d\n", FILENAME, __func__,position);
	return true;
}

void cPlayback::FindAllPids(uint16_t *, unsigned short *, uint16_t *numpida, std::string *)
{
	printf("%s:%s\n", FILENAME, __func__);
	*numpida = 0;
}
#ifdef MARTII
void cPlayback::FindAllSubtitlePids(uint16_t * /*pids*/, uint16_t *numpids, std::string * /*language*/)
{
	*numpids = 0;
}

bool cPlayback::SetSubtitlePid(unsigned short /*pid*/)
{
	return true;
}

bool cPlayback::SetDvbsubtitlePid(unsigned short /*pid*/)
{
	return true;
}

bool cPlayback::SetTeletextPid(unsigned short /*pid*/)
{
	return true;
}

void cPlayback::FindAllDvbsubtitlePids(uint16_t * /*pids*/, uint16_t *numpids, std::string * /*language*/)
{
	*numpids = 0;
}

void cPlayback::FindAllTeletextsubtitlePids(uint16_t * /*pids*/, uint16_t *numpids, std::string * /*language*/)
{
	*numpids = 0;
}

void cPlayback::SuspendSubtitle(bool /*b*/)
{
}

void cPlayback::RequestAbort()
{
}

unsigned short cPlayback::GetTeletextPid(void)
{
}
#endif

cPlayback::cPlayback(int /*num*/)
{
	printf("%s:%s\n", FILENAME, __func__);
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __func__);
}

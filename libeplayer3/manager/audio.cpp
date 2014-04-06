/*
 * audio manager handling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* ***************************** */
/* Includes                      */
/* ***************************** */

#include <stdlib.h>
#include <string.h>
#include <map>

#include "manager.h"
#include "common.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define AUDIO_MGR_DEBUG

#ifdef AUDIO_MGR_DEBUG

static short debug_level = 0;

#define audio_mgr_printf(level, x...) do { \
if (debug_level >= level) printf(x); } while (0)
#else
#define audio_mgr_printf(level, x...)
#endif

#ifndef AUDIO_MGR_SILENT
#define audio_mgr_err(x...) do { printf(x); } while (0)
#else
#define audio_mgr_err(x...)
#endif

/* Error Constants */
#define cERR_AUDIO_MGR_NO_ERROR        0
#define cERR_AUDIO_MGR_ERROR          -1

static const char FILENAME[] = __FILE__;

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static std::map<int,Track_t> Tracks;
static int CurrentPid = -1;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int ManagerAdd(Context_t * context, Track_t track)
{
	Tracks[track.Id] = track;
	context->playback->isAudio = 1;

	if (CurrentPid < 0)
		CurrentPid = track.Id;

	return cERR_AUDIO_MGR_NO_ERROR;
}

static char **ManagerList(Context_t * context __attribute__ ((unused)))
{
	int j = 0;
	char **tracklist = (char **) malloc(sizeof(char *) * ((Tracks.size() * 2) + 1));

	for(std::map<int,Track_t>::iterator it = Tracks.begin(); it != Tracks.end(); ++it)
	{
		size_t len = it->second.Name.length() + 20;
		char tmp[len];
		snprintf(tmp, len, "%d %s\n", it->second.Id, it->second.Name.c_str());
		tracklist[j] = strdup(tmp);
		snprintf(tmp, len, "%d\n", it->second.ac3flags);
		tracklist[j + 1] = strdup(tmp);
		j += 2;
	}
	tracklist[j] = NULL;

	return tracklist;
}

static int ManagerDel(Context_t * context)
{
	Tracks.clear();
	CurrentPid = -1;
	context->playback->isAudio = 0;
	return cERR_AUDIO_MGR_NO_ERROR;
}


static int Command(Context_t *context, ManagerCmd_t command, void *argument)
{
    int ret = cERR_AUDIO_MGR_NO_ERROR;

    audio_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    switch (command) {
    case MANAGER_ADD:{
	    Track_t *track = (Track_t *) argument;
	    ret = ManagerAdd(context, *track);
	    break;
	}
    case MANAGER_LIST:{
	    container_ffmpeg_update_tracks(context, context->playback->uri.c_str());
	    *((char ***) argument) = (char **) ManagerList(context);
	    break;
	}
    case MANAGER_GET:{
		*((int *) argument) = (int) CurrentPid;
		break;
	}
    case MANAGER_GET_TRACK:{
		if (CurrentPid > -1)
			*((Track_t **) argument) = &Tracks[CurrentPid];
		else
			*((Track_t **) argument) = NULL;
		break;
	}
    case MANAGER_GETNAME:{
		if (CurrentPid > -1)
			*((char **) argument) = strdup(Tracks[CurrentPid].Name.c_str());
		else
			*((char **) argument) = strdup("");
		break;
	}
    case MANAGER_SET:{
		std::map<int,Track_t>::iterator it = Tracks.find(*((int *) argument));
		if (it != Tracks.end())
			CurrentPid = *((int *) argument);
		else
			ret = cERR_AUDIO_MGR_ERROR;
		break;
	}
    case MANAGER_DEL:{
		ret = ManagerDel(context);
		break;
	}
    case MANAGER_INIT_UPDATE:{
		for (std::map<int,Track_t>::iterator it = Tracks.begin(); it != Tracks.end(); ++it)
			it->second.pending = 1;
		break;
	}
    default:
		audio_mgr_err("%s::%s ContainerCmd %d not supported!\n", FILENAME, __FUNCTION__, command);
		ret = cERR_AUDIO_MGR_ERROR;
		break;
    }

    audio_mgr_printf(10, "%s:%s: returning %d\n", FILENAME, __FUNCTION__, ret);

    return ret;
}


struct Manager_s AudioManager = {
    "Audio",
    &Command,
    NULL
};

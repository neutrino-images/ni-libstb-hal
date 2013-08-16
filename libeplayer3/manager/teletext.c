/*
 * teletext manager handling.
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

#include <libavformat/avformat.h>
#include "manager.h"
#include "common.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */
#define TRACKWRAP 20

#define TELETEXT_MGR_DEBUG

#ifdef TELETEXT_MGR_DEBUG

static short debug_level = 0;

#define teletext_mgr_printf(level, x...) do { \
if (debug_level >= level) printf(x); } while (0)
#else
#define teletext_mgr_printf(level, x...)
#endif

#ifndef TELETEXT_MGR_SILENT
#define teletext_mgr_err(x...) do { printf(x); } while (0)
#else
#define teletext_mgr_err(x...)
#endif

/* Error Constants */
#define cERR_TELETEXT_MGR_NO_ERROR        0
#define cERR_TELETEXT_MGR_ERROR          -1

static const char FILENAME[] = __FILE__;

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static Track_t * Tracks = NULL;
static int TrackCount = 0;
static int CurrentTrack = -1;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int ManagerAdd(Context_t  *context, Track_t track) {

    teletext_mgr_printf(10, "%s::%s name=\"%s\" encoding=\"%s\" id=%d\n", FILENAME, __FUNCTION__, track.Name, track.Encoding, track.Id);

    if (Tracks == NULL) {
        Tracks = malloc(sizeof(Track_t) * TRACKWRAP);
	int i;
	for (i = 0; i < TRACKWRAP; i++)
		Tracks[i].Id = -1;
    }

    if (Tracks == NULL)
    {
        teletext_mgr_err("%s:%s malloc failed\n", FILENAME, __FUNCTION__);
        return cERR_TELETEXT_MGR_ERROR;
    }

    int i;
    for (i = 0; i < TRACKWRAP; i++) {
	if (Tracks[i].Id == track.Id) {
		Tracks[i].pending = 0;
        	return cERR_TELETEXT_MGR_NO_ERROR;
	}
    }

    if (TrackCount < TRACKWRAP) {
        copyTrack(&Tracks[TrackCount], &track);
        TrackCount++;
    } else {
        teletext_mgr_err("%s:%s TrackCount out if range %d - %d\n", FILENAME, __FUNCTION__, TrackCount, TRACKWRAP);
        return cERR_TELETEXT_MGR_ERROR;
    }

    if (TrackCount > 0)
        context->playback->isTeletext = 1;

    teletext_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    return cERR_TELETEXT_MGR_NO_ERROR;
}

static char ** ManagerList(Context_t  *context __attribute__((unused))) {
    int i = 0, j = 0;
    char ** tracklist = NULL;

    teletext_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    if (Tracks != NULL) {

        tracklist = malloc(sizeof(char *) * ((TrackCount*2) + 1));

        if (tracklist == NULL)
        {
            teletext_mgr_err("%s:%s malloc failed\n", FILENAME, __FUNCTION__);
            return NULL;
        }

        for (i = 0, j = 0; i < TrackCount; i++, j+=2) {
	    if (Tracks[i].pending)
		continue;
	    size_t len = strlen(Tracks[i].Name) + 20;
	    char tmp[len];
	    snprintf(tmp, len, "%d %s\n", Tracks[i].Id, Tracks[i].Name);
            tracklist[j]    = strdup(tmp);
            tracklist[j+1]  = strdup(Tracks[i].Encoding);
        }
        tracklist[j] = NULL;
    }

    teletext_mgr_printf(10, "%s::%s return %p (%d - %d)\n", FILENAME, __FUNCTION__, tracklist, j, TrackCount);

    return tracklist;
}

static int ManagerDel(Context_t * context) {

    int i = 0;

    teletext_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    if(Tracks != NULL) {
        for (i = 0; i < TrackCount; i++) {
            freeTrack(&Tracks[i]);
        }
        free(Tracks);
        Tracks = NULL;
    } else
    {
        teletext_mgr_err("%s::%s nothing to delete!\n", FILENAME, __FUNCTION__);
        return cERR_TELETEXT_MGR_ERROR;
    }

    TrackCount = 0;
    CurrentTrack = -1;
    context->playback->isTeletext = 0;

    teletext_mgr_printf(10, "%s::%s return no error\n", FILENAME, __FUNCTION__);

    return cERR_TELETEXT_MGR_NO_ERROR;
}


static int Command(void  *_context, ManagerCmd_t command, void * argument) {
    Context_t  *context = (Context_t*) _context;
    int ret = cERR_TELETEXT_MGR_NO_ERROR;

    teletext_mgr_printf(10, "%s::%s\n", FILENAME, __FUNCTION__);

    switch(command) {
    case MANAGER_ADD: {
        Track_t * track = argument;

        ret = ManagerAdd(context, *track);
        break;
    }
    case MANAGER_LIST: {
	container_ffmpeg_update_tracks(context, context->playback->uri, 0);
        *((char***)argument) = (char **)ManagerList(context);
        break;
    }
    case MANAGER_GET: {
        teletext_mgr_printf(20, "%s::%s MANAGER_GET\n", FILENAME, __FUNCTION__);

        if ((TrackCount > 0) && (CurrentTrack >=0))
            *((int*)argument) = (int)Tracks[CurrentTrack].Id;
        else
            *((int*)argument) = (int)-1;
        break;
    }
    case MANAGER_GET_TRACK: {
        teletext_mgr_printf(20, "%s::%s MANAGER_GET_TRACK\n", FILENAME, __FUNCTION__);

        if ((TrackCount > 0) && (CurrentTrack >=0))
            *((Track_t**)argument) = (Track_t*) &Tracks[CurrentTrack];
        else
            *((Track_t**)argument) = NULL;
        break;
    }
    case MANAGER_GETENCODING: {
        if ((TrackCount > 0) && (CurrentTrack >=0))
            *((char**)argument) = (char *)strdup(Tracks[CurrentTrack].Encoding);
        else
            *((char**)argument) = (char *)strdup("");
        break;
    }
    case MANAGER_GETNAME: {
        if ((TrackCount > 0) && (CurrentTrack >=0))
            *((char**)argument) = (char *)strdup(Tracks[CurrentTrack].Name);
        else
            *((char**)argument) = (char *)strdup("");
        break;
    }
    case MANAGER_SET: {
	int i;
        teletext_mgr_printf(20, "%s::%s MANAGER_SET id=%d\n", FILENAME, __FUNCTION__, *((int*)argument));

	for (i = 0; i < TrackCount; i++)
		if (Tracks[i].Id == *((int*)argument)) {
			CurrentTrack = i;
			break;
		}

        if (i == TrackCount) {
            teletext_mgr_err("%s::%s track id %d unknown\n", FILENAME, __FUNCTION__, *((int*)argument));
            ret = cERR_TELETEXT_MGR_ERROR;
        }
        break;
    }
    case MANAGER_DEL: {
        ret = ManagerDel(context);
        break;
    }
    case MANAGER_INIT_UPDATE: {
	int i;
	for (i = 0; i < TrackCount; i++)
		Tracks[i].pending = 1;
        break;
    }
    default:
        teletext_mgr_err("%s::%s ContainerCmd %d not supported!\n", FILENAME, __FUNCTION__, command);
        ret = cERR_TELETEXT_MGR_ERROR;
        break;
    }

    teletext_mgr_printf(10, "%s:%s: returning %d\n", FILENAME, __FUNCTION__,ret);

    return ret;
}


struct Manager_s TeletextManager = {
    "Teletext",
    &Command,
    NULL
};

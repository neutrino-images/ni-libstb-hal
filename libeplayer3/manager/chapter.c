/*
 * chapter manager handling.
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

#include "manager.h"
#include "common.h"
#include "debug.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define TRACKWRAP 20

/* Error Constants */
#define cERR_CHAPTER_MGR_NO_ERROR        0
#define cERR_CHAPTER_MGR_ERROR          -1

/* ***************************** */
/* Types                         */
/* ***************************** */

/* ***************************** */
/* Variables                     */
/* ***************************** */

static Track_t *Tracks = NULL;
static int TrackCount = 0;
static int CurrentTrack = 0;    //TRACK[0] as default.

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* Functions                     */
/* ***************************** */

static int ManagerAdd(Context_t *context __attribute__((unused)), Track_t track)
{
	chapter_mgr_printf(10, "name=\"%s\" encoding=\"%s\" id=%d\n", track.Name, track.Encoding, track.Id);

	if (Tracks == NULL)
	{
		Tracks = malloc(sizeof(Track_t) * TRACKWRAP);
		int i;
		for (i = 0; i < TRACKWRAP; i++)
		{
			Tracks[i].Id = -1;
		}
	}

	if (Tracks == NULL)
	{
		chapter_mgr_err("malloc failed\n");
		return cERR_CHAPTER_MGR_ERROR;
	}

	int i;
	for (i = 0; i < TRACKWRAP; i++)
	{
		if (Tracks[i].Id == track.Id)
		{
			Tracks[i].pending = 0;
			return cERR_CHAPTER_MGR_NO_ERROR;
		}
	}

	if (TrackCount < TRACKWRAP)
	{
		copyTrack(&Tracks[TrackCount], &track);

		TrackCount++;
	}
	else
	{
		chapter_mgr_err("TrackCount out if range %d - %d\n", TrackCount, TRACKWRAP);
		return cERR_CHAPTER_MGR_ERROR;
	}

	chapter_mgr_printf(10, "\n");

	return cERR_CHAPTER_MGR_NO_ERROR;
}

static char **ManagerList(Context_t *context __attribute__((unused)))
{
	int i = 0, j = 0;
	char **tracklist = NULL;

	chapter_mgr_printf(10, "\n");

	if (Tracks != NULL)
	{
		tracklist = malloc(sizeof(char *) * ((TrackCount * 2) + 1));

		if (tracklist == NULL)
		{
			chapter_mgr_err("malloc failed\n");
			return NULL;
		}

		for (i = 0, j = 0; i < TrackCount; i++, j += 2)
		{
			if (Tracks[i].pending)
			{
				continue;
			}

			char tmp[20];
			snprintf(tmp, sizeof(tmp), "%d", (int)Tracks[i].chapter_start);
			tracklist[j] = strdup(tmp);
			tracklist[j + 1] = strdup(Tracks[i].Name);
		}
		tracklist[j] = NULL;
	}

	chapter_mgr_printf(10, "return %p (%d - %d)\n", tracklist, j, TrackCount);

	return tracklist;
}

static int ManagerDel(Context_t *context __attribute__((unused)))
{
	int i = 0;

	chapter_mgr_printf(10, "\n");

	if (Tracks != NULL)
	{
		for (i = 0; i < TrackCount; i++)
		{
			freeTrack(&Tracks[i]);
		}
		free(Tracks);
		Tracks = NULL;
	}
	else
	{
		chapter_mgr_err("nothing to delete!\n");
		return cERR_CHAPTER_MGR_ERROR;
	}

	TrackCount = 0;
	CurrentTrack = 0;

	chapter_mgr_printf(10, "return no error\n");

	return cERR_CHAPTER_MGR_NO_ERROR;
}

static int Command(Context_t *context, ManagerCmd_t command, void *argument)
{
	int ret = cERR_CHAPTER_MGR_NO_ERROR;

	chapter_mgr_printf(10, "\n");

	switch (command)
	{
		case MANAGER_ADD:
		{
			Track_t *track = argument;
			ret = ManagerAdd(context, *track);
			break;
		}
		case MANAGER_LIST:
		{
			container_ffmpeg_update_tracks(context, context->playback->uri, 0);
			*((char ***) argument) = (char **) ManagerList(context);
			break;
		}
		case MANAGER_DEL:
		{
			ret = ManagerDel(context);
			break;
		}
		case MANAGER_INIT_UPDATE:
		{
			int i;
			for (i = 0; i < TrackCount; i++)
			{
				Tracks[i].pending = 1;
			}
			break;
		}
		default:
			chapter_mgr_err("ContainerCmd %d not supported!\n", command);
			ret = cERR_CHAPTER_MGR_ERROR;
			break;
	}

	chapter_mgr_printf(10, "returning %d\n", ret);

	return ret;
}

struct Manager_s ChapterManager =
{
	"Chapter",
	&Command,
	NULL
};

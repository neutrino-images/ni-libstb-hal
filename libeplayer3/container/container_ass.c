/*
 * Container handling for subtitles handled by libass
 * konfetti 2010; based on code from crow
 *
 * The subtitle handling as container is not a very proper solution, in
 * a proper architecture this should be handled as subcontainer or something
 * like that. But we dont want to make more effort as necessary here ;)
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <ass/ass.h>

#include "common.h"
#include "misc.h"
#include "subtitle.h"
#include "writer.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define ASS_DEBUG

#ifdef ASS_DEBUG

static short debug_level = 10;

#define ass_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define ass_printf(level, fmt, x...)
#endif

#ifndef ASS_SILENT
#define ass_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define ass_err(fmt, x...)
#endif

/* Error Constants */
#define cERR_CONTAINER_ASS_NO_ERROR        0
#define cERR_CONTAINER_ASS_ERROR          -1

#define ASS_RING_SIZE 5

#define ASS_FONT "/share/fonts/neutrino.ttf"

/* ***************************** */
/* Types                         */
/* ***************************** */

typedef struct ass_s {
    unsigned char *data;
    int len;
    unsigned char *extradata;
    int extralen;

    long long int pts;
    float duration;
} ass_t;

typedef struct region_s {
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
    time_t undisplay;

    struct region_s *next;
} region_t;

/* ***************************** */
/* Varaibles                     */
/* ***************************** */

static pthread_mutex_t mutex;

static pthread_t PlayThread;
static int hasPlayThreadStarted = 0;

static unsigned char isContainerRunning = 0;

static ASS_Library *ass_library;
static ASS_Renderer *ass_renderer;

//static float ass_font_scale = 0.4; // was: 0.7
//static float ass_line_spacing = 0.4; // was: 0.7

static unsigned int screen_width = 0;
static unsigned int screen_height = 0;
static uint32_t *destination = NULL;
static int destStride = 0;
static void (*framebufferBlit) (void) = NULL;

static int needsBlit = 0;

static ASS_Track *ass_track = NULL;

static region_t *firstRegion = NULL;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

void ass_msg_callback(int level
		      __attribute__ ((unused)), const char *format,
		      va_list va, void *ctx __attribute__ ((unused)))
{
    int n;
    char *str;
    va_list dst;

    va_copy(dst, va);
    n = vsnprintf(NULL, 0, format, va);
    if (n > 0 && (str = malloc(n + 1))) {
	vsnprintf(str, n + 1, format, dst);
	ass_printf(100, "%s\n", str);
	free(str);
    }
}

static void getMutex(int line)
{
    ass_printf(150, "%d requesting mutex\n", line);

    pthread_mutex_lock(&mutex);

    ass_printf(150, "%d received mutex\n", line);
}

static void releaseMutex(int line)
{
    pthread_mutex_unlock(&mutex);

    ass_printf(150, "%d released mutex\n", line);
}

/* ********************************* */
/* Region Undisplay handling         */
/* ********************************* */

/* release and undisplay all saved regions
 */
void releaseRegions()
{
    region_t *next, *old;
    Writer_t *writer;

    if (firstRegion == NULL)
	return;

    writer = getDefaultFramebufferWriter();

    if (writer == NULL) {
	ass_err("no framebuffer writer found!\n");
    }

    next = firstRegion;
    while (next) {
	if (writer) {
	    WriterFBCallData_t out;

	    ass_printf(100, "release: w %d h %d x %d y %d\n",
		       next->w, next->h, next->x, next->y);

	    out.data = NULL;
	    out.Width = next->w;
	    out.Height = next->h;
	    out.x = next->x;
	    out.y = next->y;

	    out.Screen_Width = screen_width;
	    out.Screen_Height = screen_height;
	    out.destination = destination;
	    out.destStride = destStride;

	    writer->writeData(&out);
	    needsBlit = 1;
	}
	old = next;
	next = next->next;
	free(old);
    }

    firstRegion = NULL;
}

/* check for regions which should be undisplayed. 
 * we are very tolerant on time here, because
 * regions are also released when new regions are
 * detected (see ETSI EN 300 743 Chapter Page Composition)
 */
void checkRegions()
{
#define cDeltaTime 2
    region_t *next, *old, *prev;
    Writer_t *writer;
    time_t now = time(NULL);

    if (firstRegion == NULL)
	return;

    writer = getDefaultFramebufferWriter();

    if (!writer) {
	ass_err("no framebuffer writer found!\n");
    }

    prev = next = firstRegion;
    while (next) {
	if (now > next->undisplay + cDeltaTime) {
	    ass_printf(100, "undisplay: %ld > %ld\n", now,
		       next->undisplay + cDeltaTime);

	    if (writer) {
		WriterFBCallData_t out;

		ass_printf(100, "release: w %d h %d x %d y %d\n",
			   next->w, next->h, next->x, next->y);

		out.data = NULL;
		out.Width = next->w;
		out.Height = next->h;
		out.x = next->x;
		out.y = next->y;

		out.Screen_Width = screen_width;
		out.Screen_Height = screen_height;
		out.destination = destination;
		out.destStride = destStride;

		writer->writeData(&out);
		needsBlit = 1;
	    }

	    old = next;
	    next = prev->next = next->next;

	    if (old == firstRegion)
		firstRegion = next;
	    free(old);
	} else {
	    prev = next;
	    next = next->next;
	}
    }
}

/* store a display region for later release */
void storeRegion(unsigned int x, unsigned int y, unsigned int w,
		 unsigned int h, time_t undisplay)
{
    region_t **new = &firstRegion;

    ass_printf(100, "%d %d %d %d %ld\n", x, y, w, h, undisplay);

    while (*new)
	new = &(*new)->next;

    *new = malloc(sizeof(region_t));

    (*new)->next = NULL;
    (*new)->x = x;
    (*new)->y = y;
    (*new)->w = w;
    (*new)->h = h;
    (*new)->undisplay = undisplay;
}

/* **************************** */
/* Worker Thread                */
/* **************************** */

static void ASSThread(Context_t * context)
{
    char threadname[17];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[16] = 0;
    prctl(PR_SET_NAME, (unsigned long) &threadname);
    Writer_t *writer;

    ass_printf(10, "\n");

    while (context->playback->isCreationPhase) {
	ass_err("Thread waiting for end of init phase...\n");
	usleep(1000);
    }

    ass_printf(10, "Running!\n");

    writer = getDefaultFramebufferWriter();

    if (writer == NULL) {
	ass_err("no framebuffer writer found!\n");
    }

    while (context && context->playback && context->playback->isPlaying) {

	//IF MOVIE IS PAUSED, WAIT
	if (context->playback->isPaused) {
	    ass_printf(20, "paused\n");

	    usleep(100000);
	    continue;
	}

	if (context->playback->isSeeking) {
	    ass_printf(10, "seeking\n");

	    usleep(100000);
	    continue;
	}

	if ((isContainerRunning) && (ass_track)) {
	    ASS_Image *img = NULL;
	    int change = 0;
	    unsigned long int playPts;

	    if (context && context->playback) {
		if (context->playback->
		    Command(context, PLAYBACK_PTS, &playPts) < 0)
		    continue;
	    }
	    //FIXME: durch den sleep bleibt die cpu usage zw. 5 und 13%, ohne
	    //       steigt sie bei Verwendung von subtiteln bis auf 95%.
	    //       ich hoffe dadurch gehen keine subtitle verloren, wenn die playPts
	    //       durch den sleep verschlafen wird. Besser wäre es den nächsten
	    //       subtitel zeitpunkt zu bestimmen und solange zu schlafen.
	    usleep(10000);
	    if (!context->playback->mayWriteToFramebuffer)
		continue;
	    getMutex(__LINE__);
	    checkRegions();

	    if (ass_renderer && ass_track)
		img =
		    ass_render_frame(ass_renderer, ass_track,
				     playPts / 90.0, &change);

	    ass_printf(150, "img %p pts %lu %f\n", img, playPts,
		       playPts / 90.0);

	    if (img && ass_renderer && ass_track) {
		/* the spec says, that if a new set of regions is present
		 * the complete display switches to the new state. So lets
		 * release the old regions on display.
		 */
		if (change != 0)
		    releaseRegions();

		time_t now = time(NULL);
		time_t undisplay = now + 10;

		if (ass_track && ass_track->events)
		    undisplay =
			now + (ass_track->events->Duration + 500) / 90000;

		ASS_Image *it;
		int x0 = screen_width - 1;
		int y0 = screen_height - 1;
		int x1 = 0;
		int y1 = 0;
		for (it = img; it; it = it->next) {
		    if (it->w && it->h) {
			if (it->dst_x < x0)
			    x0 = it->dst_x;
			if (it->dst_y < y0)
			    y0 = it->dst_y;
			if (it->dst_x + it->w > x1)
			    x1 = it->dst_x + it->w;
			if (it->dst_y + it->h > y1)
			    y1 = it->dst_y + it->h;
		    }
		}
		if (x1 > 0 && y1 > 0) {
		    x1++;
		    y1++;
		    int x, y;
		    uint32_t *dst =
			destination + y0 * destStride / sizeof(uint32_t) +
			x0;
		    int destStrideDiff =
			destStride / sizeof(uint32_t) - (x1 - x0);
		    for (y = y0; y < y1; y++) {
			for (x = x0; x < x1; x++)
			    *dst++ = 0x80808080;
			dst += destStrideDiff;
		    }
		    storeRegion(x0, y0, x1 - x0, y1 - y0, undisplay);
		    needsBlit = 1;
		}

		while (context && context->playback
		       && context->playback->isPlaying && img) {
		    WriterFBCallData_t out;

		    ass_printf(100,
			       "w %d h %d s %d x %d y %d c %d chg %d now %ld und %ld\n",
			       img->w, img->h, img->stride, img->dst_x,
			       img->dst_y, img->color, change, now,
			       undisplay);

		    /* api docu said w and h can be zero which
		     * means image should not be rendered
		     */
		    if ((img->w != 0) && (img->h != 0) && writer) {
			out.data = img->bitmap;
			out.Width = img->w;
			out.Height = img->h;
			out.Stride = img->stride;
			out.x = img->dst_x;
			out.y = img->dst_y;
			out.color = img->color;

			out.Screen_Width = screen_width;
			out.Screen_Height = screen_height;
			out.destination = destination;
			out.destStride = destStride;

			if (context && context->playback
			    && context->playback->isPlaying && writer)
			    writer->writeData(&out);

			needsBlit = 1;
		    }

		    /* Next image */
		    img = img->next;
		}
	    }
	    releaseMutex(__LINE__);
	} else {
	    usleep(1000);
	}

	if (needsBlit && framebufferBlit)
	    framebufferBlit();
	needsBlit = 0;

	/* cleanup no longer used but not overwritten regions */
	getMutex(__LINE__);
	checkRegions();
	releaseMutex(__LINE__);
    }				/* while */

    if (needsBlit && framebufferBlit)
	framebufferBlit();
    needsBlit = 0;

    hasPlayThreadStarted = 0;

    ass_printf(10, "terminating\n");
    pthread_exit(NULL);
}

/* **************************** */
/* Container part for ass       */
/* **************************** */

int container_ass_init(Context_t * context)
{
    SubtitleOutputDef_t output;

    ass_printf(10, ">\n");

    ass_library = ass_library_init();

    if (!ass_library) {
	ass_err("ass_library_init failed!\n");
	return cERR_CONTAINER_ASS_ERROR;
    }

    if (debug_level >= 100)
	ass_set_message_cb(ass_library, ass_msg_callback, NULL);

    ass_set_extract_fonts(ass_library, 1);
    ass_set_style_overrides(ass_library, NULL);

    ass_renderer = ass_renderer_init(ass_library);

    if (!ass_renderer) {
	ass_err("ass_renderer_init failed!\n");

	if (ass_library)
	    ass_library_done(ass_library);
	ass_library = NULL;

	return cERR_CONTAINER_ASS_ERROR;
    }

    context->output->subtitle->Command(context, OUTPUT_GET_SUBTITLE_OUTPUT,
				       &output);

    screen_width = output.screen_width;
    screen_height = output.screen_height;
    destination = output.destination;
    destStride = output.destStride;
    framebufferBlit = output.framebufferBlit;

    ass_printf(10, "width %d, height %d\n", screen_width, screen_height);

    ass_set_frame_size(ass_renderer, screen_width, screen_height);
    ass_set_margins(ass_renderer, (int) (0.03 * screen_height),
		    (int) (0.03 * screen_height),
		    (int) (0.03 * screen_width),
		    (int) (0.03 * screen_width));

    ass_set_use_margins(ass_renderer, 1);
//    ass_set_font_scale(ass_renderer, (ass_font_scale * screen_height) / 240.0);

    ass_set_hinting(ass_renderer, ASS_HINTING_LIGHT);
//    ass_set_line_spacing(ass_renderer, (ass_line_spacing * screen_height) / 240.0);
    ass_set_fonts(ass_renderer, ASS_FONT, "Arial", 0, NULL, 1);

    ass_set_aspect_ratio(ass_renderer, 1.0, 1.0);

    isContainerRunning = 1;

    return cERR_CONTAINER_ASS_NO_ERROR;
}

int container_ass_process_data(Context_t * context
			       __attribute__ ((unused)),
			       SubtitleData_t * data)
{
    int first_kiss;

    ass_printf(20, ">\n");

    if (!isContainerRunning) {
	ass_err("Container not running\n");
	return cERR_CONTAINER_ASS_ERROR;
    }

    if (ass_track == NULL) {
	first_kiss = 1;
	ass_track = ass_new_track(ass_library);

	if (ass_track == NULL) {
	    ass_err("error creating ass_track\n");
	    return cERR_CONTAINER_ASS_ERROR;
	}
	ass_track->PlayResX = screen_width;
	ass_track->PlayResY = screen_height;
    }

    if ((data->extradata) && (first_kiss)) {
	ass_printf(30, "processing private %d bytes\n", data->extralen);
	ass_process_codec_private(ass_track, (char *) data->extradata,
				  data->extralen);
	ass_printf(30, "processing private done\n");
    }

    if (data->data) {
	ass_printf(30, "processing data %d bytes\n", data->len);
	ass_process_data(ass_track, (char *) data->data, data->len);
	ass_printf(30, "processing data done\n");
    }

    return cERR_CONTAINER_ASS_NO_ERROR;
}

static int container_ass_stop(Context_t * context __attribute__ ((unused)))
{
    int ret = cERR_CONTAINER_ASS_NO_ERROR;
    int wait_time = 20;
    Writer_t *writer;

    ass_printf(10, "\n");

    if (!isContainerRunning) {
	ass_err("Container not running\n");
	return cERR_CONTAINER_ASS_ERROR;
    }

    while ((hasPlayThreadStarted != 0) && (--wait_time) > 0) {
	ass_printf(10,
		   "Waiting for ass thread to terminate itself, will try another %d times\n",
		   wait_time);

	usleep(100000);
    }

    if (wait_time == 0) {
	ass_err("Timeout waiting for thread!\n");

	ret = cERR_CONTAINER_ASS_ERROR;
    }

    getMutex(__LINE__);

    releaseRegions();

    if (ass_track)
	ass_free_track(ass_track);

    ass_track = NULL;

    if (ass_renderer)
	ass_renderer_done(ass_renderer);
    ass_renderer = NULL;

    if (ass_library)
	ass_library_done(ass_library);
    ass_library = NULL;

    isContainerRunning = 0;

    hasPlayThreadStarted = 0;

    writer = getDefaultFramebufferWriter();

    if (writer) {
	writer->reset();
    }

    releaseMutex(__LINE__);

    ass_printf(10, "ret %d\n", ret);
    return ret;
}

static int container_ass_switch_subtitle(Context_t * context, int *arg
					 __attribute__ ((unused)))
{
    int error;
    int ret = cERR_CONTAINER_ASS_NO_ERROR;
    pthread_attr_t attr;

    ass_printf(10, "\n");

    if (!isContainerRunning) {
	ass_err("Container not running\n");
	return cERR_CONTAINER_ASS_ERROR;
    }

    if (context && context->playback && context->playback->isPlaying) {
	ass_printf(10, "is Playing\n");
    } else {
	ass_printf(10, "is NOT Playing\n");
    }

    if (hasPlayThreadStarted == 0) {
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if ((error =
	     pthread_create(&PlayThread, &attr, (void *) &ASSThread,
			    context)) != 0) {
	    ass_printf(10, "Error creating thread, error:%d:%s\n", error,
		       strerror(error));

	    hasPlayThreadStarted = 0;
	    ret = cERR_CONTAINER_ASS_ERROR;
	} else {
	    ass_printf(10, "Created thread\n");

	    hasPlayThreadStarted = 1;
	}
    } else {
	ass_printf(10, "A thread already exists!\n");

	ret = cERR_CONTAINER_ASS_ERROR;
    }

    getMutex(__LINE__);

    releaseRegions();

    /* free the track so extradata will be written next time
     * process_data is called.
     */
    if (ass_track)
	ass_free_track(ass_track);

    ass_track = NULL;

    releaseMutex(__LINE__);

    ass_printf(10, "exiting with value %d\n", ret);

    return ret;
}


static int Command(void *_context, ContainerCmd_t command, void *argument)
{
    Context_t *context = (Context_t *) _context;
    int ret = cERR_CONTAINER_ASS_NO_ERROR;

    ass_printf(50, "Command %d\n", command);

    switch (command) {
    case CONTAINER_INIT:{
	    ret = container_ass_init(context);
	    break;
	}
    case CONTAINER_STOP:{
	    ret = container_ass_stop(context);
	    break;
	}
    case CONTAINER_SWITCH_SUBTITLE:{
	    ret = container_ass_switch_subtitle(context, (int *) argument);
	    break;
	}
    case CONTAINER_DATA:{
	    SubtitleData_t *data = (SubtitleData_t *) argument;
	    ret = container_ass_process_data(context, data);
	    break;
	}
    default:
	ass_err("ContainerCmd %d not supported!\n", command);
	ret = cERR_CONTAINER_ASS_ERROR;
	break;
    }

    ass_printf(50, "exiting with value %d\n", ret);

    return ret;
}

static char *ASS_Capabilities[] = { "ass", NULL };

Container_t ASSContainer = {
    "ASS",
    &Command,
    ASS_Capabilities
};

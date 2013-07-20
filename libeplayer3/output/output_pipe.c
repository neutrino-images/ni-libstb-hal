/*
 * Pipe Output handling.
 *
 * 2012 by martii
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <memory.h>
#include <asm/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/uio.h>
#include <errno.h>

#include "common.h"
#include "output.h"
#include "writer.h"
#include "misc.h"
#include "pes.h"

/* ***************************** */
/* Makros/Constants              */
/* ***************************** */

#define PIPE_DEBUG

static short debug_level = 0;

static const char FILENAME[] = __FILE__;

#ifdef PIPE_DEBUG
#define pipe_printf(level, fmt, x...) do { \
if (debug_level >= level) printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x ); } while (0)
#else
#define pipe_printf(x...)
#endif

#ifndef PIPE_SILENT
#define pipe_err(fmt, x...) do { printf("[%s:%s] " fmt, __FILE__, __FUNCTION__, ## x); } while (0)
#else
#define pipe_err(x...)
#endif

#define cERR_PIPE_NO_ERROR      0
#define cERR_PIPE_ERROR        -1

static const char TELETEXTPIPE[] 	= "/tmp/.eplayer3_teletext";
static const char DVBSUBTITLEPIPE[] 	= "/tmp/.eplayer3_dvbsubtitle";

static int teletextfd 		= -1;
static int dvbsubtitlefd 	= -1;

pthread_mutex_t Pipemutex;

/* ***************************** */
/* Prototypes                    */
/* ***************************** */
int PipeStop(Context_t  *context, char * type);

/* ***************************** */
/* MISC Functions                */
/* ***************************** */

void getPipeMutex(const char *filename __attribute__((unused)), const char *function __attribute__((unused)), int line __attribute__((unused))) {

    pipe_printf(250, "requesting mutex\n");

    pthread_mutex_lock(&Pipemutex);

    pipe_printf(250, "received mutex\n");
}

void releasePipeMutex(const char *filename __attribute__((unused)), const char *function __attribute__((unused)), int line __attribute__((unused))) {
    pthread_mutex_unlock(&Pipemutex);

    pipe_printf(250, "released mutex\n");

}

int PipeOpen(Context_t  *context __attribute__((unused)), char * type) {
    unsigned char teletext = !strcmp("teletext", type);
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);

    pipe_printf(10, "t%d d%d\n", teletext, dvbsubtitle);

    if (teletext && teletextfd == -1) {
        mkfifo(TELETEXTPIPE, 0644);
        teletextfd = open(TELETEXTPIPE, O_RDWR | O_NONBLOCK);

        if (teletextfd < 0)
        {
            pipe_err("failed to open %s - errno %d\n", TELETEXTPIPE, errno);
            pipe_err("%s\n", strerror(errno));
            return cERR_PIPE_ERROR;
        }
    }
    if (dvbsubtitle && dvbsubtitlefd == -1) {
        mkfifo(DVBSUBTITLEPIPE, 0644);
        dvbsubtitlefd = open(DVBSUBTITLEPIPE, O_RDWR | O_NONBLOCK);

        if (dvbsubtitlefd < 0)
        {
            pipe_err("failed to open %s - errno %d\n", DVBSUBTITLEPIPE, errno);
            pipe_err("%s\n", strerror(errno));
            return cERR_PIPE_ERROR;
        }
    }

    return cERR_PIPE_NO_ERROR;
}

int PipeClose(Context_t  *context, char * type) {
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "t%d d%d\n", teletext, dvbsubtitle);

    /* closing stand alone is not allowed, so prevent
     * user from closing and dont call stop. stop will
     * set default values for us (speed and so on).
     */
    PipeStop(context, type);

    getPipeMutex(FILENAME, __FUNCTION__,__LINE__);

    if (dvbsubtitle && dvbsubtitlefd != -1) {
        close(dvbsubtitlefd);
        dvbsubtitlefd = -1;
    }
    if (teletext && teletextfd != -1) {
        close(teletextfd);
        teletextfd = -1;
    }

    releasePipeMutex(FILENAME, __FUNCTION__,__LINE__);
    return cERR_PIPE_NO_ERROR;
}

int PipePlay(Context_t  *context __attribute__((unused)), char * type __attribute__((unused))) {
    int ret = cERR_PIPE_NO_ERROR;

#if 0
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "t%d d%d\n", teletext, dvbsubtitle);

    if (dvbsubtitle && dvbsubtitlefd != -1) {
    }
    if (teletext && teletextfd != -1) {
    }
#endif
    return ret;
}

int PipeStop(Context_t  *context __attribute__((unused)), char * type __attribute__((unused))) {
    int ret = cERR_PIPE_NO_ERROR;

#if 0
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "t%d d%d\n", teletext, dvbsubtitle);

    getPipeMutex(FILENAME, __FUNCTION__,__LINE__);

    if (dvbsubtitle && dvbsubtitlefd != -1) {
    }
    if (teletext && teletextfd != -1) {
    }

    releasePipeMutex(FILENAME, __FUNCTION__,__LINE__);
#endif

    return ret;
}

int PipeFlush(Context_t  *context __attribute__((unused)), char * type) {
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "t%d d%d\n", teletext, dvbsubtitle);

    if ( (dvbsubtitle && dvbsubtitlefd != -1) || (teletext && teletextfd != -1) ) {
        getPipeMutex(FILENAME, __FUNCTION__,__LINE__);

	if (dvbsubtitle && dvbsubtitlefd != -1) {
	    char buf[65536];
	    while(0 < read(dvbsubtitlefd, buf, sizeof(buf)));
	}
	if (teletext && teletextfd != -1) {
	    char buf[65536];
	    while(0 < read(teletextfd, buf, sizeof(buf)));
	}

        releasePipeMutex(FILENAME, __FUNCTION__,__LINE__);
    }

    pipe_printf(10, "exiting\n");

    return cERR_PIPE_NO_ERROR;
}

int PipeClear(Context_t  *context __attribute__((unused)), char * type) {
    int ret = cERR_PIPE_NO_ERROR;
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "v%d a%d\n", dvbsubtitle, teletext);

    if ( (dvbsubtitle && dvbsubtitlefd != -1) || (teletext && teletextfd != -1) ) {
        getPipeMutex(FILENAME, __FUNCTION__,__LINE__);

	if (dvbsubtitle && dvbsubtitlefd != -1) {
	    char buf[65536];
	    while(0 < read(dvbsubtitlefd, buf, sizeof(buf)));
	}
	if (teletext && teletextfd != -1) {
	    char buf[65536];
	    while(0 < read(teletextfd, buf, sizeof(buf)));
	}

        releasePipeMutex(FILENAME, __FUNCTION__,__LINE__);
    }

    pipe_printf(10, "exiting\n");

    return ret;
}

int PipeSwitch(Context_t  *context __attribute__((unused)), char * type __attribute__((unused))) {
#if 0
    unsigned char dvbsubtitle = !strcmp("dvbsubtitle", type);
    unsigned char teletext = !strcmp("teletext", type);

    pipe_printf(10, "v%d a%d\n", dvbsubtitle, teletext);

    if ( (dvbsubtitle && dvbsubtitlefd != -1) || (teletext && teletextfd != -1) ) {
        getPipeMutex(FILENAME, __FUNCTION__,__LINE__);

        if (teletext && teletextfd != -1) {
        }

        if (dvbsubtitle && dvbsubtitlefd != -1) {
        }

        releasePipeMutex(FILENAME, __FUNCTION__,__LINE__);

    }

    pipe_printf(10, "exiting\n");
#endif
    return cERR_PIPE_NO_ERROR;
}

static int writePESDataTeletext(int fd, unsigned char *data, size_t data_len)
{
    unsigned int len = 0;
    if (data_len > 0) {
    	len = data_len + 39;
	char header[45];
	memset(header, 0, sizeof(header));
	header[2] = 0x01;
	header[3] = 0xbd;
	header[4] = (len >> 8) & 0xff;
	header[5] = len & 0xff;
	struct iovec iov[2];
	iov[0].iov_base = header;
	iov[0].iov_len = 45;
	iov[1].iov_base = data;
	iov[1].iov_len = data_len;
	len = writev(fd, iov, 2);
	if (len != iov[0].iov_len + iov[1].iov_len) {
	    // writing to pipe failed, clear it.
	    char buf[65536];
	    while(0 < read(fd, buf, sizeof(buf)));
	}
    }
    return len;
}

static int writePESDataDvbsubtitle(int fd, unsigned char *data, size_t data_len, int64_t pts)
{
    int len = 0;
    if (data_len > 0) {
    	len = data_len + 10;
	char header[16];
	memset(header, 0, sizeof(header));
	header[2] = 0x01;
	header[3] = 0xbd;
	header[4] = (len >> 8) & 0xff;
	header[5] = len & 0xff;

	if (pts) {
		header[7] = 0x80;
		header[13] = 0x01 | ((pts << 1) & 0xff);
		pts >>= 7;
		header[12] = pts & 0xff;
		pts >>=8;
		header[11] = 0x01 | ((pts << 1) & 0xff);
		pts >>= 7;
		header[10] = pts & 0xff;
		pts >>=8;
		header[9] = 0x21 | ((pts << 1) & 0xff);
	}
	header[8] = 14 - 9;
	header[14] = 0x20;
	struct iovec iov[2];
	iov[0].iov_base = header;
	iov[0].iov_len = 16;
	iov[1].iov_base = data;
	iov[1].iov_len = data_len;
	len = writev(fd, iov, 2);
	if (len != (int)(iov[0].iov_len + iov[1].iov_len)) {
	    // writing to pipe failed, clear it.
	    char buf[65536];
	    while(0 < read(fd, buf, sizeof(buf)));
	}
    }
    return len;
}

static int Write(void  *_context __attribute__((unused)), void* _out)
{
    AudioVideoOut_t    *out      = (AudioVideoOut_t*) _out;
    int                ret       = cERR_PIPE_NO_ERROR;
    int                res       = 0;
    unsigned char      dvbsubtitle;
    unsigned char      teletext;

    if (out == NULL)
    {
       pipe_err("null pointer passed\n");
       return cERR_PIPE_ERROR;
    }
    
    dvbsubtitle = !strcmp("dvbsubtitle", out->type);
    teletext = !strcmp("teletext", out->type);
  
    pipe_printf(20, "DataLength=%u PrivateLength=%u Pts=%llu FrameRate=%f\n", 
                                                    out->len, out->extralen, out->pts, out->frameRate);
    pipe_printf(20, "v%d a%d\n", dvbsubtitle, teletext);

    if (dvbsubtitle) {
	res = writePESDataDvbsubtitle(dvbsubtitlefd, out->data, out->len, out->pts);

        if (res <= 0)
        {
            ret = cERR_PIPE_ERROR;
        }
    } else if (teletext) {
	res = writePESDataTeletext(teletextfd, out->data, out->len);

        if (res <= 0)
        {
            ret = cERR_PIPE_ERROR;
        }
    }

    return ret;
}

static int reset(Context_t  *context __attribute__((unused)))
{
    int ret = cERR_PIPE_NO_ERROR;

    return ret;
}

static int Command(void  *_context, OutputCmd_t command, void * argument) {
    Context_t* context = (Context_t*) _context;
    int ret = cERR_PIPE_NO_ERROR;
    
    pipe_printf(50, "Command %d\n", command);

    switch(command) {
    case OUTPUT_OPEN: {
        ret = PipeOpen(context, (char*)argument);
        break;
    }
    case OUTPUT_CLOSE: {
        ret = PipeClose(context, (char*)argument);
        reset(context);
        break;
    }
    case OUTPUT_PLAY: {	// 4
        ret = PipePlay(context, (char*)argument);
        break;
    }
    case OUTPUT_STOP: {
        reset(context);
        ret = PipeStop(context, (char*)argument);
        break;
    }
    case OUTPUT_FLUSH: {
        ret = PipeFlush(context, (char*)argument);
        reset(context);
        break;
    }
    case OUTPUT_CLEAR: {
        ret = PipeClear(context, (char*)argument);
        break;
    }
    case OUTPUT_SWITCH: {
        ret = PipeSwitch(context, (char*)argument);
        break;
    }
    default:
        pipe_err("ContainerCmd %d not supported!\n", command);
        ret = cERR_PIPE_ERROR;
        break;
    }

    pipe_printf(50, "exiting with value %d\n", ret);

    return ret;
}

static char *PipeCapabilities[] = { "teletext", "dvbsubtitle", NULL };

struct Output_s PipeOutput = {
    "DVBSubTitle",
    &Command,
    &Write,
    PipeCapabilities
};

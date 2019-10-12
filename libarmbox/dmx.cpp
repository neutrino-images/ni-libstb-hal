/*
 * cDemux implementation for arm receivers (tested on mutant hd51
 * hardware)
 *
 * derived from libtriple/dmx_td.cpp
 *
 * (C) 2010-2013 Stefan Seyfried
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#include <cstring>
#include <cstdio>
#include <string>
#include <sys/ioctl.h>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include "dmx_hal.h"
#include "hal_debug.h"

#include "video_lib.h"
/* needed for getSTC... */
extern cVideo *videoDecoder;

#define hal_debug(args...) _hal_debug(HAL_DEBUG_DEMUX, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_DEMUX, this, args)
#define hal_debug_c(args...) _hal_debug(HAL_DEBUG_DEMUX, NULL, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_DEMUX, NULL, args)
#define hal_info_z(args...) _hal_info(HAL_DEBUG_DEMUX, thiz, args)
#define hal_debug_z(args...) _hal_debug(HAL_DEBUG_DEMUX, thiz, args)

#define dmx_err(_errfmt, _errstr, _revents) do { \
	hal_info("%s " _errfmt " fd:%d, ev:0x%x %s pid:0x%04hx flt:0x%02hx\n", \
		__func__, _errstr, fd, _revents, DMX_T[dmx_type], pid, flt); \
} while(0);

cDemux *videoDemux = NULL;
cDemux *audioDemux = NULL;
//cDemux *pcrDemux = NULL;

static const char *DMX_T[] = {
	"DMX_INVALID",
	"DMX_VIDEO",
	"DMX_AUDIO",
	"DMX_PES",
	"DMX_PSI",
	"DMX_PIP",
	"DMX_TP",
	"DMX_PCR"
};

/* this is the number of different cDemux() units, not the number of
 * /dev/dvb/.../demuxX devices! */
#if BOXMODEL_VUULTIMO4K
#define NUM_DEMUX 24
#else
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUUNO4K
#define NUM_DEMUX 16
#else
#define NUM_DEMUX 4
#endif
#endif
/* the current source of each cDemux unit */
#if BOXMODEL_VUULTIMO4K
static int dmx_source[NUM_DEMUX] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#else
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUUNO4K
static int dmx_source[NUM_DEMUX] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#else
static int dmx_source[NUM_DEMUX] = { 0, 0, 0, 0 };
#endif
#endif

/* map the device numbers. */
#if BOXMODEL_VUULTIMO4K
#define NUM_DEMUXDEV 24
#else
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUUNO4K
#define NUM_DEMUXDEV 16
#else
#define NUM_DEMUXDEV 8
#endif
#endif
static const char *devname[NUM_DEMUXDEV] = {
	"/dev/dvb/adapter0/demux0",
	"/dev/dvb/adapter0/demux1",
	"/dev/dvb/adapter0/demux2",
	"/dev/dvb/adapter0/demux3",
	"/dev/dvb/adapter0/demux4",
	"/dev/dvb/adapter0/demux5",
	"/dev/dvb/adapter0/demux6",
	"/dev/dvb/adapter0/demux7"
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUUNO4K
	, "/dev/dvb/adapter0/demux8"
	, "/dev/dvb/adapter0/demux9"
	, "/dev/dvb/adapter0/demux10"
	, "/dev/dvb/adapter0/demux11"
	, "/dev/dvb/adapter0/demux12"
	, "/dev/dvb/adapter0/demux13"
	, "/dev/dvb/adapter0/demux14"
	, "/dev/dvb/adapter0/demux15"
#if BOXMODEL_VUULTIMO4K
	, "/dev/dvb/adapter0/demux16"
	, "/dev/dvb/adapter0/demux17"
	, "/dev/dvb/adapter0/demux18"
	, "/dev/dvb/adapter0/demux19"
	, "/dev/dvb/adapter0/demux20"
	, "/dev/dvb/adapter0/demux21"
	, "/dev/dvb/adapter0/demux22"
	, "/dev/dvb/adapter0/demux23"
#endif
#endif
};
/* did we already DMX_SET_SOURCE on that demux device? */
#if BOXMODEL_VUULTIMO4K
static bool init[NUM_DEMUXDEV] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
#else
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUUNO4K
static bool init[NUM_DEMUXDEV] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
#else
static bool init[NUM_DEMUXDEV] = { false, false, false, false, false, false, false, false };
#endif
#endif

typedef struct dmx_pdata {
	int last_source;
	OpenThreads::Mutex *mutex;
} dmx_pdata;
#define P ((dmx_pdata *)pdata)

cDemux::cDemux(int n)
{
	if (n < 0 || n >= NUM_DEMUX)
	{
		hal_info("%s ERROR: n invalid (%d)\n", __FUNCTION__, n);
		num = 0;
	}
	else
		num = n;
	fd = -1;
	pdata = (void *)calloc(1, sizeof(dmx_pdata));
	P->last_source = -1;
	P->mutex = new OpenThreads::Mutex;
	dmx_type = DMX_INVALID;
}

cDemux::~cDemux()
{
	hal_debug("%s #%d fd: %d\n", __FUNCTION__, num, fd);
	Close();
	/* wait until Read() has released the mutex */
	(*P->mutex).lock();
	(*P->mutex).unlock();
	free(P->mutex);
	free(pdata);
	pdata = NULL;
}

bool cDemux::Open(DMX_CHANNEL_TYPE pes_type, void * /*hVideoBuffer*/, int uBufferSize)
{
	if (fd > -1)
		hal_info("%s FD ALREADY OPENED? fd = %d\n", __FUNCTION__, fd);

	dmx_type = pes_type;
	buffersize = uBufferSize;

	/* return code is unchecked anyway... */
	return true;
}

static bool _open(cDemux *thiz, int num, int &fd, int &last_source, DMX_CHANNEL_TYPE dmx_type, int buffersize)
{
	int flags = O_RDWR|O_CLOEXEC;
	int devnum = dmx_source[num];
	if (last_source == devnum) {
		hal_debug_z("%s #%d: source (%d) did not change\n", __func__, num, last_source);
		if (fd > -1)
			return true;
	}
	if (fd > -1) {
		/* we changed source -> close and reopen the fd */
		hal_debug_z("%s #%d: FD ALREADY OPENED fd = %d lastsource %d devnum %d\n",
				__func__, num, fd, last_source, devnum);
		close(fd);
	}

	if (dmx_type != DMX_PSI_CHANNEL)
		flags |= O_NONBLOCK;

	fd = open(devname[devnum], flags);
	if (fd < 0)
	{
		hal_info_z("%s %s: %m\n", __FUNCTION__, devname[devnum]);
		return false;
	}
	hal_debug_z("%s #%d pes_type: %s(%d), uBufferSize: %d fd: %d\n", __func__,
		 num, DMX_T[dmx_type], dmx_type, buffersize, fd);

	/* this would actually need locking, but the worst that weill happen is, that
	 * we'll DMX_SET_SOURCE twice per device, so don't bother... */
	if (!init[devnum])
	{
		/* this should not change anything... */
		int n = DMX_SOURCE_FRONT0 + devnum;
		hal_info_z("%s: setting %s to source %d\n", __func__, devname[devnum], n);
		if (ioctl(fd, DMX_SET_SOURCE, &n) < 0)
			hal_info_z("%s DMX_SET_SOURCE failed!\n", __func__);
		else
			init[devnum] = true;
	}
	if (buffersize == 0)
		buffersize = 0xffff; // may or may not be reasonable  --martii
	if (buffersize > 0)
	{
		/* probably uBufferSize == 0 means "use default size". TODO: find a reasonable default */
		if (ioctl(fd, DMX_SET_BUFFER_SIZE, buffersize) < 0)
			hal_info_z("%s DMX_SET_BUFFER_SIZE failed (%m)\n", __func__);
	}

	last_source = devnum;
	return true;
}

void cDemux::Close(void)
{
	hal_debug("%s #%d, fd = %d\n", __FUNCTION__, num, fd);
	if (fd < 0)
	{
		hal_info("%s #%d: not open!\n", __FUNCTION__, num);
		return;
	}

	pesfds.clear();
	ioctl(fd, DMX_STOP);
	close(fd);
	fd = -1;
}

bool cDemux::Start(bool)
{
	hal_debug("%s #%d fd: %d type: %s\n", __func__, num, fd, DMX_T[dmx_type]);
	if (fd < 0)
	{
		hal_info("%s #%d: not open!\n", __FUNCTION__, num);
		return false;
	}
	ioctl(fd, DMX_START);
	return true;
}

bool cDemux::Stop(void)
{
	hal_debug("%s #%d fd: %d type: %s\n", __func__, num, fd, DMX_T[dmx_type]);
	if (fd < 0)
	{
		hal_info("%s #%d: not open!\n", __FUNCTION__, num);
		return false;
	}
	ioctl(fd, DMX_STOP);
	return true;
}

int cDemux::Read(unsigned char *buff, int len, int timeout)
{
#if 0
	if (len != 4095 && timeout != 10)
		fprintf(stderr, "cDemux::%s #%d fd: %d type: %s len: %d timeout: %d\n",
			__FUNCTION__, num, fd, DMX_T[dmx_type], len, timeout);
#endif
	if (fd < 0)
	{
		hal_info("%s #%d: not open!\n", __func__, num);
		return -1;
	}
	/* avoid race in destructor: ~cDemux needs to wait until Read() returns */
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(*P->mutex);
	int rc;
	int to = timeout;
	struct pollfd ufds;
	ufds.fd = fd;
	ufds.events = POLLIN|POLLPRI|POLLERR;
	ufds.revents = 0;

	/* hack: if the frontend loses and regains lock, the demuxer often will not
	 * return from read(), so as a "emergency exit" for e.g. NIT scan, set a (long)
	 * timeout here */
	if (dmx_type == DMX_PSI_CHANNEL && timeout <= 0)
		to = 60 * 1000;

	if (to > 0)
	{
 retry:
		rc = ::poll(&ufds, 1, to);
		if (ufds.fd != fd)
		{
			/* Close() will set fd to -1, this is normal. Everything else is not. */
			hal_info("%s:1 ========== fd has changed, %d->%d ==========\n", __func__, ufds.fd, fd);
			return -1;
		}
		if (!rc)
		{
			if (timeout == 0) /* we took the emergency exit */
			{
				dmx_err("timed out for timeout=0!, %s", "", 0);
				return -1; /* this timeout is an error */
			}
			return 0; // timeout
		}
		else if (rc < 0)
		{
			dmx_err("poll: %s,", strerror(errno), 0)
			//hal_info("%s poll: %m\n", __FUNCTION__);
			/* happens, when running under gdb... */
			if (errno == EINTR)
				goto retry;
			return -1;
		}
#if 0
		if (ufds.revents & POLLERR) /* POLLERR means buffer error, i.e. buffer overflow */
		{
			dmx_err("received %s,", "POLLERR", ufds.revents);
			/* this seems to happen sometimes at recording start, without bad effects */
			return 0;
		}
#endif
		if (ufds.revents & POLLHUP) /* we get POLLHUP if e.g. a too big DMX_BUFFER_SIZE was set */
		{
			dmx_err("received %s,", "POLLHUP", ufds.revents);
			return -1;
		}
		if (!(ufds.revents & POLLIN)) /* we requested POLLIN but did not get it? */
		{
			dmx_err("received %s, please report!", "POLLIN", ufds.revents);
			return 0;
		}
	}
	if (ufds.fd != fd)	/* does this ever happen? and if, is it harmful? */
	{			/* read(-1,...) will just return EBADF anyway... */
		hal_info("%s:2 ========== fd has changed, %d->%d ==========\n", __func__, ufds.fd, fd);
		return -1;
	}

	rc = ::read(fd, buff, len);
	//fprintf(stderr, "fd %d ret: %d\n", fd, rc);
	if (rc < 0)
		dmx_err("read: %s", strerror(errno), 0);

	return rc;
}

bool cDemux::sectionFilter(unsigned short _pid, const unsigned char * const filter,
			   const unsigned char * const mask, int len, int timeout,
			   const unsigned char * const negmask)
{
	struct dmx_sct_filter_params s_flt;
	memset(&s_flt, 0, sizeof(s_flt));
	pid = _pid;

	_open(this, num, fd, P->last_source, dmx_type, buffersize);

	if (len > DMX_FILTER_SIZE)
	{
		hal_info("%s #%d: len too long: %d, DMX_FILTER_SIZE %d\n", __func__, num, len, DMX_FILTER_SIZE);
		len = DMX_FILTER_SIZE;
	}
	flt = filter[0];
	s_flt.pid = pid;
	s_flt.timeout = timeout;
	memcpy(s_flt.filter.filter, filter, len);
	memcpy(s_flt.filter.mask,   mask,   len);
	if (negmask != NULL)
		memcpy(s_flt.filter.mode, negmask, len);

	s_flt.flags = DMX_IMMEDIATE_START|DMX_CHECK_CRC;

	int to = 0;
	switch (filter[0])
	{
		case 0x00: /* program_association_section */
			to = 2000;
			break;
		case 0x01: /* conditional_access_section */
			to = 6000;
			break;
		case 0x02: /* program_map_section */
			to = 1500;
			break;
		case 0x03: /* transport_stream_description_section */
			to = 10000;
			break;
		/* 0x04 - 0x3F: reserved */
		case 0x40: /* network_information_section - actual_network */
			to = 10000;
			break;
		case 0x41: /* network_information_section - other_network */
			to = 15000;
			break;
		case 0x42: /* service_description_section - actual_transport_stream */
			to = 10000;
			break;
		/* 0x43 - 0x45: reserved for future use */
		case 0x46: /* service_description_section - other_transport_stream */
			to = 10000;
			break;
		/* 0x47 - 0x49: reserved for future use */
		case 0x4A: /* bouquet_association_section */
			to = 11000;
			break;
		/* 0x4B - 0x4D: reserved for future use */
		case 0x4E: /* event_information_section - actual_transport_stream, present/following */
			to = 2000;
			break;
		case 0x4F: /* event_information_section - other_transport_stream, present/following */
			to = 10000;
			break;
		/* 0x50 - 0x5F: event_information_section - actual_transport_stream, schedule */
		/* 0x60 - 0x6F: event_information_section - other_transport_stream, schedule */
		case 0x70: /* time_date_section */
			s_flt.flags &= ~DMX_CHECK_CRC; /* section has no CRC */
			s_flt.flags |= DMX_ONESHOT;
			//s_flt.pid     = 0x0014;
			to = 30000;
			break;
		case 0x71: /* running_status_section */
			s_flt.flags &= ~DMX_CHECK_CRC; /* section has no CRC */
			to = 0;
			break;
		case 0x72: /* stuffing_section */
			s_flt.flags &= ~DMX_CHECK_CRC; /* section has no CRC */
			to = 0;
			break;
		case 0x73: /* time_offset_section */
			s_flt.flags |= DMX_ONESHOT;
			//s_flt.pid     = 0x0014;
			to = 30000;
			break;
		/* 0x74 - 0x7D: reserved for future use */
		case 0x7E: /* discontinuity_information_section */
			s_flt.flags &= ~DMX_CHECK_CRC; /* section has no CRC */
			to = 0;
			break;
		case 0x7F: /* selection_information_section */
			to = 0;
			break;
		/* 0x80 - 0x8F: ca_message_section */
		/* 0x90 - 0xFE: user defined */
		/*        0xFF: reserved */
		default:
			//return -1;
			break;
	}
	/* the negmask == NULL is a hack: the users of negmask are PMT-update
	 * and sectionsd EIT-Version change. And they really want no timeout
	 * if timeout == 0 instead of "default timeout" */
	if (timeout == 0 && negmask == NULL)
		s_flt.timeout = to;

	hal_debug("%s #%d pid:0x%04hx fd:%d type:%s len:%d to:%d flags:%x flt[0]:%02x\n", __func__, num,
		pid, fd, DMX_T[dmx_type], len, s_flt.timeout,s_flt.flags, s_flt.filter.filter[0]);
#if 0
	fprintf(stderr,"filt: ");for(int i=0;i<FILTER_LENGTH;i++)fprintf(stderr,"%02hhx ",s_flt.filter[i]);fprintf(stderr,"\n");
	fprintf(stderr,"mask: ");for(int i=0;i<FILTER_LENGTH;i++)fprintf(stderr,"%02hhx ",s_flt.mask  [i]);fprintf(stderr,"\n");
	fprintf(stderr,"posi: ");for(int i=0;i<FILTER_LENGTH;i++)fprintf(stderr,"%02hhx ",s_flt.positive[i]);fprintf(stderr,"\n");
#endif
	ioctl (fd, DMX_STOP);
	if (ioctl(fd, DMX_SET_FILTER, &s_flt) < 0)
		return false;

	return true;
}

bool cDemux::pesFilter(const unsigned short _pid)
{
	struct dmx_pes_filter_params p_flt;
	pid = _pid;
	flt = 0;
	/* allow PID 0 for web streaming e.g.
	 * this check originally is from tuxbox cvs but I'm not sure
	 * what it is good for...
	if (pid <= 0x0001 && dmx_type != DMX_PCR_ONLY_CHANNEL)
		return false;
	 */
	if ((pid >= 0x0002 && pid <= 0x000f) || pid >= 0x1fff)
		return false;

	hal_debug("%s #%d pid: 0x%04hx fd: %d type: %s\n", __FUNCTION__, num, pid, fd, DMX_T[dmx_type]);

	_open(this, num, fd, P->last_source, dmx_type, buffersize);

	memset(&p_flt, 0, sizeof(p_flt));
	p_flt.pid    = pid;
	p_flt.output = DMX_OUT_DECODER;
	p_flt.input  = DMX_IN_FRONTEND;
	p_flt.flags  = 0;

	switch (dmx_type)
	{
		case DMX_PCR_ONLY_CHANNEL:
			p_flt.pes_type = DMX_PES_PCR;
			break;
		case DMX_AUDIO_CHANNEL:
			p_flt.pes_type = DMX_PES_AUDIO;
			break;
		case DMX_VIDEO_CHANNEL:
			p_flt.pes_type = DMX_PES_VIDEO;
			break;
		case DMX_PIP_CHANNEL: /* PIP is a special version of DMX_VIDEO_CHANNEL */
			p_flt.pes_type = DMX_PES_VIDEO1;
			break;
		case DMX_PES_CHANNEL:
			p_flt.pes_type = DMX_PES_OTHER;
			p_flt.output  = DMX_OUT_TAP;
			break;
		case DMX_TP_CHANNEL:
			p_flt.pes_type = DMX_PES_OTHER;
			p_flt.output  = DMX_OUT_TSDEMUX_TAP;
			break;
		default:
			hal_info("%s #%d invalid dmx_type %d!\n", __func__, num, dmx_type);
			return false;
	}
	return (ioctl(fd, DMX_SET_PES_FILTER, &p_flt) >= 0);
}

void cDemux::SetSyncMode(AVSYNC_TYPE /*mode*/)
{
	hal_debug("%s #%d\n", __FUNCTION__, num);
}

void *cDemux::getBuffer()
{
	hal_debug("%s #%d\n", __FUNCTION__, num);
	return NULL;
}

void *cDemux::getChannel()
{
	hal_debug("%s #%d\n", __FUNCTION__, num);
	return NULL;
}

bool cDemux::addPid(unsigned short Pid)
{
	hal_debug("%s: pid 0x%04hx\n", __func__, Pid);
	pes_pids pfd;
	int ret;
	if (dmx_type != DMX_TP_CHANNEL)
	{
		hal_info("%s pes_type %s not implemented yet! pid=%hx\n", __FUNCTION__, DMX_T[dmx_type], Pid);
		return false;
	}
	_open(this, num, fd, P->last_source, dmx_type, buffersize);
	if (fd == -1)
		hal_info("%s bucketfd not yet opened? pid=%hx\n", __FUNCTION__, Pid);
	pfd.fd = fd; /* dummy */
	pfd.pid = Pid;
	pesfds.push_back(pfd);
	ret = (ioctl(fd, DMX_ADD_PID, &Pid));
	if (ret < 0)
		hal_info("%s: DMX_ADD_PID (%m) pid=%hx\n", __func__, Pid);
	return (ret != -1);
}

void cDemux::removePid(unsigned short Pid)
{
	if (dmx_type != DMX_TP_CHANNEL)
	{
		hal_info("%s pes_type %s not implemented yet! pid=%hx\n", __FUNCTION__, DMX_T[dmx_type], Pid);
		return;
	}
	for (std::vector<pes_pids>::iterator i = pesfds.begin(); i != pesfds.end(); ++i)
	{
		if ((*i).pid == Pid) {
			hal_debug("removePid: removing demux fd %d pid 0x%04x\n", fd, Pid);
			if (ioctl(fd, DMX_REMOVE_PID, Pid) < 0)
				hal_info("%s: (DMX_REMOVE_PID, 0x%04hx): %m\n", __func__, Pid);
			pesfds.erase(i);
			return; /* TODO: what if the same PID is there multiple times */
		}
	}
	hal_info("%s pid 0x%04x not found\n", __FUNCTION__, Pid);
}

void cDemux::getSTC(int64_t * STC)
{
	/* apparently I can only get the PTS of the video decoder,
	 * but that's good enough for dvbsub */
	hal_debug("%s #%d\n", __func__, num);
	int64_t pts = 0;
	if (videoDecoder)
		pts = videoDecoder->GetPTS();
	*STC = pts;
}

int cDemux::getUnit(void)
{
	hal_debug("%s #%d\n", __FUNCTION__, num);
	/* just guessed that this is the right thing to do.
	   right now this is only used by the CA code which is stubbed out
	   anyway */
	return num;
}

bool cDemux::SetSource(int unit, int source)
{
	if (unit >= NUM_DEMUX || unit < 0) {
		hal_info_c("%s: unit (%d) out of range, NUM_DEMUX %d\n", __func__, unit, NUM_DEMUX);
		return false;
	}
	hal_debug_c("%s(%d, %d) => %d to %d\n", __func__, unit, source, dmx_source[unit], source);
	if (source < 0 || source >= NUM_DEMUXDEV)
		hal_info_c("%s(%d, %d) ERROR: source %d out of range!\n", __func__, unit, source, source);
	else
		dmx_source[unit] = source;
	return true;
}

int cDemux::GetSource(int unit)
{
	if (unit >= NUM_DEMUX || unit < 0) {
		hal_info_c("%s: unit (%d) out of range, NUM_DEMUX %d\n", __func__, unit, NUM_DEMUX);
		return -1;
	}
	hal_debug_c("%s(%d) => %d\n", __func__, unit, dmx_source[unit]);
	return dmx_source[unit];
}

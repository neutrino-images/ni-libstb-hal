/*
 * (C)
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

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring>

#include <pthread.h>
#include <aio.h>
#include <time.h>

#include <map>
#include <set>
#include <vector>

#include "record_lib.h"
#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_RECORD, this, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_RECORD, this, args)
#define hal_info_c(args...) _hal_info(HAL_DEBUG_RECORD, NULL, args)

/*
 * Recordings that share one demux device share one tap on it.
 *
 * On VU+ boxes a second TS tap on a demux device that already has one
 * delivers nothing: two overlapping recordings of a channel both set up
 * their filter without an error, the first gets the stream, the second
 * not a byte from its start to its end, even after the first stopped.
 * So there the first recording on a device opens the tap, and every
 * recording on that device reads from it: a thread reads the tap, cuts
 * the stream into TS packets and hands each recording the packets of
 * its own PIDs, in a buffer of its own. A recording that leaves takes
 * nothing from the others; the tap is closed with the last one.
 */
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUDUO4KSE || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4K || BOXMODEL_VUUNO4KSE || BOXMODEL_VUZERO4K
#define RECORD_SHARED_TAP 1
#else
#define RECORD_SHARED_TAP 0
#endif

#define TS_SIZE 188
#define TS_SYNC 0x47

struct record_tap;

struct record_feed
{
	record_tap *tap;
	std::set<unsigned short> pids;
	unsigned short vpid;
	std::vector<uint8_t> ring;	/* packets for this recording, not yet read */
	size_t rpos;
	size_t fill;
	bool overflow;			/* packets dropped since the last read */
	int error;			/* the tap failed: errno for the next read */
};

struct record_tap
{
	int device;
	cDemux *dmx;
	pthread_t thread;
	bool running;
	std::set<unsigned short> pids;	/* what the demux filters; only grows */
	std::vector<record_feed *> feeds;
	uint8_t carry[TS_SIZE];		/* a packet cut by the end of a read */
	int carry_len;
};

static pthread_mutex_t tap_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tap_cond = PTHREAD_COND_INITIALIZER;
static std::map<int, record_tap *> taps;

/* with tap_mutex held */
static void feed_put(record_feed *f, const uint8_t *pkt)
{
	if (f->ring.size() - f->fill < TS_SIZE)
	{
		f->overflow = true;
		return;
	}
	size_t wpos = (f->rpos + f->fill) % f->ring.size();
	for (int i = 0; i < TS_SIZE; i++)
		f->ring[(wpos + i) % f->ring.size()] = pkt[i];
	f->fill += TS_SIZE;
}

/* with tap_mutex held */
static void tap_deliver(record_tap *t, const uint8_t *pkt)
{
	unsigned short pid = ((pkt[1] & 0x1f) << 8) | pkt[2];
	for (std::vector<record_feed *>::iterator i = t->feeds.begin(); i != t->feeds.end(); ++i)
		if ((*i)->pids.count(pid))
			feed_put(*i, pkt);
}

static void *tap_thread(void *c)
{
	record_tap *t = (record_tap *)c;
	char threadname[17] = "RecordTap";
	prctl(PR_SET_NAME, (unsigned long)&threadname);
	const int len = TS_SIZE * 512;
	uint8_t *buf = (uint8_t *)malloc(len);
	if (!buf)
	{
		pthread_mutex_lock(&tap_mutex);
		for (std::vector<record_feed *>::iterator i = t->feeds.begin(); i != t->feeds.end(); ++i)
			(*i)->error = ENOMEM;
		pthread_cond_broadcast(&tap_cond);
		pthread_mutex_unlock(&tap_mutex);
		return NULL;
	}
	hal_info_c("RecordTap: demux%d: begin\n", t->device);
	while (t->running)
	{
		ssize_t s = t->dmx->Read(buf, len, 50);
		int err = errno;
		if (s <= 0)
		{
			if (s < 0 && err != EAGAIN)
			{
				pthread_mutex_lock(&tap_mutex);
				for (std::vector<record_feed *>::iterator i = t->feeds.begin(); i != t->feeds.end(); ++i)
				{
					if (err == EOVERFLOW)
						(*i)->overflow = true;
					else
						(*i)->error = err;
				}
				t->carry_len = 0;
				pthread_cond_broadcast(&tap_cond);
				pthread_mutex_unlock(&tap_mutex);
				if (err != EOVERFLOW)
				{
					errno = err;
					hal_info_c("RecordTap: demux%d: read failed: %m\n", t->device);
					break;
				}
			}
			continue;
		}
		pthread_mutex_lock(&tap_mutex);
		int pos = 0;
		if (t->carry_len)
		{
			int need = TS_SIZE - t->carry_len;
			if (need > s)
				need = s;
			memcpy(t->carry + t->carry_len, buf, need);
			t->carry_len += need;
			pos = need;
			if (t->carry_len == TS_SIZE)
			{
				if (t->carry[0] == TS_SYNC)
					tap_deliver(t, t->carry);
				t->carry_len = 0;
			}
		}
		while (pos < s)
		{
			if (buf[pos] != TS_SYNC)	/* out of step: find the next packet */
			{
				pos++;
				continue;
			}
			if (s - pos < TS_SIZE)
			{
				t->carry_len = s - pos;
				memcpy(t->carry, buf + pos, t->carry_len);
				break;
			}
			tap_deliver(t, buf + pos);
			pos += TS_SIZE;
		}
		pthread_cond_broadcast(&tap_cond);
		pthread_mutex_unlock(&tap_mutex);
	}
	free(buf);
	hal_info_c("RecordTap: demux%d: end\n", t->device);
	return NULL;
}

/* with tap_mutex held */
static void tap_add_pid(record_tap *t, unsigned short pid)
{
	if (t->pids.insert(pid).second)
		t->dmx->addPid(pid);
}

/* helper function to call the cpp thread loop */
void *execute_record_thread(void *c)
{
	cRecord *obj = (cRecord *)c;
	obj->RecordThread();
	return NULL;
}

void *execute_writer_thread(void *c)
{
	cRecord *obj = (cRecord *)c;
	obj->WriterThread();
	return NULL;
}

cRecord::cRecord(int num, int bs_dmx, int bs)
{
	hal_info("%s %d\n", __func__, num);
	dmx = NULL;
	record_thread_running = false;
	file_fd = -1;
	exit_flag = RECORD_STOPPED;
	dmx_num = num;
	bufsize = bs;
	bufsize_dmx = bs_dmx;
	failureCallback = NULL;
	failureData = NULL;
	feed = NULL;
}

/* Leave the shared tap, and close it when this was the last recording on
 * it. The PIDs it added stay on the demux until then: the others do not
 * get them, as each one takes only its own. */
void cRecord::LeaveTap()
{
	if (!feed)
		return;
	pthread_mutex_lock(&tap_mutex);
	record_tap *t = feed->tap;
	for (std::vector<record_feed *>::iterator i = t->feeds.begin(); i != t->feeds.end(); ++i)
	{
		if (*i == feed)
		{
			t->feeds.erase(i);
			break;
		}
	}
	delete feed;
	feed = NULL;
	bool last = t->feeds.empty();
	if (last)
	{
		taps.erase(t->device);
		t->running = false;
	}
	pthread_mutex_unlock(&tap_mutex);
	if (!last)
		return;
	pthread_join(t->thread, NULL);
	t->dmx->Stop();
	delete t->dmx;
	hal_info("%s: demux%d: tap closed\n", __func__, t->device);
	delete t;
}

/* Read for the recording: from its feed of the shared tap, or from its
 * own demux. A feed answers as the demux does: 0 after timeout ms with
 * nothing, -1 with EOVERFLOW once when packets were dropped. */
ssize_t cRecord::ReadSource(unsigned char *buf, int len, int timeout)
{
	if (!feed)
		return dmx->Read(buf, len, timeout);

	pthread_mutex_lock(&tap_mutex);
	if (!feed->fill && !feed->overflow && !feed->error)
	{
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_nsec += (long)timeout * 1000000L;
		until.tv_sec += until.tv_nsec / 1000000000L;
		until.tv_nsec %= 1000000000L;
		pthread_cond_timedwait(&tap_cond, &tap_mutex, &until);
	}
	ssize_t n;
	if (feed->error)
	{
		errno = feed->error;
		n = -1;
	}
	else if (feed->overflow)
	{
		feed->overflow = false;
		errno = EOVERFLOW;
		n = -1;
	}
	else
	{
		n = (ssize_t)feed->fill < len ? (ssize_t)feed->fill : len;
		for (ssize_t i = 0; i < n; i++)
			buf[i] = feed->ring[(feed->rpos + i) % feed->ring.size()];
		feed->rpos = (feed->rpos + n) % feed->ring.size();
		feed->fill -= n;
		if (!n)
			errno = EAGAIN;
	}
	pthread_mutex_unlock(&tap_mutex);
	return n;
}

cRecord::~cRecord()
{
	hal_info("%s: calling ::Stop()\n", __func__);
	Stop();
	hal_info("%s: end\n", __func__);
}

bool cRecord::Open(void)
{
	hal_info("%s\n", __func__);
	exit_flag = RECORD_STOPPED;
	return true;
}

#if 0
// unused
void cRecord::Close(void)
{
	hal_info("%s: \n", __func__);
}
#endif

bool cRecord::Start(int fd, unsigned short vpid, unsigned short *apids, int numpids, uint64_t)
{
	hal_info("%s: fd %d, vpid 0x%03x\n", __func__, fd, vpid);
	/* Marks the image for the hunt for the empty second recording on
	 * VU+ boxes: a log with this line was made with DMX_START checked
	 * (cDemux::Start() says when it fails), and one without it was not. */
	printf("[record] cRecord::Start: diag: DMX_START is checked\n");
	int i;

	if (RECORD_SHARED_TAP)
	{
		int device = cDemux::GetSource(dmx_num);
		printf("[record] cRecord::Start: diag: shared tap on demux%d\n", device);
		pthread_mutex_lock(&tap_mutex);
		record_tap *t;
		std::map<int, record_tap *>::iterator it = taps.find(device);
		if (it != taps.end())
			t = it->second;
		else
		{
			t = new record_tap;
			t->device = device;
			t->carry_len = 0;
			t->dmx = new cDemux(dmx_num);
			t->dmx->Open(DMX_TP_CHANNEL, NULL, bufsize_dmx);
			errno = 0;
			if (t->dmx->pesFilter(vpid))
				hal_info("%s: dmx unit %d (demux%d): pes filter pid 0x%04x ok\n",
					__func__, dmx_num, device, vpid);
			else
				hal_info("%s: dmx unit %d (demux%d): pes filter pid 0x%04x FAILED (%m)\n",
					__func__, dmx_num, device, vpid);
			t->pids.insert(vpid);
			t->dmx->Start();
			t->running = true;
			if (pthread_create(&t->thread, 0, tap_thread, t))
			{
				hal_info("%s: error creating tap thread! (%m)\n", __func__);
				delete t->dmx;
				delete t;
				pthread_mutex_unlock(&tap_mutex);
				exit_flag = RECORD_FAILED_READ;
				return false;
			}
			taps[device] = t;
		}
		feed = new record_feed;
		feed->tap = t;
		feed->vpid = vpid;
		feed->ring.resize(bufsize_dmx > TS_SIZE ? bufsize_dmx : 2048 * 1024);
		feed->rpos = 0;
		feed->fill = 0;
		feed->overflow = false;
		feed->error = 0;
		feed->pids.insert(vpid);
		tap_add_pid(t, vpid);
		for (i = 0; i < numpids; i++)
		{
			feed->pids.insert(apids[i]);
			tap_add_pid(t, apids[i]);
		}
		t->feeds.push_back(feed);
		hal_info("%s: dmx unit %d (demux%d): %d pids, %d recordings on the tap\n",
			__func__, dmx_num, device, (int)feed->pids.size(), (int)t->feeds.size());
		pthread_mutex_unlock(&tap_mutex);
	}
	else
	{
	if (!dmx)
		dmx = new cDemux(dmx_num);

	dmx->Open(DMX_TP_CHANNEL, NULL, bufsize_dmx);
	/* Say what the demux took. Two recordings of one channel share a
	 * demux device, and some drivers do not hand the same PIDs to both:
	 * the filter calls are where that would show. */
	errno = 0;
	if (dmx->pesFilter(vpid))
		hal_info("%s: dmx unit %d (demux%d): pes filter pid 0x%04x ok\n",
			__func__, dmx_num, cDemux::GetSource(dmx_num), vpid);
	else
		hal_info("%s: dmx unit %d (demux%d): pes filter pid 0x%04x FAILED (%m)\n",
			__func__, dmx_num, cDemux::GetSource(dmx_num), vpid);

	int pids_failed = 0;
	for (i = 0; i < numpids; i++)
		if (!dmx->addPid(apids[i]))
			pids_failed++;
	hal_info("%s: dmx unit %d (demux%d): %d more pids, %d failed\n",
		__func__, dmx_num, cDemux::GetSource(dmx_num), numpids, pids_failed);
	}

	file_fd = fd;
	exit_flag = RECORD_RUNNING;
	if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
		perror("posix_fadvise");

	i = pthread_create(&record_thread, 0, execute_record_thread, this);
	if (i != 0)
	{
		exit_flag = RECORD_FAILED_READ;
		errno = i;
		hal_info("%s: error creating thread! (%m)\n", __func__);
		LeaveTap();
		delete dmx;
		dmx = NULL;
		return false;
	}
	record_thread_running = true;
	return true;
}

bool cRecord::Stop(void)
{
	hal_info("%s\n", __func__);

	if (exit_flag != RECORD_RUNNING)
		hal_info("%s: status not RUNNING? (%d)\n", __func__, exit_flag);

	exit_flag = RECORD_STOPPED;
	if (record_thread_running)
		pthread_join(record_thread, NULL);
	record_thread_running = false;

	LeaveTap();
	/* We should probably do that from the destructor... */
	if (!dmx && !RECORD_SHARED_TAP)
		hal_info("%s: dmx == NULL?\n", __func__);
	else
		delete dmx;
	dmx = NULL;

	if (file_fd != -1)
		close(file_fd);
	else
		hal_info("%s: file_fd not open??\n", __func__);
	file_fd = -1;
	return true;
}

bool cRecord::ChangePids(unsigned short /*vpid*/, unsigned short *apids, int numapids)
{
	std::vector<pes_pids> pids;
	int j;
	bool found;
	unsigned short pid;
	hal_info("%s\n", __func__);
	if (feed)
	{
		/* the video PID stays, as below; the demux keeps what it has */
		pthread_mutex_lock(&tap_mutex);
		feed->pids.clear();
		feed->pids.insert(feed->vpid);
		for (j = 0; j < numapids; j++)
		{
			feed->pids.insert(apids[j]);
			tap_add_pid(feed->tap, apids[j]);
		}
		pthread_mutex_unlock(&tap_mutex);
		return true;
	}
	if (!dmx)
	{
		hal_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->pesfds;
	/* the first PID is the video pid, so start with the second PID... */
	for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i)
	{
		found = false;
		pid = (*i).pid;
		for (j = 0; j < numapids; j++)
		{
			if (pid == apids[j])
			{
				found = true;
				break;
			}
		}
		if (!found)
			dmx->removePid(pid);
	}
	for (j = 0; j < numapids; j++)
	{
		found = false;
		for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i)
		{
			if ((*i).pid == apids[j])
			{
				found = true;
				break;
			}
		}
		if (!found)
			dmx->addPid(apids[j]);
	}
	return true;
}

bool cRecord::AddPid(unsigned short pid)
{
	std::vector<pes_pids> pids;
	hal_info("%s: \n", __func__);
	if (feed)
	{
		pthread_mutex_lock(&tap_mutex);
		feed->pids.insert(pid);
		tap_add_pid(feed->tap, pid);
		pthread_mutex_unlock(&tap_mutex);
		return true;
	}
	if (!dmx)
	{
		hal_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->pesfds;
	for (std::vector<pes_pids>::const_iterator i = pids.begin(); i != pids.end(); ++i)
	{
		if ((*i).pid == pid)
			return true; /* or is it an error to try to add the same PID twice? */
	}
	return dmx->addPid(pid);
}

void cRecord::WriterThread()
{
	char threadname[17];
	strncpy(threadname, "WriterThread", sizeof(threadname));
	threadname[16] = 0;
	prctl(PR_SET_NAME, (unsigned long)&threadname);
	unsigned int chunk = 0;
	while (!sem_wait(&sem))
	{
		if (!io_len[chunk]) // empty, assume end of recording
			return;
		unsigned char *p_buf = io_buf[chunk];
		size_t p_len = io_len[chunk];
		while (p_len)
		{
			ssize_t written = write(file_fd, p_buf, p_len);
			if (written < 0)
				break;
			p_len -= written;
			p_buf += written;
		}
		if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
			perror("posix_fadvise");
		chunk++;
		chunk %= RECORD_WRITER_CHUNKS;
	}
}

void cRecord::RecordThread()
{
	hal_info("%s: begin\n", __func__);
	char threadname[17];
	strncpy(threadname, "RecordThread", sizeof(threadname));
	threadname[16] = 0;
	prctl(PR_SET_NAME, (unsigned long)&threadname);
	int readsize = bufsize / 16;
	int buf_pos = 0;
	int count = 0;
	int queued = 0;
	uint8_t *buf;
	struct aiocb a;

	buf = (uint8_t *)malloc(bufsize);
	hal_info("BUFSIZE=0x%x READSIZE=0x%x\n", bufsize, readsize);
	if (!buf)
	{
		exit_flag = RECORD_FAILED_MEMORY;
		hal_info("%s: unable to allocate buffer! (out of memory)\n", __func__);
		if (failureCallback)
			failureCallback(failureData);
		hal_info("%s: end\n", __func__);
		pthread_exit(NULL);
	}

	int val = fcntl(file_fd, F_GETFL);
	if (fcntl(file_fd, F_SETFL, val | O_APPEND))
		hal_info("%s: O_APPEND? (%m)\n", __func__);

	memset(&a, 0, sizeof(a));
	a.aio_fildes = file_fd;
	a.aio_sigevent.sigev_notify = SIGEV_NONE;

	if (!feed)
		dmx->Start();
	/* How much the demux delivers, every 10 seconds: a recording that
	 * gets nothing shows here, where nothing else says so. */
	uint64_t bytes_total = 0;
	uint64_t bytes_period = 0;
	struct timespec period_start;
	clock_gettime(CLOCK_MONOTONIC, &period_start);
	int overflow_count = 0;
	int dmx_overflow_count = 0;
	bool overflow = false;
	int r = 0;
	while (exit_flag == RECORD_RUNNING)
	{
		if (buf_pos < bufsize)
		{
			if (overflow_count)
			{
				hal_info("%s: Overflow cleared after %d iterations\n", __func__, overflow_count);
				overflow_count = 0;
			}
			int toread = bufsize - buf_pos;
			if (toread > readsize)
				toread = readsize;
			ssize_t s = ReadSource(buf + buf_pos, toread, 50);
			hal_debug("%s: buf_pos %6d s %6d / %6d\n", __func__, buf_pos, (int)s, bufsize - buf_pos);
			int read_errno = errno;
			if (s > 0)
			{
				bytes_total += s;
				bytes_period += s;
			}
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (now.tv_sec - period_start.tv_sec >= 10)
			{
				hal_info("%s: dmx unit %d (demux%d): %" PRIu64 " bytes in %ld s, %" PRIu64 " in all\n",
					__func__, dmx_num, cDemux::GetSource(dmx_num), bytes_period,
					(long)(now.tv_sec - period_start.tv_sec), bytes_total);
				bytes_period = 0;
				period_start = now;
			}
			errno = read_errno; /* the log above must not decide below */
			if (s < 0)
			{
				int err = errno;
				if (err == EOVERFLOW)
				{
					/* The demux had more than its buffer holds and
					 * dropped the rest. It says so once and then goes
					 * on delivering, so the recording goes on as well:
					 * a gap in the file is better than a file that ends
					 * here while the recording is still shown running. */
					if (!(dmx_overflow_count++ % 10))
						hal_info("%s: demux buffer overflow, data lost (%d)\n", __func__, dmx_overflow_count);
					state = REC_STATUS_OVERFLOW;
				}
				else if (err != EAGAIN)
				{
					errno = err;
					hal_info("%s: read failed: %m\n", __func__);
					exit_flag = RECORD_FAILED_READ;
					state = REC_STATUS_OVERFLOW;
					break;
				}
			}
			else
			{
				overflow = false;
				buf_pos += s;
				if (count > 100)
				{
					if (buf_pos < bufsize / 2)
						continue;
				}
				else
				{
					count += 1;
				}
			}
		}
		else
		{
			if (!overflow)
				overflow_count = 0;
			overflow = true;
			if (!(overflow_count % 10))
				hal_info("%s: buffer full! Overflow? (%d)\n", __func__, ++overflow_count);
			state = REC_STATUS_SLOW;
		}
		r = aio_error(&a);
		if (r == EINPROGRESS)
		{
			hal_debug("%s: aio in progress, free: %d\n", __func__, bufsize - buf_pos);
			continue;
		}
		// not calling aio_return causes a memory leak --martii
		r = aio_return(&a);
		if (r < 0)
		{
			exit_flag = RECORD_FAILED_FILE;
			hal_debug("%s: aio_return = %d (%m)\n", __func__, r);
			break;
		}
		else
			hal_debug("%s: aio_return = %d, free: %d\n", __func__, r, bufsize - buf_pos);
		if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
			perror("posix_fadvise");
		if (queued)
		{
			memmove(buf, buf + queued, buf_pos - queued);
			buf_pos -= queued;
		}
		queued = buf_pos;
		a.aio_buf = buf;
		a.aio_nbytes = queued;
		r = aio_write(&a);
		if (r)
		{
			hal_info("%s: aio_write %d (%m)\n", __func__, r);
			exit_flag = RECORD_FAILED_FILE;
			break;
		}
	}
	if (!feed)
		dmx->Stop();
	hal_info("%s: dmx unit %d (demux%d): %" PRIu64 " bytes in all\n",
		__func__, dmx_num, cDemux::GetSource(dmx_num), bytes_total);
	while (true) /* write out the unwritten buffer content */
	{
		hal_debug("%s: run-out write, buf_pos %d\n", __func__, buf_pos);
		r = aio_error(&a);
		if (r == EINPROGRESS)
		{
			usleep(50000);
			continue;
		}
		r = aio_return(&a);
		if (r < 0)
		{
			exit_flag = RECORD_FAILED_FILE;
			hal_info("%s: aio_result: %d (%m)\n", __func__, r);
			break;
		}
		if (!queued)
			break;
		memmove(buf, buf + queued, buf_pos - queued);
		buf_pos -= queued;
		queued = buf_pos;
		a.aio_buf = buf;
		a.aio_nbytes = queued;
		r = aio_write(&a);
	}
	free(buf);

#if 0
	// TODO: do we need to notify neutrino about failing recording?
	CEventServer eventServer;
	eventServer.registerEvent2(NeutrinoMessages::EVT_RECORDING_ENDED, CEventServer::INITID_NEUTRINO, "/tmp/neutrino.sock");
	stream2file_status2_t s;
	s.status = exit_flag;
	strncpy(s.filename, basename(myfilename), 512);
	s.filename[511] = '\0';
	strncpy(s.dir, dirname(myfilename), 100);
	s.dir[99] = '\0';
	eventServer.sendEvent(NeutrinoMessages::EVT_RECORDING_ENDED, CEventServer::INITID_NEUTRINO, &s, sizeof(s));
	printf("[stream2file]: pthreads exit code: %i, dir: '%s', filename: '%s' myfilename: '%s'\n", exit_flag, s.dir, s.filename, myfilename);
#endif

	if ((exit_flag != RECORD_STOPPED) && failureCallback)
		failureCallback(failureData);
	hal_info("%s: end\n", __func__);
	pthread_exit(NULL);
}

int cRecord::GetStatus()
{
	return (exit_flag == RECORD_STOPPED) ? REC_STATUS_STOPPED : REC_STATUS_OK;
}

void cRecord::ResetStatus()
{
	return;
}

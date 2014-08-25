#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring>

#include <aio.h>

#include "record_lib.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_RECORD, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_RECORD, this, args)

/* helper functions to call the cpp thread loops */
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
	lt_info("%s %d\n", __func__, num);
	dmx = NULL;
	record_thread_running = false;
	file_fd = -1;
	exit_flag = RECORD_STOPPED;
	dmx_num = num;
	bufsize = bs;
	bufsize_dmx = bs_dmx;
	failureCallback = NULL;
	failureData = NULL;
}

cRecord::~cRecord()
{
	lt_info("%s: calling ::Stop()\n", __func__);
	Stop();
	lt_info("%s: end\n", __func__);
}

bool cRecord::Open(void)
{
	lt_info("%s\n", __func__);
	exit_flag = RECORD_STOPPED;
	return true;
}

#if 0
// unused
void cRecord::Close(void)
{
	lt_info("%s: \n", __func__);
}
#endif

bool cRecord::Start(int fd, unsigned short vpid, unsigned short *apids, int numpids, uint64_t)
{
	lt_info("%s: fd %d, vpid 0x%03x\n", __func__, fd, vpid);
	int i;

	if (!dmx)
		dmx = new cDemux(dmx_num);

	dmx->Open(DMX_TP_CHANNEL, NULL, bufsize_dmx);
	dmx->pesFilter(vpid);

	for (i = 0; i < numpids; i++)
		dmx->addPid(apids[i]);

	file_fd = fd;
	exit_flag = RECORD_RUNNING;
	if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
		perror("posix_fadvise");

	i = pthread_create(&record_thread, 0, execute_record_thread, this);
	if (i != 0)
	{
		exit_flag = RECORD_FAILED_READ;
		errno = i;
		lt_info("%s: error creating thread! (%m)\n", __func__);
		delete dmx;
		dmx = NULL;
		return false;
	}
	record_thread_running = true;
	return true;
}

bool cRecord::Stop(void)
{
	lt_info("%s\n", __func__);

	if (exit_flag != RECORD_RUNNING)
		lt_info("%s: status not RUNNING? (%d)\n", __func__, exit_flag);

	exit_flag = RECORD_STOPPED;
	if (record_thread_running)
		pthread_join(record_thread, NULL);
	record_thread_running = false;

	/* We should probably do that from the destructor... */
	if (!dmx)
		lt_info("%s: dmx == NULL?\n", __func__);
	else
		delete dmx;
	dmx = NULL;

	if (file_fd != -1)
		close(file_fd);
	else
		lt_info("%s: file_fd not open??\n", __func__);
	file_fd = -1;
	return true;
}

bool cRecord::ChangePids(unsigned short /*vpid*/, unsigned short *apids, int numapids)
{
	std::vector<pes_pids> pids;
	int j;
	bool found;
	unsigned short pid;
	lt_info("%s\n", __func__);
	if (!dmx) {
		lt_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->getPesPids();
	/* the first PID is the video pid, so start with the second PID... */
	for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i) {
		found = false;
		pid = (*i).pid;
		for (j = 0; j < numapids; j++) {
			if (pid == apids[j]) {
				found = true;
				break;
			}
		}
		if (!found)
			dmx->removePid(pid);
	}
	for (j = 0; j < numapids; j++) {
		found = false;
		for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i) {
			if ((*i).pid == apids[j]) {
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
	lt_info("%s: \n", __func__);
	if (!dmx) {
		lt_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->getPesPids();
	for (std::vector<pes_pids>::const_iterator i = pids.begin(); i != pids.end(); ++i) {
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
	prctl (PR_SET_NAME, (unsigned long)&threadname);
	unsigned int chunk = 0;
	while (!sem_wait(&sem)) {
		if (!io_len[chunk]) // empty, assume end of recording
			return;
		unsigned char *p_buf = io_buf[chunk];
		size_t p_len = io_len[chunk];
		while (p_len) {
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
	lt_info("%s: begin\n", __func__);
	char threadname[17];
	strncpy(threadname, "RecordThread", sizeof(threadname));
	threadname[16] = 0;
	prctl (PR_SET_NAME, (unsigned long)&threadname);
	int readsize = bufsize/16;
	int buf_pos = 0;
	int count = 0;
	int queued = 0;
	uint8_t *buf;
	struct aiocb a;

	buf = (uint8_t *)malloc(bufsize);
	lt_info("BUFSIZE=0x%x READSIZE=0x%x\n", bufsize, readsize);
	if (!buf)
	{
		exit_flag = RECORD_FAILED_MEMORY;
		lt_info("%s: unable to allocate buffer! (out of memory)\n", __func__);
		if (failureCallback)
			failureCallback(failureData);
		lt_info("%s: end\n", __func__);
		pthread_exit(NULL);
	}

	int val = fcntl(file_fd, F_GETFL);
	if (fcntl(file_fd, F_SETFL, val|O_APPEND))
		lt_info("%s: O_APPEND? (%m)\n", __func__);

	memset(&a, 0, sizeof(a));
	a.aio_fildes = file_fd;
	a.aio_sigevent.sigev_notify = SIGEV_NONE;

	dmx->Start();
	int overflow_count = 0;
	bool overflow = false;
	int r = 0;
	while (exit_flag == RECORD_RUNNING)
	{
		if (buf_pos < bufsize)
		{
			if (overflow_count) {
				lt_info("%s: Overflow cleared after %d iterations\n", __func__, overflow_count);
				overflow_count = 0;
			}
			int toread = bufsize - buf_pos;
			if (toread > readsize)
				toread = readsize;
			ssize_t s = dmx->Read(buf + buf_pos, toread, 50);
			lt_debug("%s: buf_pos %6d s %6d / %6d\n", __func__, buf_pos, (int)s, bufsize - buf_pos);
			if (s < 0)
			{
				if (errno != EAGAIN && (errno != EOVERFLOW || !overflow))
				{
					lt_info("%s: read failed: %m\n", __func__);
					exit_flag = RECORD_FAILED_READ;
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
				lt_info("%s: buffer full! Overflow? (%d)\n", __func__, ++overflow_count);
		}
		r = aio_error(&a);
		if (r == EINPROGRESS)
		{
			lt_debug("%s: aio in progress, free: %d\n", __func__, bufsize - buf_pos);
			continue;
		}
		// not calling aio_return causes a memory leak  --martii
		r = aio_return(&a);
		if (r < 0)
		{
			exit_flag = RECORD_FAILED_FILE;
			lt_debug("%s: aio_return = %d (%m)\n", __func__, r);
			break;
		}
		else
			lt_debug("%s: aio_return = %d, free: %d\n", __func__, r, bufsize - buf_pos);
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
			lt_info("%s: aio_write %d (%m)\n", __func__, r);
			exit_flag = RECORD_FAILED_FILE;
			break;
		}
	}
	dmx->Stop();
	while (true) /* write out the unwritten buffer content */
	{
		lt_debug("%s: run-out write, buf_pos %d\n", __func__, buf_pos);
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
			lt_info("%s: aio_result: %d (%m)\n", __func__, r);
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
	strncpy(s.filename,basename(myfilename),512);
	s.filename[511] = '\0';
	strncpy(s.dir,dirname(myfilename),100);
	s.dir[99] = '\0';
	eventServer.sendEvent(NeutrinoMessages::EVT_RECORDING_ENDED, CEventServer::INITID_NEUTRINO, &s, sizeof(s));
	printf("[stream2file]: pthreads exit code: %i, dir: '%s', filename: '%s' myfilename: '%s'\n", exit_flag, s.dir, s.filename, myfilename);
#endif

	if ((exit_flag != RECORD_STOPPED) && failureCallback)
		failureCallback(failureData);
	lt_info("%s: end\n", __func__);
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

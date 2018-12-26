#ifndef __RECORD_LIB_H__
#define __RECORD_LIB_H__

#include <semaphore.h>
#include "dmx_hal.h"

#define REC_STATUS_OK 0
#define REC_STATUS_SLOW 1
#define REC_STATUS_OVERFLOW 2
#define REC_STATUS_STOPPED 4

typedef enum {
	RECORD_RUNNING,
	RECORD_STOPPED,
	RECORD_FAILED_READ,	/* failed to read from DMX */
	RECORD_FAILED_OVERFLOW,	/* cannot write fast enough */
	RECORD_FAILED_FILE,	/* cannot write to file */
	RECORD_FAILED_MEMORY	/* out of memory */
} record_state_t;

class cRecord
{
	private:
		int file_fd;
		int dmx_num;
		cDemux *dmx;
		pthread_t record_thread;
		bool record_thread_running;
		record_state_t exit_flag;
		int state;
		int bufsize;
		int bufsize_dmx;
		void (*failureCallback)(void *);
		void *failureData;

		sem_t sem;
#define RECORD_WRITER_CHUNKS 16
		unsigned char *io_buf[RECORD_WRITER_CHUNKS];
		size_t io_len[RECORD_WRITER_CHUNKS];
	public:
		cRecord(int num = 0, int bs_dmx = 2048 * 1024, int bs = 4096 * 1024); 
		void setFailureCallback(void (*f)(void *), void *d) { failureCallback = f; failureData = d; }
		~cRecord();

		bool Open();
		bool Start(int fd, unsigned short vpid, unsigned short *apids, int numapids, uint64_t ch = 0);
		bool Stop(void);
		bool AddPid(unsigned short pid);
		int  GetStatus();
		void ResetStatus();
		bool ChangePids(unsigned short vpid, unsigned short *apids, int numapids);

		void RecordThread();
		void WriterThread();
};

#endif // __RECORD_LIB_H__

#ifndef __RECORD_TD_H
#define __RECORD_TD_H

#include <pthread.h>
#include "dmx_lib.h"

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
		cDemux *dmx;
		pthread_t record_thread;
		bool record_thread_running;
		record_state_t exit_flag;
#ifdef MARTII
		int bufsize;
		int bufsize_dmx;
		void (*failureCallback)(void *);
		void *failureData;
#endif
	public:
#ifdef MARTII
		cRecord(int num = 0, int bs_dmx = 100 * 188 * 1024, int bs = 100 * 188 * 1024);
		void setFailureCallback(void (*f)(void *), void *d) { failureCallback = f; failureData = d; }
#else
		cRecord(int num = 0);
#endif
		~cRecord();

		bool Open();
		bool Start(int fd, unsigned short vpid, unsigned short *apids, int numapids);
		bool Stop(void);
		bool AddPid(unsigned short pid);
		bool ChangePids(unsigned short vpid, unsigned short *apids, int numapids);

		void RecordThread();
};
#endif

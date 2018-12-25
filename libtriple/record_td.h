#ifndef __RECORD_TD_H__
#define __RECORD_TD_H__

#include <pthread.h>
#include "dmx_hal.h"

#define REC_STATUS_OK 0
#define REC_STATUS_SLOW 1
#define REC_STATUS_OVERFLOW 2

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
		int state;
	public:
		cRecord(int num = 0);
		~cRecord();

		bool Open();
		bool Start(int fd, unsigned short vpid, unsigned short *apids, int numapids, uint64_t ch = 0);
		bool Stop(void);
		bool AddPid(unsigned short pid);
		int  GetStatus();
		void ResetStatus();
		bool ChangePids(unsigned short vpid, unsigned short *apids, int numapids);

		void RecordThread();
};

#endif // __RECORD_TD_H__

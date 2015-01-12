#ifndef _THREAD_ABSTRACTION_H
#define _THREAD_ABSTRACTION_H

#include <pthread.h>

class Thread
{
	bool mIsRunning;
	pthread_t mThread;

	static void* runThread(void*);
	Thread(const Thread&);
	const Thread& operator=(const Thread&);

	public:
		Thread();
		virtual ~Thread();
		int startThread();
		int cancelThread();
		int detachThread();
		int joinThread();

		int setCancelModeDisable();
		int setSchedulePriority(int);

	protected:
		virtual void run() = 0;
};

#endif

#ifndef _SIMPLETHREAD_ABSTRACTION_H
#define _SIMPLETHREAD_ABSTRACTION_H

#include <pthread.h>

class SimpleThread
{
	bool mIsRunning;
	pthread_t mThread;

	static void* runThread(void*);
	SimpleThread(const SimpleThread&);
	const SimpleThread& operator=(const SimpleThread&);

	public:
		SimpleThread();
		~SimpleThread();
		void startThread();
		void joinThread();

	protected:
		virtual void run() = 0;
};

#endif

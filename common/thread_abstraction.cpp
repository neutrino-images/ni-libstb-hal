#include "thread_abstraction.h"

SimpleThread::SimpleThread() :
	mIsRunning(false),
	mThread()
{
}

SimpleThread::~SimpleThread()
{
	// if thread is still running on object destruction, cancel thread the hard way:
	if (mIsRunning)
	{
		pthread_cancel(mThread);
	}
}

void SimpleThread::startThread()
{
	mIsRunning = true;
	pthread_create(&mThread, 0, &SimpleThread::runThread, this);
}

void SimpleThread::joinThread()
{
	pthread_join(mThread, 0);
	mIsRunning = false;
}

void* SimpleThread::runThread(void* ptr)
{
	static_cast<SimpleThread*>(ptr)->run();
	return 0;
}

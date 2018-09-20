#include "thread_abstraction.h"

Thread::Thread() :
	mIsRunning(false),
	mThread()
{
}

Thread::~Thread()
{
	// if thread is still running on object destruction, cancel thread the hard way:
	if (mIsRunning)
	{
		pthread_cancel(mThread);
	}
}

void Thread::startThread()
{
	mIsRunning = true;
	pthread_create(&mThread, 0, &Thread::runThread, this);
}

void Thread::joinThread()
{
	pthread_join(mThread, 0);
	mIsRunning = false;
}

void* Thread::runThread(void* ptr)
{
	((Thread*)ptr)->run();
	return 0;
}

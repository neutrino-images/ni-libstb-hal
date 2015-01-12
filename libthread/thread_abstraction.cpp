#include <thread_abstraction.h>

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

int Thread::startThread()
{
	mIsRunning = true;
	return pthread_create(&mThread, 0, &Thread::runThread, this);
}

int Thread::cancelThread()
{
	return pthread_cancel(mThread);
	mIsRunning = false;
}

int Thread::detachThread()
{
	mIsRunning = false; // thread shall not cancel on object destruction!
	return pthread_detach(mThread);
}

int Thread::joinThread()
{
	int ret = pthread_join(mThread, 0);
	mIsRunning = false;
	return ret;
}

int Thread::setCancelModeDisable()
{
	return pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
}

int Thread::setSchedulePriority(int prio)
{
	return pthread_setschedprio(mThread, prio);
}

void* Thread::runThread(void* ptr)
{
	Thread* t = static_cast<Thread*>(ptr);
	t->run();
	return 0;
}

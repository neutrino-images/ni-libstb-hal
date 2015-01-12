#include <condition_abstraction.h>

Condition::Condition() :
	mCondition()
{
	pthread_cond_init(&mCondition, 0);
}

Condition::~Condition()
{
	pthread_cond_destroy(&mCondition);
}

int Condition::wait(Mutex* const aMutex)
{
	return pthread_cond_wait(&mCondition, &(aMutex->mMutex));
}

int Condition::broadcast()
{
	return pthread_cond_broadcast(&mCondition);
}

int Condition::signal()
{
	return pthread_cond_signal(&mCondition);
}

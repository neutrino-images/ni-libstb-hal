#include "mutex_abstraction.h"

Mutex::Mutex() :
	mMutex()
{
	pthread_mutex_init(&mMutex, 0);
}

Mutex::~Mutex()
{
	pthread_mutex_destroy(&mMutex);
}

void Mutex::lock()
{
	pthread_mutex_lock(&mMutex);
}

void Mutex::unlock()
{
	pthread_mutex_unlock(&mMutex);
}

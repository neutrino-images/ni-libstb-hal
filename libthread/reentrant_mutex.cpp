#include <reentrant_mutex.h>

ReentrantMutex::ReentrantMutex() :
	Mutex(PTHREAD_MUTEX_RECURSIVE)
{
}

ReentrantMutex::~ReentrantMutex()
{
	//safely destroyed in ~Mutex();
}

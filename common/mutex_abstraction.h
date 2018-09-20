#ifndef _MUTEX_ABSTRACTION_H
#define _MUTEX_ABSTRACTION_H

#include <pthread.h>

class Mutex
{
	pthread_mutex_t mMutex;

	Mutex(const Mutex&);
	const Mutex& operator=(const Mutex&);

	public:
		Mutex();
		virtual ~Mutex();
		void lock();
		void unlock();
};

#endif

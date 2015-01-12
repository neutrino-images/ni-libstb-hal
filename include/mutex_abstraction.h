#ifndef _MUTEX_ABSTRACTION_H
#define _MUTEX_ABSTRACTION_H

#include <pthread.h>

class Mutex
{
	friend class Condition;

	pthread_mutex_t mMutex;

	Mutex(const Mutex&);
	const Mutex& operator=(const Mutex&);

	protected:
		explicit Mutex(int);

	public:
		Mutex();
		virtual ~Mutex();
		virtual void lock();
		virtual void unlock();
};

#endif

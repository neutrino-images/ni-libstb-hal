#ifndef _SCOPED_LOCK_H
#define _SCOPED_LOCK_H

#include "mutex_abstraction.h"

class ScopedLock
{
	Mutex& mMutex;

	ScopedLock(const ScopedLock&);
	const ScopedLock& operator=(const ScopedLock&);

	public:
		ScopedLock(Mutex&);
		~ScopedLock();
};

#endif

#include <scoped_lock.h>

ScopedLock::ScopedLock(Mutex& aMutex) :
	mMutex(aMutex)
{
	mMutex.lock();
}

ScopedLock::~ScopedLock()
{
	mMutex.unlock();
}

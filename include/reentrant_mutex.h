#ifndef _REENTRANT_MUTEX_H
#define _REENTRANT_MUTEX_H

#include "mutex_abstraction.h"

class ReentrantMutex : public Mutex
{
	ReentrantMutex(const ReentrantMutex&);
	const ReentrantMutex& operator=(const ReentrantMutex&);

	public:
		ReentrantMutex();
		virtual ~ReentrantMutex();
};

#endif

#ifndef _CONDITION_ABSTRACTION_H
#define _CONDITION_ABSTRACTION_H

#include <pthread.h>

#include "mutex_abstraction.h"

class Condition
{
	pthread_cond_t mCondition;

	Condition(const Condition&);
	const Condition& operator=(const Condition&);

	public:
		Condition();
		virtual ~Condition();
		virtual int wait(Mutex* const aMutex);
		virtual int broadcast();
		virtual int signal();
};

#endif

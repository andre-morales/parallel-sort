#pragma once
#include "cond_lock.h"
#include <stdatomic.h>

typedef struct {
	int target;

	#if !USE_SPINLOCKS
	ConditionLock lock;
	int count;
	#else
	atomic_int atCount;
	atomic_int atHolding;
	atomic_int atFlag;
	#endif
} Barrier;

void barr_init(Barrier* barrier, int target);
void barr_wait(Barrier* barrier);
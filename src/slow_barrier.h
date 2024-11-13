#pragma once
#include "cond_lock.h"
#include <stdbool.h>
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
} SlowBarrier;

void slowbarr_init(SlowBarrier* barrier, int target);
bool slowbarr_wait(SlowBarrier* barrier);
void slowbarr_lower(SlowBarrier* barrier);
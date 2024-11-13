#pragma once
#include "cond_lock.h"
#include <stdbool.h>

typedef struct {
	ConditionLock lock;

    // How many threads are expected to arrive at this barrier.
	int target;

    // How many threads have arrived at the barrier so far.
    int count;

    // How many threads should be allowed to move on before releasing the barrier fully.
	int allow;

    // How many threads have been early released so far.
    int released;
} CountBarrier;

void countbarr_init(CountBarrier* barrier, int target, int allow);
int countbarr_wait(CountBarrier* barrier);
void countbarr_lower(CountBarrier* barrier);
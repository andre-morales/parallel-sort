#pragma once
#include "cond_lock.h"
#include <stdbool.h>

typedef struct {
	ConditionLock lock;
	int target;
	int count;
} SlowBarrier;

void slowbarr_init(SlowBarrier* barrier, int target);
bool slowbarr_wait(SlowBarrier* barrier);
void slowbarr_lower(SlowBarrier* barrier);
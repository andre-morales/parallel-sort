#pragma once
#include "cond_lock.h"
#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
	int target;
	int count;
	ConditionLock lock;
} SlowBarrier;

void slowbarr_init(SlowBarrier* barrier, int target);
bool slowbarr_wait(SlowBarrier* barrier);
void slowbarr_lower(SlowBarrier* barrier);
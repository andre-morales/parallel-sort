#pragma once
#include "cond_lock.h"
#include <stdatomic.h>

typedef struct {
	int target;
	int count;
	ConditionLock lock;
} Barrier;

void barr_init(Barrier* barrier, int target);
void barr_wait(Barrier* barrier);
#pragma once
#include "cond_lock.h"

typedef struct {
	ConditionLock lock;
	int target;
	int count;
} Barrier;

void barr_init(Barrier* barrier, int target);
void barr_wait(Barrier* barrier);
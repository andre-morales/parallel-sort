#pragma once
#include "cond_lock.h"

typedef struct {
	ConditionLock lock;
	int target;
	int count;
} Barrier;

void barrierInit(Barrier* barrier, int target);
void barrierWait(Barrier* barrier);
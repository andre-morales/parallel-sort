#include "barrier.h"
#include <stdbool.h>
#include <stdio.h>

#if !USE_SPINLOCKS
void barr_init(Barrier* barrier, int target) {
	cl_init(&barrier->lock);
	barrier->target = target;
}

void barr_wait(Barrier* barrier) {
	cl_lock(&barrier->lock);
	barrier->count++;
	if (barrier->count == barrier->target) {
		cl_notifyAll(&barrier->lock);
		barrier->count = 0;
	} else {
		cl_wait(&barrier->lock);
	}
	cl_unlock(&barrier->lock);
}

#else

void barr_init(Barrier* barrier, int target) {
	barrier->target = target;
	atomic_init(&barrier->atCount, 0);
	atomic_init(&barrier->atHolding, 0);
	atomic_init(&barrier->atFlag, 0);
}

void barr_wait(Barrier* barrier) {
	int newCount = atomic_fetch_add(&barrier->atCount, 1);

	// If I am the last thread
	if (newCount == barrier->target - 1) {
		atomic_store(&barrier->atCount, 0);
		atomic_store(&barrier->atHolding, barrier->target);
		atomic_store(&barrier->atFlag, 1);
	} else {
		while(atomic_load(&barrier->atFlag) == 0) {}
		
		int id = atomic_fetch_sub(&barrier->atHolding, 1);
		if (id == 2) {
			atomic_store(&barrier->atFlag, 0);
		}
	}
}
#endif
#include "slow_barrier.h"

#if !USE_SPINLOCKS
void slowbarr_init(SlowBarrier* barrier, int target) {
	cl_init(&barrier->lock);
	barrier->target = target;
}

bool slowbarr_wait(SlowBarrier* barrier) {
	cl_lock(&barrier->lock);
	barrier->count++;
	if (barrier->count == barrier->target) {
		cl_unlock(&barrier->lock);
		return true;
	}

	cl_wait(&barrier->lock);
	cl_unlock(&barrier->lock);
	return false;
}

void slowbarr_lower(SlowBarrier* barrier) {
	cl_lock(&barrier->lock);
	barrier->count = 0;
	cl_notifyAll(&barrier->lock);
	cl_unlock(&barrier->lock);
}

#else

void slowbarr_init(SlowBarrier* barrier, int target) {
	barrier->target = target;
	atomic_init(&barrier->atCount, 0);
	atomic_init(&barrier->atHolding, 0);
	atomic_init(&barrier->atFlag, 0);
}

bool slowbarr_wait(SlowBarrier* barrier) {
	int newCount = atomic_fetch_add(&barrier->atCount, 1);

	// If I am the last thread
	if (newCount == barrier->target - 1) {
		return true;
	} else {
		while(atomic_load(&barrier->atFlag) == 0) {}
		
		int id = atomic_fetch_sub(&barrier->atHolding, 1);
		if (id == 2) {
			atomic_store(&barrier->atFlag, 0);
		}
		return false;
	}
}

void slowbarr_lower(SlowBarrier* barrier) {
	atomic_store(&barrier->atCount, 0);
	atomic_store(&barrier->atHolding, barrier->target);
	atomic_store(&barrier->atFlag, 1);
}
#endif
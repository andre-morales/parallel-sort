#include "slow_barrier.h"

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

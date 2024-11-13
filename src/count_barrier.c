#include "count_barrier.h"

void countbarr_init(CountBarrier* barrier, int target, int allow) {
	cl_init(&barrier->lock);
	barrier->target = target;
	barrier->allow = allow;
	barrier->count = 0;
	barrier->released = 0;
}

int countbarr_wait(CountBarrier* barrier) {
	cl_lock(&barrier->lock);
	// If the last thread arrived at the barrier
	if (++barrier->count == barrier->target) {
		// This thread will go on without waiting, but we should wake the other threads
		for (int i = 0; i < barrier->allow - 1; i++) {
			cl_notify(&barrier->lock);
		}

		// Let this thread be released straight trough
		barrier->released++;
		cl_unlock(&barrier->lock);  
		return 1;
	}

	// Wait for the other threads to arrive
	cl_wait(&barrier->lock);

	// Account for the release of this thread
	int index = ++barrier->released;

	cl_unlock(&barrier->lock);

	// If this thread is in the early allowed threads, return its index.
	if (index <= barrier->allow) {
		return index;
	}
	
	// This thread was released with the full lowering.
	return 0;
}

void countbarr_lower(CountBarrier* barrier) {
	cl_lock(&barrier->lock);
	barrier->count = 0;
	cl_notifyAll(&barrier->lock);
	cl_unlock(&barrier->lock);
}

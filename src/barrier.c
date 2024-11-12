#include "barrier.h"

void barrierInit(Barrier* barrier, int target) {
    cl_init(&barrier->lock);
    barrier->target = target;
}

void barrierWait(Barrier* barrier) {
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

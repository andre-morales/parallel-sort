#include "cond_lock.h"

void cl_init(ConditionLock* cl) {
    pthread_mutex_init(&cl->mutex, NULL);
    pthread_cond_init(&cl->cv, NULL);
}

void cl_lock(ConditionLock* cl) {
    pthread_mutex_lock(&cl->mutex);
}

void cl_wait(ConditionLock* cl) {
    pthread_cond_wait(&cl->cv, &cl->mutex);
}

void cl_notify(ConditionLock* cl) {
    pthread_cond_signal(&cl->cv);
}

void cl_notifyAll(ConditionLock* cl) {
    pthread_cond_broadcast(&cl->cv);
}

void cl_unlock(ConditionLock* cl) {
    pthread_mutex_unlock(&cl->mutex);
}
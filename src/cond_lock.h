#pragma once
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cv;
} ConditionLock;

void cl_init(ConditionLock*);
void cl_lock(ConditionLock*);
void cl_unlock(ConditionLock*);
void cl_wait(ConditionLock*);
void cl_notify(ConditionLock*);
void cl_notifyAll(ConditionLock*);

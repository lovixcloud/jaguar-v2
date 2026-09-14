#ifndef JAG_WORKER_H
#define JAG_WORKER_H

#include "runtime/core/value.h"
#include <pthread.h>
#include <stdbool.h>

typedef JagValue *(*WorkerJobFn)(JagValue *arg);

typedef struct WorkerJob {
    WorkerJobFn fn;
    JagValue *arg;
    JagValue *result;
    bool completed;
    struct WorkerJob *next;
} WorkerJob;

typedef struct {
    int num_threads;
    pthread_t *threads;
    WorkerJob *job_head;
    WorkerJob *job_tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_terminated;
} WorkerPool;

WorkerPool *jag_worker_pool(int num_threads);
WorkerJob *jag_worker_submit(WorkerPool *pool, WorkerJobFn fn, JagValue *arg);
void jag_worker_pool_free(WorkerPool *pool);

#endif

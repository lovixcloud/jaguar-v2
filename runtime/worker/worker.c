#include "worker.h"
#include <stdio.h>
#include <stdlib.h>

static void *worker_thread_loop(void *arg) {
    WorkerPool *pool = (WorkerPool *)arg;
    for (;;) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->job_head && !pool->is_terminated) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        if (pool->is_terminated && !pool->job_head) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        WorkerJob *job = pool->job_head;
        if (job) {
            pool->job_head = job->next;
            if (!pool->job_head) pool->job_tail = NULL;
        }
        pthread_mutex_unlock(&pool->mutex);

        if (job && job->fn) {
            job->result = job->fn(job->arg);
            job->completed = true;
        }
    }
    return NULL;
}

WorkerPool *jag_worker_pool(int num_threads) {
    WorkerPool *pool = (WorkerPool *)calloc(1, sizeof(WorkerPool));
    pool->num_threads = num_threads;
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread_loop, pool);
    }
    return pool;
}

WorkerJob *jag_worker_submit(WorkerPool *pool, WorkerJobFn fn, JagValue *arg) {
    WorkerJob *job = (WorkerJob *)calloc(1, sizeof(WorkerJob));
    job->fn = fn;
    job->arg = jag_val_dup(arg);
    job->completed = false;

    pthread_mutex_lock(&pool->mutex);
    if (!pool->job_head) {
        pool->job_head = job;
        pool->job_tail = job;
    } else {
        pool->job_tail->next = job;
        pool->job_tail = job;
    }
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return job;
}

void jag_worker_pool_free(WorkerPool *pool) {
    if (!pool) return;
    pthread_mutex_lock(&pool->mutex);
    pool->is_terminated = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    free(pool->threads);
    free(pool);
}

#ifndef JAG_ASYNC_H
#define JAG_ASYNC_H

#include "runtime/core/value.h"
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>

typedef struct JagTask JagTask;
typedef void (*TaskFn)(void *arg);

typedef struct {
    int epoll_fd;
    bool running;
    ucontext_t main_context;
    JagTask *current_task;
} EventLoop;

struct JagTask {
    uint64_t id;
    bool completed;
    JagValue *result;
    char *error;
    ucontext_t context;
    char *stack;
    size_t stack_size;
    TaskFn fn;
    void *arg;
    struct JagTask *next;
};

void event_loop_init(void);
void event_loop_run(void);
void event_loop_stop(void);
EventLoop *get_event_loop(void);

JagTask *task_create(TaskFn fn, void *arg);
void task_resume(JagTask *task);
void task_yield(void);
JagValue *task_await(JagTask *task);

typedef void (*TimerCb)(void *arg);
uint64_t live_after(uint64_t ms, TimerCb cb, void *arg);
uint64_t live_every(uint64_t ms, TimerCb cb, void *arg);
void live_clear(uint64_t timer_id);

#endif

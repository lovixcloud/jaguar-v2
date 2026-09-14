#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

static EventLoop g_loop;
static uint64_t g_task_id_seq = 1;
static uint64_t g_timer_id_seq = 1;

typedef struct JagTimer {
    uint64_t id;
    uint64_t expiry_ms;
    uint64_t interval_ms;
    TimerCb cb;
    void *arg;
    bool is_repeating;
    struct JagTimer *next;
} JagTimer;

static JagTimer *g_timers = NULL;
static JagTask *g_task_queue = NULL;

static uint64_t current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

void event_loop_init(void) {
    memset(&g_loop, 0, sizeof(EventLoop));
    g_loop.epoll_fd = epoll_create1(0);
    g_loop.running = false;
}

EventLoop *get_event_loop(void) {
    return &g_loop;
}

static void task_entry(uint32_t hi, uint32_t lo) {
    uint64_t ptr_val = ((uint64_t)hi << 32) | lo;
    JagTask *task = (JagTask *)ptr_val;
    if (task && task->fn) {
        task->fn(task->arg);
    }
    task->completed = true;
    swapcontext(&task->context, &g_loop.main_context);
}

JagTask *task_create(TaskFn fn, void *arg) {
    JagTask *t = (JagTask *)calloc(1, sizeof(JagTask));
    t->id = g_task_id_seq++;
    t->fn = fn;
    t->arg = arg;
    t->stack_size = 128 * 1024;
    t->stack = (char *)malloc(t->stack_size);

    getcontext(&t->context);
    t->context.uc_stack.ss_sp = t->stack;
    t->context.uc_stack.ss_size = t->stack_size;
    t->context.uc_link = &g_loop.main_context;

    uint64_t ptr_val = (uint64_t)t;
    uint32_t hi = (uint32_t)(ptr_val >> 32);
    uint32_t lo = (uint32_t)(ptr_val & 0xFFFFFFFF);
    makecontext(&t->context, (void (*)(void))task_entry, 2, hi, lo);

    t->next = g_task_queue;
    g_task_queue = t;
    return t;
}

void task_resume(JagTask *task) {
    if (!task || task->completed) return;
    JagTask *prev = g_loop.current_task;
    g_loop.current_task = task;
    swapcontext(&g_loop.main_context, &task->context);
    g_loop.current_task = prev;
}

void task_yield(void) {
    if (g_loop.current_task) {
        swapcontext(&g_loop.current_task->context, &g_loop.main_context);
    }
}

JagValue *task_await(JagTask *task) {
    while (task && !task->completed) {
        task_yield();
    }
    return task ? task->result : NULL;
}

uint64_t live_after(uint64_t ms, TimerCb cb, void *arg) {
    JagTimer *t = (JagTimer *)calloc(1, sizeof(JagTimer));
    t->id = g_timer_id_seq++;
    t->expiry_ms = current_time_ms() + ms;
    t->interval_ms = 0;
    t->cb = cb;
    t->arg = arg;
    t->is_repeating = false;
    t->next = g_timers;
    g_timers = t;
    return t->id;
}

uint64_t live_every(uint64_t ms, TimerCb cb, void *arg) {
    JagTimer *t = (JagTimer *)calloc(1, sizeof(JagTimer));
    t->id = g_timer_id_seq++;
    t->expiry_ms = current_time_ms() + ms;
    t->interval_ms = ms;
    t->cb = cb;
    t->arg = arg;
    t->is_repeating = true;
    t->next = g_timers;
    g_timers = t;
    return t->id;
}

void live_clear(uint64_t timer_id) {
    JagTimer **curr = &g_timers;
    while (*curr) {
        if ((*curr)->id == timer_id) {
            JagTimer *to_free = *curr;
            *curr = to_free->next;
            free(to_free);
            return;
        }
        curr = &(*curr)->next;
    }
}

void event_loop_run(void) {
    g_loop.running = true;

    struct epoll_event events[64];
    while (g_loop.running) {
        uint64_t now = current_time_ms();

        JagTimer **curr = &g_timers;
        while (*curr) {
            JagTimer *t = *curr;
            if (now >= t->expiry_ms) {
                if (t->cb) t->cb(t->arg);
                if (t->is_repeating) {
                    t->expiry_ms = now + t->interval_ms;
                    curr = &t->next;
                } else {
                    *curr = t->next;
                    free(t);
                }
            } else {
                curr = &t->next;
            }
        }

        JagTask *q = g_task_queue;
        while (q) {
            if (!q->completed) {
                task_resume(q);
            }
            q = q->next;
        }

        int nfds = epoll_wait(g_loop.epoll_fd, events, 64, 10);
        (void)nfds;

        bool has_work = false;
        if (g_timers) has_work = true;
        JagTask *t = g_task_queue;
        while (t) {
            if (!t->completed) { has_work = true; break; }
            t = t->next;
        }
        if (!has_work) break;
    }
}

void event_loop_stop(void) {
    g_loop.running = false;
}

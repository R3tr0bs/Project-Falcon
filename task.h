#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "heap.h"

#define MAX_TASKS 16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct task {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eflags;
    uint32_t id;
    task_state_t state;
    uint32_t* stack;
    struct task* next;
    struct task* prev;
    uint32_t priority;
    uint32_t time_slice;
    char name[32];
} task_t;

void task_init(void);
task_t* task_create(void (*entry)(void), const char* name, uint32_t priority);
void task_switch(void);
void task_yield(void);
void task_exit(void);
task_t* task_get_current(void);
void task_scheduler_enable(void);
void task_scheduler_disable(void);

// Global task array for external access
extern task_t tasks[];

#endif
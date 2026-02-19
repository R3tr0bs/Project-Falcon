#include "task.h"
#include "utils.h"
#include "ports.h"

task_t tasks[MAX_TASKS];  // Global task array
static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t task_count = 0;
static uint32_t scheduler_enabled = 0;
static uint32_t next_task_id = 1;

static void idle_task(void) {
    while (1) {
        task_yield();
    }
}

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = 0;
        tasks[i].state = TASK_TERMINATED;
        tasks[i].stack = NULL;
        tasks[i].next = NULL;
        tasks[i].prev = NULL;
    }
    
    task_t* idle = task_create(idle_task, "idle", 0);
    if (idle) {
        idle->state = TASK_READY;
        current_task = idle;
    }
}

task_t* task_create(void (*entry)(void), const char* name, uint32_t priority) {
    if (task_count >= MAX_TASKS) return NULL;
    
    task_t* task = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_TERMINATED) {
            task = &tasks[i];
            break;
        }
    }
    
    if (!task) return NULL;
    
    task->stack = (uint32_t*)kmalloc(TASK_STACK_SIZE);
    if (!task->stack) return NULL;
    
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = 10 + priority;
    
    for (int i = 0; i < 32 && name[i]; i++) {
        task->name[i] = name[i];
    }
    task->name[31] = '\0';
    
    uint32_t* stack_top = (uint32_t*)((uint8_t*)task->stack + TASK_STACK_SIZE);
    stack_top--;
    
    *stack_top = 0x202; // EFLAGS with IF set
    stack_top--;
    *stack_top = 0x08; // CS
    stack_top--;
    *stack_top = (uint32_t)entry; // EIP
    
    for (int i = 0; i < 7; i++) {
        stack_top--;
        *stack_top = 0;
    }
    
    task->esp = (uint32_t)stack_top;
    task->ebp = (uint32_t)stack_top;
    task->eax = 0;
    task->ebx = 0;
    task->ecx = 0;
    task->edx = 0;
    task->esi = 0;
    task->edi = 0;
    task->eflags = 0x202;
    
    if (!task_list) {
        task_list = task;
        task->next = task;
        task->prev = task;
    } else {
        task->next = task_list;
        task->prev = task_list->prev;
        task_list->prev->next = task;
        task_list->prev = task;
    }
    
    task_count++;
    return task;
}

// Forward declaration for task switching function
void task_switch_asm(task_t* old_task, task_t* new_task);

void task_switch(void) {
    if (!scheduler_enabled || !current_task) return;
    
    task_t* next_task = current_task->next;
    while (next_task && next_task->state != TASK_READY) {
        next_task = next_task->next;
        if (next_task == current_task) {
            next_task = NULL;
            break;
        }
    }
    
    if (!next_task) {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (tasks[i].state == TASK_READY) {
                next_task = &tasks[i];
                break;
            }
        }
    }
    
    if (next_task && next_task != current_task) {
        task_t* old_task = current_task;
        current_task = next_task;
        old_task->state = TASK_READY;
        current_task->state = TASK_RUNNING;
        task_switch_asm(old_task, current_task);
    }
}

void task_yield(void) {
    task_switch();
}

void task_exit(void) {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        if (current_task->stack) {
            kfree(current_task->stack);
            current_task->stack = NULL;
        }
        task_count--;
        task_switch();
    }
}

task_t* task_get_current(void) {
    return current_task;
}

void task_scheduler_enable(void) {
    scheduler_enabled = 1;
}

void task_scheduler_disable(void) {
    scheduler_enabled = 0;
}

// Simple task switch implementation (placeholder for assembly)
void task_switch_asm(task_t* old_task, task_t* new_task) {
    // This is a simplified implementation
    // In a real OS, this would save/restore registers and stack pointer
    // For now, we'll just update the task states
    if (old_task) {
        old_task->state = TASK_READY;
    }
    if (new_task) {
        new_task->state = TASK_RUNNING;
    }
}
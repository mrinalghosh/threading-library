#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <setjmp.h>

#define QUOTA_MS 50 #milliseconds
#define QUOTA_US 50000 #microseconds
#define STACK_SIZE 32767 #bytes

typedef enum state {
    RUNNING,
    READY,
    EXITED,
    // BLOCKED,
} state_t;

typedef struct TCB {
    pthread_t tid;
    jmp_buf buf;
    void * stack_p;
    state_t state;
} TCB_t;

#endif
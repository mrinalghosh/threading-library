#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <setjmp.h>
#include <signal.h>

#define QUOTA 50000       // microseconds
#define STACK_SIZE 32767  // bytes
#define MAX_THREADS 128
/* Register name definitions from libc
JB_RBX 0
JB_RBP 1
JB_R12 2
JB_R13 3
JB_R14 4
JB_R15 5
JB_RSP 6
JB_PC 7
*/

typedef enum state {
    RUNNING,
    READY,
    EXITED,
} state_t;

typedef struct TCB {
    pthread_t tid;
    jmp_buf buf;  // 64b[8]
    void *stack;
    state_t state;
} TCB_t;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void pthread_exit(void *value_ptr);
pthread_t pthread_self(void);
void schedule(void);

#endif
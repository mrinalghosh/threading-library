#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <setjmp.h>

#define QUOTA 50000
#define STACKSIZE 32767
#define MAXTHREADS 128

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

typedef enum State {
    RUNNING,
    READY,
    EXITED,
} State;

typedef struct TCB {
    pthread_t id;
    jmp_buf buf;
    void *stack;
    State state;
    struct TCB *next;
    struct TCB *prev;
} TCB;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

void scheduler(int signum);

#endif /* THREADS_H */
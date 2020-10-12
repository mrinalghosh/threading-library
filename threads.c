#include "threads.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ec440threads.h"

int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;

void scheduler(int signum) {
    if (!setjmp(current->buf)) {  // save current buffer
        do {
            if (current->state == RUNNING)
                current->state = READY;
            current = current->next;
        } while (current->state == EXITED);

        current->state = RUNNING;  // mark next thread - running
        longjmp(current->buf, 1);  // longjmp to next thread
    }
}

static void thread_init(void) {
    TCB *mt = (TCB *)calloc(1, sizeof(TCB));

    head = mt;     // used to keep track of where to insert
    current = mt;  // used in scheduler
    mt->next = mt;
    mt->prev = mt;  // circular singleton

    mt->stack = NULL;  // main has a REAL stack
    mt->state = RUNNING;
    mt->id = (pthread_t)thread_count;

    setjmp(mt->buf);

    ++thread_count;

    struct sigaction act;
    memset(&act, '\0', sizeof(act));  // calloc instead of static var?
    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;  // can even recieve alarm signal within handler

    if (sigaction(SIGALRM, &act, NULL) < 0) {  // this hasn't failed yet
        perror("SIGACTION");
        exit(1);
    }

    ualarm(QUOTA, QUOTA);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    if (!thread_count) {
        thread_init();
    }

    if (thread_count >= MAXTHREADS) {
        perror("max threads reached");
        exit(1);
    }

    TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));  // calloc not malloc - don't have to memset null byte

    new_thread->prev = head->prev;  // add to end of circular linked list
    head->prev->next = new_thread;
    head->prev = new_thread;
    new_thread->next = head;

    new_thread->id = (pthread_t)thread_count;
    *thread = new_thread->id;  // probably not needed - process calls pthread_self anyway

    new_thread->stack = calloc(1, STACKSIZE);  // allocate thread stack - THIS IS NEVER FREED - NOT ERRORING BUT FIX :/

    setjmp(new_thread->buf);
    new_thread->state = READY;
    new_thread->stack = new_thread->stack + STACKSIZE - 8;  // 8 from the top - stack starts from high addresses - Camden OH
    *(unsigned long *)new_thread->stack = (unsigned long)pthread_exit;

    new_thread->buf[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;
    new_thread->buf[0].__jmpbuf[JB_R13] = (unsigned long)arg;
    new_thread->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);
    new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)new_thread->stack);

    ++thread_count;
    return 0;
}

void pthread_exit(void *value_ptr) {
    current->state = EXITED;
    --thread_count;           // since tid is only unique to threads which have not exited - can reuse
    scheduler(SIGALRM);       // return to scheduler early - unfair quota for next thread?
    __builtin_unreachable();  // GNU compiler built in - never return to this function if we hit this
}

pthread_t pthread_self(void) {
    return current->id;
}
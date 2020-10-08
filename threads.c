#include "threads.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ec440threads.h"

/*TODO:
    helper function - initialize thread subsys after first pthread_create()
    decide how to enumerate thread ids - 0 to n or something else?
*/

int thread_count = 0;

static void init(void) {
    // helper function called with first thread creation
    // initialize main thread first
    TCB *m_thread = (TCB *)calloc(1, sizeof(TCB));

    // create circular list with just single element
    m_thread->next = m_thread;
    m_thread->last = m_thread;

    // initialize other tcb vars
    m_thread->stack = NULL;  // never need to look at this pointer
    m_thread->state = READY;
    m_thread->id = (pthread_t)0;

    // save jmp_buf
    setjmp(m_thread->buf);

    // increment thread count
    ++thread_count;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    /*TODO:
    Store thread ID in location ref by thread object
    execute start_routine with arg as sole argument
    if start routine returns, implicitly call pthread_exit
    deal with main thread exiting
    */
    if (!thread_count) {
        init();
    }
}

void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

void schedule(void);
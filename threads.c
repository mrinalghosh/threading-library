#include "threads.h"

// #include <pthread.h>
// #include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // for ualarm

#include "ec440threads.h"

/*TODO:
    helper function - initialize thread subsys after thread pthread_create()
    decide how to enumerate thread ids - 0 to n or something else?

    if process exits - implicitly call pthread_exit? - need to go back to scheduler?
*/

int thread_count = 0;
TCB *head = NULL;
TCB *current = head;

void scheduler(int signum) {
    // schedule should be the signal handler
    printf("inside scheduler function\n");

    // set current to next thread
    current = current->next;

    // longjump to next thread
    longjmp(current->buf, 1);
}

static void _inspect(jmp_buf buf) {
    printf(
        "RBX: 0x%08lx\nRBP: 0x%08lx\nR12: 0x%08lx\nR13: 0x%08lx\nR14: \
        0x%08lx\nR15: 0x%08lx\nSP:  0x%08lx\nPC:  0x%08lx\n",
        buf[0].__jmpbuf[JB_RBX], buf[0].__jmpbuf[JB_RBP],
        buf[0].__jmpbuf[JB_R12], buf[0].__jmpbuf[JB_R13],
        buf[0].__jmpbuf[JB_R14], buf[0].__jmpbuf[JB_R15],
        buf[0].__jmpbuf[JB_RSP], buf[0].__jmpbuf[JB_PC]);

    printf("RSPd:0x%08lx\nPCd: 0x%08lx\n",
           ptr_demangle(buf[0].__jmpbuf[JB_RSP]),
           ptr_demangle(buf[0].__jmpbuf[JB_PC]));

    // buf[0].__jmpbuf[JB_PC] = ptr_mangle((long unsigned int)win);
}

static void _init(void) {
    // helper function called with first thread creation - initializes first thread
    TCB *first_thread = (TCB *)calloc(1, sizeof(TCB));

    // create circular list with just single element
    first_thread->next = first_thread;
    first_thread->last = first_thread;

    // set head to be first_thread
    head = first_thread;

    // set current to first thread
    curr = first_thread;

    // initialize other tcb vars
    first_thread->stack = NULL;   // never need to look at this pointer
    first_thread->state = READY;  // TODO: is this RUNNING or READY
    first_thread->id = (pthread_t)thread_count;

    // save jmp_buf
    setjmp(first_thread->buf);

    // increment thread count
    ++thread_count;

    // signal handling
    struct sigaction act;
    act.sa_handler = signal_handler;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    // set repeating alarm for 50ms
    ualarm(QUOTA, QUOTA);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    /*TODO:
    NOTES:
    attr always NULL
    */

    // initialize main thread
    if (!thread_count) {
        _init();
    } else {
        // create thread and add to end of circular linked list
        TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));
        head->last->next = new_thread;
        new_thread->next = head;

        // save thread id
        new_thread->id = (pthread_t)thread_count;

        // allocate memory to stack
        new_thread->stack = calloc(1, STACK_SIZE);  // TODO: is this 1x32767 or 32767x1? - use calloc or malloc?

        // populate tcb
        setjmp(new_thread->buf);
        new_thread->state = READY;  // TODO: is this right?

        // put address of pthread_exit on top of stack
        memcpy(new_thread->stack, (long unsigned)pthread_exit, 8);

        // MOVE STACK POINTER DOWN BY 8 bytes - 64 bits
        new_thread->stack = (void *)((long unsigned *)new_thread->stack++);

        // start_thunk
        new_thread->buf[0].__jmpbuf[JB_R12] = ptr_mangle((long unsigned)start_routine);  // ->pc
        new_thread->buf[0].__jmpbuf[JB_R13] = (long unsigned)arg;                        // ->rdi
        start_thunk();

        // mangle RIP (done above) and RSP
        new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle(new_thread->stack);

        // increment thread_c
        ++thread_count;
    }

    return 0;
}

void pthread_exit(void *value_ptr) {
    // ignore value of value_ptr
    // clean up all information relating to terminating thread

    // set status to exited
    current->state = EXITED;

    // free stack
    free(current->stack);

    // don't remove from linked list for now

    // TODO: increment to next thread?????
}

pthread_t pthread_self(void) {
    return current->id;
}
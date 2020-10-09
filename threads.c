#include "threads.h"

// #include <pthread.h>
// #include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // ualarm

#include "ec440threads.h"

/*TODO:
    helper function - initialize thread subsys after thread pthread_create()
    decide how to enumerate thread ids - 0 to n or something else?

    if process exits - implicitly call pthread_exit? - need to go back to scheduler?
*/

int thread_c = 0;
TCB *head = NULL;
TCB *current = NULL;

static void inspect_jmpbuf(jmp_buf buf) {
    printf(
        "RBX: 0x%08lx\nRBP: 0x%08lx\nR12: 0x%08lx\nR13: 0x%08lx\nR14: 0x%08lx\nR15: 0x%08lx\nSP:  0x%08lx\nPC:  0x%08lx\n",
        buf[0].__jmpbuf[JB_RBX], buf[0].__jmpbuf[JB_RBP],
        buf[0].__jmpbuf[JB_R12], buf[0].__jmpbuf[JB_R13],
        buf[0].__jmpbuf[JB_R14], buf[0].__jmpbuf[JB_R15],
        buf[0].__jmpbuf[JB_RSP], buf[0].__jmpbuf[JB_PC]);

    printf("RSPd:0x%08lx\nPCd: 0x%08lx\n",
           ptr_demangle(buf[0].__jmpbuf[JB_RSP]),
           ptr_demangle(buf[0].__jmpbuf[JB_PC]));
}

void inspect_thread(TCB *thread) {
    printf("___________________________________\n");
    // printf("Thread Control Block:\nid:%08x\n", thread->id);
    inspect_jmpbuf(thread->buf);
    printf("stack:%p\nstate:%d\n", thread->stack, thread->state);
    printf("___________________________________\n");
}

void scheduler(int signum) {
    printf("inside scheduler - recieved SIGALRM\n");
    if (!setjmp(current->buf)) {  // setjmp returns zero the first time it's called
        current->state = READY;
        current = current->next;  // set current to next thread
        current->state = RUNNING;
        longjmp(current->buf, 1);  // longjump to next thread
    }
}

static void thread_init(void) {
    printf("inside init - created first thread\n");

    // helper function called with first thread creation - initializes first thread
    TCB *first = (TCB *)calloc(1, sizeof(TCB));

    // create circular list with just single element
    first->next = first;
    first->last = first;

    // set head to be first
    head = first;

    // set current to first thread
    current = first;

    // initialize other tcb vars
    first->stack = calloc(1, STACK_SIZE);  // allocate memory for stack independently
    first->state = READY;                  // TODO: is this RUNNING or READY
    first->id = (pthread_t)thread_c;

    // save jmp_buf
    setjmp(first->buf);

    // increment thread count
    ++thread_c;

    // signal handling
    struct sigaction act;

    // zero struct
    memset(&act, '\0', sizeof(act));

    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;  // can even recieve alarm signal within handler

    // if (sigaction(SIGALRM, &act, NULL) < 0) { // this hasn't failed
    //     printf("SIGALARM FAILED - BREAKING\n");
    //     perror("sigaction");
    //     exit(1);
    // }

    // doesn't fail - only run once
    sigaction(SIGALRM, &act, NULL);

    // set repeating alarm for quota micro seconds
    ualarm(QUOTA, QUOTA);

    inspect_thread(first);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    /*TODO:
    NOTES:
    attr always NULL
    */

    printf("inside pthread create\n");

    // TODO: case where 128 threads are created

    if (!thread_c) {  // initialize first thread
        thread_init();
    } else {
        // create thread and add to end of circular linked list
        TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));
        head->last->next = new_thread;
        new_thread->next = head;

        // save thread id
        new_thread->id = (pthread_t)thread_c;

        // allocate memory to stack
        new_thread->stack = calloc(1, STACK_SIZE);  // TODO: is this 1x32767 or 32767x1? - use calloc or malloc?

        // populate tcb
        setjmp(new_thread->buf);
        new_thread->state = READY;  // TODO: is this right?

        // TODO: put address of pthread_exit on top of stack
        *(long unsigned *)new_thread->stack = (long unsigned)pthread_exit;

        // MOVE STACK POINTER DOWN BY 8 bytes - 64 bits
        new_thread->stack = (void *)(new_thread->stack + 8);

        // start_thunk
        new_thread->buf[0].__jmpbuf[JB_R12] = ptr_mangle((long unsigned)start_routine);  // ->pc
        new_thread->buf[0].__jmpbuf[JB_R13] = (long unsigned)arg;                        // ->rdi
        start_thunk();

        // mangle RIP (done above) and RSP
        new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((long unsigned)new_thread->stack);

        // increment thread_c
        ++thread_c;
    }

    inspect_thread(new_thread);

    return 0;
}

void pthread_exit(void *value_ptr) {
    // ignore value of value_ptr
    // clean up all information relating to terminating thread

    printf("inside pthread exit\n");

    // set status to exited
    current->state = EXITED;

    // free stack
    free(current->stack);

    // don't remove from linked list for now
    // TODO: increment to next thread?

    // return pthread_self();
    __builtin_unreachable();
}

pthread_t pthread_self(void) {
    return current->id;
}
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

int tcount = 0;
TCB *head = NULL;
TCB *current = NULL;
// TCB *mess = NULL;

// void inspect_jmpbuf(jmp_buf buf) {
//     printf(
//         "RBX: 0x%08lx\nRBP: 0x%08lx\nR12: 0x%08lx\nR13: 0x%08lx\nR14: 0x%08lx\nR15: 0x%08lx\nSP:  0x%08lx\nPC:  0x%08lx\n",
//         buf[0].__jmpbuf[JB_RBX], buf[0].__jmpbuf[JB_RBP],
//         buf[0].__jmpbuf[JB_R12], buf[0].__jmpbuf[JB_R13],
//         buf[0].__jmpbuf[JB_R14], buf[0].__jmpbuf[JB_R15],
//         buf[0].__jmpbuf[JB_RSP], buf[0].__jmpbuf[JB_PC]);

//     printf("RSPd:0x%08lx\nPCd: 0x%08lx\n",
//            ptr_demangle(buf[0].__jmpbuf[JB_RSP]),
//            ptr_demangle(buf[0].__jmpbuf[JB_PC]));
// }

// void inspect_thread(TCB *thread) {
//     inspect_jmpbuf(thread->buf);
//     printf("stack:%p\nstate:%d\n", thread->stack, thread->state);
//     printf("next:%p\nlast:%p\n", thread->next, thread->last);
// }

void scheduler(int signum) {
    // printf("-------8<-------------[ cut here ]-------------\n");
    // printf("Scheduling: thread %d ===> thread %d\n", (unsigned)current->id, (unsigned)current->next->id);
    // inspect_thread(current);

    if (!setjmp(current->buf)) {  // setjmp returns zero the first time it's called - only runs the one time.
        current->state = READY;   // stop running current thread

        // IMPLEMENTATION WITH KEEPING EXITED THREADS IN LINKED LIST
        // do {
        //     current = current->next;  // go to next thread
        //     printf("GOING TO THREAD %d with STATE:%d\n", (unsigned)current->id, current->state);
        // } while (current->state == EXITED);
        // raise(SIGSEGV);

        current = current->next;
        current->state = RUNNING;  // set state as running
        longjmp(current->buf, 1);  // longjump to next thread
    }
}

static void thread_init(void) {
    // printf("Inside init - created main thread\n");

    // helper function called with first thread creation - initializes main thread
    TCB *mt = (TCB *)calloc(1, sizeof(TCB));

    // create circular list with just single element
    mt->next = mt;
    mt->last = mt;

    // set head and current to be mt
    head = mt;
    current = mt;

    // initialize other tcb vars
    mt->stack = NULL;     // main has its own stack
    mt->state = RUNNING;  // TODO: is this RUNNING or READY
    mt->id = (pthread_t)tcount;

    // save jmp_buf
    setjmp(mt->buf);

    // increment thread count
    ++tcount;

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

    // inspect contents of tcb
    // inspect_thread(mt);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    // printf("Inside pthread_create()\nThread count: %d\n", tcount);

    if (tcount == 0) {  // initialize single main thread
        thread_init();
    }

    if (tcount >= MAXTHREADS) {
        printf("Maximum (128) threads allocated, exiting with code 1!\n");
        exit(1);
    }

    // create thread
    TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));

    // add to end of circular linked list
    new_thread->last = head->last;
    head->last->next = new_thread;
    head->last = new_thread;
    new_thread->next = head;

    // save thread id
    new_thread->id = (pthread_t)tcount;

    // allocate memory to stack
    new_thread->stack = calloc(1, STACKSIZE);  // TODO: is this 1x32767 or 32767x1? - use calloc to prevent having to zero

    // populate tcb
    setjmp(new_thread->buf);
    new_thread->state = READY;  // TODO: is this right?

    //TODO: what to do with the 'thread' arg above?

    // TODO: put address of pthread_exit on top of stack
    new_thread->stack = new_thread->stack + STACKSIZE - 8;  // 8 from the top - stack starts from high address
    *(unsigned long *)new_thread->stack = (unsigned long)pthread_exit;

    // MOVE STACK POINTER DOWN BY 8 bytes - 64 bits
    // new_thread->stack = (void *)(new_thread->stack + 8);

    // mangle RIP/PC and RSP
    new_thread->buf[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;
    new_thread->buf[0].__jmpbuf[JB_R13] = (unsigned long)arg;                     // ->rdi
    new_thread->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);  // ->pc - from camden OH
    new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)new_thread->stack);

    // increment tcount
    ++tcount;

    // inspect_thread(new_thread);

    return 0;
}

void pthread_exit(void *value_ptr) {
    // ignore value of value_ptr
    // clean up all information relating to terminating thread

    // set status to exited
    current->state = EXITED;

    // printf("THREAD %d EXITED ---- STATE WAS SET TO %d\n", (unsigned)current->id, current->state);

    // free stack
    // if (!current->stack)
    //     free(current->stack);

    // removing from linked list - might need to free the memory from the TCB later - maybe maintain a secondary linked list of things to delete?
    current->last->next = current->next;
    current->next->last = current->last;

    // TODO: create new linked list to clean up when main thread exits
    // TCB *temp = mess;  // linked list LINE - only using next - first time mess is just null
    // while (!temp) { // find last position of mess
    //     temp = temp->next;
    // }

    --tcount;

    // return pthread_self();
    scheduler(SIGALRM); // return to scheduler early
    __builtin_unreachable();  // GNU compiler built in - never return to this function if we hit this
}

pthread_t pthread_self(void) {
    return current->id;
}
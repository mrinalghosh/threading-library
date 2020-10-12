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

int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;

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
//     printf("next:%p\nlast:%p\n", thread->next, thread->prev);
// }

void scheduler(int signum) {
    // printf("-------8<-------------[ cut here ]-------------\n");
    // printf("Scheduling: thread %d ===> thread %d\n", (unsigned)current->id, (unsigned)current->next->id);
    // inspect_thread(current);

    if (!setjmp(current->buf)) {  // save current buffer
        // current->state = READY;   // stop running current thread

        // IMPLEMENTATION WITH KEEPING EXITED THREADS IN LINKED LIST
        do {
            if (current->state == RUNNING)
                current->state = READY;
            current = current->next;  // go to next thread
            // printf("GOING TO THREAD %d with STATE:%d\n", (unsigned)current->id, current->state);
        } while (current->state == EXITED);

        // current = current->next;   // at this point we have lost track of the current thread - WITHOUT explicitly freeing it :(
        current->state = RUNNING;  // set state as running
        longjmp(current->buf, 1);  // longjmp to next thread
    }
}

static void thread_init(void) {
    TCB *mt = (TCB *)calloc(1, sizeof(TCB));

    // set head and current to be main thread
    head = mt;
    current = mt;

    // initialize TCB
    mt->next = mt;
    mt->prev = mt;     // circular list with single element
    mt->stack = NULL;  // main has a REAL stack
    mt->state = RUNNING;
    mt->id = (pthread_t)thread_count;

    setjmp(mt->buf);

    ++thread_count;

    // signal handling
    struct sigaction act;
    memset(&act, '\0', sizeof(act));  // zero struct

    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;  // can even recieve alarm signal within handler

    if (sigaction(SIGALRM, &act, NULL) < 0) {  // this hasn't failed yet
        perror("SIGACTION");
        exit(1);
    }

    ualarm(QUOTA, QUOTA);

    // inspect_thread(mt);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    // printf("Inside pthread_create()\nThread count: %d\n", thread_count);

    if (thread_count == 0) {  // initialize single main thread
        thread_init();
    }

    if (thread_count >= MAXTHREADS) {
        perror("maximum threads created!");
        exit(1);
    }

    // create thread
    TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));

    // add to end of circular linked list
    new_thread->prev = head->prev;
    head->prev->next = new_thread;
    head->prev = new_thread;
    new_thread->next = head;

    // save thread id
    new_thread->id = (pthread_t)thread_count;
    *thread = new_thread->id;

    // allocate memory to stack
    new_thread->stack = calloc(1, STACKSIZE);

    // populate tcb
    setjmp(new_thread->buf);
    new_thread->state = READY;

    new_thread->stack = new_thread->stack + STACKSIZE - 8;  // 8 from the top - stack starts from high address - camden OH
    *(unsigned long *)new_thread->stack = (unsigned long)pthread_exit;

    // mangle RIP/PC and RSP
    new_thread->buf[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;
    new_thread->buf[0].__jmpbuf[JB_R13] = (unsigned long)arg;                     // ->rdi
    new_thread->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);  // ->pc - from camden OH
    new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)new_thread->stack);

    ++thread_count;

    return 0;
}

void pthread_exit(void *value_ptr) {
    current->state = EXITED;

    // pop from circ linked list
    // current->prev->next = current->next;
    // current->next->prev = current->prev;

    // TODO: clean up by freeing exited threads - maybe maintain a secondary linked list of things to delete when thread count hits 1? main ends?
    // free(current->stack); // "free(): invalid pointer"

    --thread_count;  // since tid is only unique to threads which have not exited - can reuse

    // exit(0) once prev thread has exited - IF IT DOESN'T PERROR or SEGFAULT earlier, the exit code is zero automatically

    scheduler(SIGALRM);       // return to scheduler early
    __builtin_unreachable();  // GNU compiler built in - never return to this function if we hit this
}

pthread_t pthread_self(void) {
    return current->id;
}
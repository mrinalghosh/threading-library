#include "threads.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ec440threads.h"

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

void schedule(void);
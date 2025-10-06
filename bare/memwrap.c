//#define _GNU_SOURCE
//#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <pthread.h>

#include "memwrap.h"

static struct memstats  memstats;

typedef void *(*malloc_t)(size_t);
typedef void (*free_t)(void *);
typedef void *(*calloc_t)(size_t, size_t);
typedef void *(*realloc_t)(void *, size_t);

void memstats_zero() {
    memstats.total_allocs = 0;
    memstats.total_allocated = 0;
    memstats.total_frees = 0;
    memstats.total_freed = 0;
}

struct memstats *memstats_get() {
    return &memstats;
}

extern void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    void *p = __real_malloc(size);

    memstats.total_allocs++;
    memstats.total_allocated += size;
    return p;
}

extern void __real_free(void *ptr);
void __wrap_free(void *ptr) {
    if (ptr) {
        // TODO: how do we get the size of the freed block?
        memstats.total_frees++;
    }

    __real_free(ptr);
}

extern void *__real_calloc(size_t nmemb, size_t size);
void *__wrap_calloc(size_t nmemb, size_t size) {
    void *p = __real_calloc(nmemb, size);

    memstats.total_allocs++;
    memstats.total_allocated += nmemb * size;
    return p;
}

extern void *__real_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size) {
    void *p = __real_realloc(ptr, size);

    memstats.total_allocated += size;
    memstats.total_allocs++;         // not realy, it's a resize?
    return p;
}

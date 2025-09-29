#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "memwrap.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

void *malloc(size_t size) {
    static malloc_t real_malloc = NULL;
    if (!real_malloc)
        real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");

    void *p = real_malloc(size);

    pthread_mutex_lock(&lock);
    memstats.total_allocs++;
    memstats.total_allocated += size;
    pthread_mutex_unlock(&lock);

    return p;
}

void free(void *ptr) {
    static free_t real_free = NULL;
    if (!real_free)
        real_free = (free_t)dlsym(RTLD_NEXT, "free");

    if (ptr) {
        // (optional) track size if you keep a map of pointer->size
        pthread_mutex_lock(&lock);
        // TODO: how do we get the size of the freed block?
        memstats.total_frees++;
        pthread_mutex_unlock(&lock);
    }

    real_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
    static calloc_t real_calloc = NULL;
    if (!real_calloc)
        real_calloc = (calloc_t)dlsym(RTLD_NEXT, "calloc");

    void *p = real_calloc(nmemb, size);

    pthread_mutex_lock(&lock);
    memstats.total_allocs++;
    memstats.total_allocated += nmemb * size;
    pthread_mutex_unlock(&lock);

    return p;
}

void *realloc(void *ptr, size_t size) {
    static realloc_t real_realloc = NULL;
    if (!real_realloc)
        real_realloc = (realloc_t)dlsym(RTLD_NEXT, "realloc");

    void *p = real_realloc(ptr, size);

    pthread_mutex_lock(&lock);
    memstats.total_allocated += size;
    memstats.total_allocs++;         // not realy, it's a resize?
    pthread_mutex_unlock(&lock);

    return p;
}

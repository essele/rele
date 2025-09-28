#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static size_t total_allocs = 0;
static size_t total_allocated = 0;
static size_t total_freed = 0;
static size_t total_frees = 0;

typedef void *(*malloc_t)(size_t);
typedef void (*free_t)(void *);
typedef void *(*calloc_t)(size_t, size_t);
typedef void *(*realloc_t)(void *, size_t);

void *malloc(size_t size) {
    static malloc_t real_malloc = NULL;
    if (!real_malloc)
        real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");

    void *p = real_malloc(size);

    pthread_mutex_lock(&lock);
    total_allocs++;
    total_allocated += size;
    fprintf(stderr, "[malloc] %zu bytes -> %p (total=%zu)\n", size, p, total_allocated - total_freed);
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
        total_frees++;
        fprintf(stderr, "[free] %p\n", ptr);
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
    total_allocs++;
    total_allocated += nmemb * size;
    fprintf(stderr, "[calloc] %zu x %zu -> %p (total=%zu)\n", nmemb, size, p, total_allocated - total_freed);
    pthread_mutex_unlock(&lock);

    return p;
}

void *realloc(void *ptr, size_t size) {
    static realloc_t real_realloc = NULL;
    if (!real_realloc)
        real_realloc = (realloc_t)dlsym(RTLD_NEXT, "realloc");

    void *p = real_realloc(ptr, size);

    pthread_mutex_lock(&lock);
    total_allocated += size;
    total_allocs++;         // not realy, it's a resize?
    fprintf(stderr, "[realloc] %p resized to %zu -> %p (total=%zu)\n", ptr, size, p, total_allocated - total_freed);
    pthread_mutex_unlock(&lock);

    return p;
}

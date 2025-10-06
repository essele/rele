//#define _GNU_SOURCE
//#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <pthread.h>

#include "memwrap.h"


size_t __total_allocs;
size_t __total_allocated;
unsigned char *__stack_low;


typedef void *(*malloc_t)(size_t);
typedef void (*free_t)(void *);
typedef void *(*calloc_t)(size_t, size_t);
typedef void *(*realloc_t)(void *, size_t);


extern void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    void *p = __real_malloc(size);

    __total_allocs++;
    __total_allocated += size;
    return p;
}

extern void __real_free(void *ptr);
void __wrap_free(void *ptr) {
    __real_free(ptr);
}

extern void *__real_calloc(size_t nmemb, size_t size);
void *__wrap_calloc(size_t nmemb, size_t size) {
    void *p = __real_calloc(nmemb, size);

    __total_allocs++;
    __total_allocated += nmemb * size;
    return p;
}

extern void *__real_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size) {
    void *p = __real_realloc(ptr, size);

    __total_allocated += size;
    __total_allocs++;         // not realy, it's a resize?
    return p;
}

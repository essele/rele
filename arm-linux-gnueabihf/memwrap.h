#ifndef __MEMWRAP_H
#define __MEMWRAP_H

/*
 * The include file for memwrap so that we can get at the stats
 */
#include <stdlib.h>
#include <string.h>

extern size_t __total_allocs;
extern size_t __total_allocated;
extern unsigned char *__stack_low;


struct memstats {
    size_t total_allocs;
    size_t total_allocated;
    size_t total_stack;
};

#define ASSUMED_STACK_SIZE      (1024 * 128)


static inline unsigned char *get_sp(void) {
    unsigned char *sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

static inline void memstats_zero() {
    __total_allocated = 0;
    __total_allocs = 0;

    // Now a stack fill....
    unsigned char *sp = get_sp();
    size_t len = (size_t)(sp - __stack_low);
    memset(__stack_low, 0xAA, len);
}

/* Measure how much of the filled area has been touched */
static inline size_t stack_usage(void) {
    unsigned char *sp = get_sp();
    unsigned char *p  = __stack_low;
    while (p < sp && *p == 0xAA)
        p++;
    return (size_t)(sp - p);
}

static inline void memstats_get(struct memstats *m) {
    m->total_allocated = __total_allocated;
    m->total_allocs = __total_allocs;
    m->total_stack = stack_usage();
}


static inline void memstats_init(void) {
    __stack_low = get_sp() - ASSUMED_STACK_SIZE;
}

#endif
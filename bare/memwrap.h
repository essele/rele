/*
 * The include file for memwrap so that we can get at the stats
 */
#include <stdlib.h>

struct memstats {
    size_t total_allocs;
    size_t total_allocated;
    size_t total_freed;
    size_t total_frees;
};

extern void memstats_zero();
extern struct memstats *memstats_get();



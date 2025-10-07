#include <stdio.h>
#include <stdlib.h>
#include "memwrap.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "shim.h"
#include "pmu.h"


// Ensure the engines are all visible...
extern struct engine pcre_engine;
extern struct engine newlib_engine;
extern struct engine tre_engine;
extern struct engine slre_engine;
extern struct engine tiny_regex_c_engine;
extern struct engine subreg_engine;
extern struct engine rele_engine;


struct engine *engines[] = {
    &rele_engine,
    &newlib_engine,
    &pcre_engine,
    &tre_engine,
    &slre_engine,
    &tiny_regex_c_engine,
    &subreg_engine,
    NULL
};

#define MAX_ALLOWED_CYCLES (10000000)

uint32_t time_compile(struct engine *eng, char *regex, int flags) {
    uint32_t start, end, delta;
    uint64_t sum = 0;
    int32_t i = 0;

    for (int i = 0; i < 10; i++) {
        eng->compile(regex);
        eng->free();
    }

    while (i < 100000000) {
        start = read_cycle_counter();
        eng->compile(regex);
        end = read_cycle_counter();
        eng->free();
        if (end > start) {
            delta = end - start;
        } else {
            delta = (UINT32_MAX - start + 1) + end;
        }
        sum += delta;

        if (sum > MAX_ALLOWED_CYCLES) break;
        i++;
    }
    return (uint32_t)(sum/i);
}

uint32_t time_match(struct engine *eng, char *regex, char *text, int flags) {
    uint32_t start, end, delta;
    uint64_t sum = 0;
    int32_t i = 0;

    eng->compile(regex);

    while (i < 100000000) {
        start = read_cycle_counter();
        eng->match(text);
        end = read_cycle_counter();
        if (end > start) {
            delta = end - start;
        } else {
            delta = (UINT32_MAX - start + 1) + end;
        }
        sum += delta;

        if (sum > MAX_ALLOWED_CYCLES) break;
        i++;
    }
    eng->free();
    return (uint32_t)(sum/i);
}

int test_compile(struct engine *eng, char *regex, int flags, struct memstats *mem) {
    int rc;

    memstats_zero();
    rc = eng->compile(regex);
    memstats_get(mem);
    eng->free();
    return rc;
}

int test_match(struct engine *eng, char *regex, char *text, int flags, struct memstats *mem) {
    int rc;

    memstats_zero();
    rc = eng->compile(regex);
    // TODO: rc checking
    rc = eng->match(text);
    memstats_get(mem);
    eng->free();
    return rc;
}

int main(void) {
    struct memstats mem;

    //char *regex = "a?a?a?a?a?a?aaaaaa";
    //char *text = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
//    char *regex = "a?a?a?a?aaaaaaa";
    //char *text = "aaaaaaaaaaa";

    char *regex = "..(a(b(c(d(e(hello))))))..";
    char *text = "fredabcdehellookblah";

    memstats_init();
    enable_pmu();

    printf("Hello from bare metal!\n");


    memstats_zero();
    memstats_get(&mem);    
    fprintf(stderr, "Stack usage: %u\n", mem.total_stack);

    memstats_get(&mem);    
    fprintf(stderr, "Stack usage: %u\n", mem.total_stack);

    memstats_zero();
    memstats_get(&mem);    
    fprintf(stderr, "Stack usage: %u\n", mem.total_stack);

    struct engine **ep = engines;
    while (*ep) {
        struct engine *eng = *ep;
        struct memstats mem;

        fprintf(stderr, "Engine: %s\n", eng->name);

        int compile_rc = test_compile(eng, regex, 0, &mem);
        fprintf(stderr, "Engine: %s compile=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                    compile_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);

        int match_rc = test_match(eng, regex, text, 0, &mem);
        fprintf(stderr, "Engine: %s match=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                    match_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);


        uint32_t compile_time = time_compile(eng, regex, 0);
        uint32_t match_time = time_match(eng, regex, text, 0);
        fprintf(stderr, "Engine: %s (compile=%lu match=%lu)\n", eng->name, compile_time, match_time);

        ep++;
    }


    printf("here\n");

    return 0;
}

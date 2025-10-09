#include <stdio.h>
#include <stdlib.h>
#include "memwrap.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "shim.h"

#include "test.h"


// Ensure the engines are all visible...
extern struct engine pcre_engine;
extern struct engine libc_engine;
extern struct engine newlib_engine;
extern struct engine re2_engine;
extern struct engine tre_engine;
extern struct engine slre_engine;
extern struct engine tiny_regex_c_engine;
extern struct engine subreg_engine;
extern struct engine rele_engine;


struct engine *engines[] = {
    &rele_engine,
    &libc_engine,
    &newlib_engine,
    &pcre_engine,
    &re2_engine,
    &tre_engine,
    &slre_engine,
    &tiny_regex_c_engine,
    &subreg_engine,
    NULL
};

/**
 * Work out the difference between two struct timespecs in ns
 */
struct timespec diff_timespec(struct timespec start, struct timespec end) {
    struct timespec delta;

    if ((end.tv_nsec - start.tv_nsec) < 0) {
        delta.tv_sec  = end.tv_sec - start.tv_sec - 1;
        delta.tv_nsec = 1000000000L + end.tv_nsec - start.tv_nsec;
    } else {
        delta.tv_sec  = end.tv_sec - start.tv_sec;
        delta.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return delta;
}

/**
 * Convert a delta timespec to a number of ms, us, and ns
 */
uint64_t timespec_to_ms(struct timespec t) {
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
uint64_t timespec_to_us(struct timespec t) {
    return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
uint64_t timespec_to_ns(struct timespec t) {
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

/**
 * Routines that time the execution of compiling and matching
 */
#define MAX_ALLOWED_NS (1000 * 1000 * 1000 * 2)
#define MAX_ITERATIONS (200000)

uint32_t time_compile(struct engine *eng, char *regex, int flags) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    // Just to warm the cache up ...
    for (i = 0; i < 2; i++) {
        eng->compile(regex);
        eng->free();
    }

    i = 0;
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->compile(regex);
        clock_gettime(CLOCK_MONOTONIC, &end);
        eng->free();
        sum += timespec_to_ns(diff_timespec(start, end));
        i++;
    }
    return (uint32_t)(sum/i);
}

uint32_t time_match(struct engine *eng, char *regex, char *text, int flags) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    eng->compile(regex);
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->match(text);
        clock_gettime(CLOCK_MONOTONIC, &end);
        sum += timespec_to_ns(diff_timespec(start, end));
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

int main(int argc, char *argv[]) {
    memstats_init();

    printf("Hello!\n");

    const struct testcase **tcp = cases;
    while (*tcp) {
        const struct testcase *test = *tcp++;

        char *regex = test->regex;
        char *text = test->text;

        fprintf(stderr, "Test: %s/%s\n", test->group, test->name);
        fprintf(stderr, "Regex: %s\n", test->regex);
        if (strlen(test->text) > 100) {
            fprintf(stderr, "Text: (long, %d chars)\n", (int)strlen(test->text));
        } else {
            fprintf(stderr, "Text: %s\n", test->text);
        }

        struct engine **ep = engines;
        while (*ep) {
            struct engine *eng = *ep;
            struct memstats mem;
            int compile_rc, match_rc;
            uint32_t compile_time, match_time;

            fprintf(stderr, "Engine: %s\n", eng->name);

            compile_rc = test_compile(eng, regex, 0, &mem);
            fprintf(stderr, "Engine: %s compile=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                        compile_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);

            if (compile_rc == 1) {
                match_rc = test_match(eng, regex, text, 0, &mem);
                fprintf(stderr, "Engine: %s match=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                            match_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);
            }

            if (compile_rc ==1) {
                compile_time = time_compile(eng, regex, 0);
                match_time = time_match(eng, regex, text, 0);
                fprintf(stderr, "Engine: %s (compile=%u match=%u)\n", eng->name, compile_time, match_time);
            }
            ep++;
        }
    }

    printf("here\n");

    return 0;
}

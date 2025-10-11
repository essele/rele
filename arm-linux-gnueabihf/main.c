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
#define MAX_ALLOWED_NS (1000UL * 1000 * 1000 * 5)
#define MAX_ITERATIONS (200000)

uint32_t time_compile(struct engine *eng, char *regex, int flags) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    // Just to warm the cache up ...
    for (i = 0; i < 2; i++) {
        eng->compile(regex, flags);
        eng->free();
    }

    i = 0;
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->compile(regex, flags);
        clock_gettime(CLOCK_MONOTONIC, &end);
        eng->free();
        sum += timespec_to_ns(diff_timespec(start, end));
        i++;
    }
    return (uint32_t)(sum/i);
}

uint32_t time_match(struct engine *eng, char *regex, int cflags, char *text, int mflags) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    eng->compile(regex, cflags);
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->match(text, mflags);
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
    rc = eng->compile(regex, flags);
    memstats_get(mem);
    eng->free();
    return rc;
}

int test_match(struct engine *eng, char *regex, int cflags, char *text, int mflags, struct memstats *mem) {
    int rc;

    memstats_zero();
    rc = eng->compile(regex, cflags);
    // TODO: rc checking
    rc = eng->match(text, mflags);
    memstats_get(mem);
    eng->free();
    return rc;
}

/**
 * We accept arguments that are lists of things (comma separated) so this
 * routine will see if the item is in the list.
 * 
 * If the list is "all", then everything matches
 */
int is_in(char *item, char *list) {
    if (strcmp(list, "all") == 0) return 1;

    char *p = strstr(list, item);
    if (!p) return 0;

    // It's there, but needs to be full, so the beginning and end need to match
    // a full item.
    if (p == list || p[-1] == ',') {
        // start of list, or start of item, ok.
        int len = strlen(item);
        if (p[len] == 0 || p[len] == ',') {
            // end of list, or end of item, so also ok
            return 1;
        }
    }
    return 0;
}


int main(int argc, char *argv[]) {
    memstats_init();

    printf("Hello!\n");

    // Preparation for config
    char    *cf_groups = "all";
    char    *cf_tests = "all";
    char    *cf_engines = "all";
    int     cf_show_matches = 0;
    int     cf_build_tree = 0;
    int     cf_one = 0;

    // Some rudimentary argument processing
    int args = argc;
    char **ap = &argv[1];
    while (args && *ap) {
        char *arg = *ap;
        fprintf(stderr, "Arg is: %s\n", *ap);
        
        if (strcmp(arg, "-g") == 0 && args > 1) {
            cf_groups = *++ap;
        } else if (strcmp(arg, "-t") == 0 && args > 1) {
            cf_tests = *++ap;
        } else if (strcmp(arg, "-e") == 0 && args > 1) {
            cf_engines = *++ap;
        } else if (strcmp(arg, "-r") == 0) {
            cf_show_matches = 1;
        } else if (strcmp(arg, "-tree") == 0) {
            cf_build_tree = 1;
        } else if (strcmp(arg, "-1") == 0) {
            cf_one = 1;
        }
        ap++;
        args--;
    }

    const struct testcase **tcp = cases;
    while (*tcp) {
        const struct testcase *test = *tcp++;

        if (!is_in(test->group, cf_groups)) continue;
        if (!is_in(test->name, cf_tests)) continue;

        fprintf(stderr, "Test: %s/%s\n", test->group, test->name);
        fprintf(stderr, "Regex: %s\n", test->regex);
        if (strlen(test->text) > 100) {
            fprintf(stderr, "Text: (long, %d chars)\n", (int)strlen(test->text));
        } else {
            fprintf(stderr, "Text: %s\n", test->text);
        }

        struct engine **ep = engines;
        while (*ep) {
            struct engine *eng = *ep++;
            struct memstats mem;
            int compile_rc, match_rc;
            uint32_t compile_time, match_time;

            if (!is_in(eng->name, cf_engines)) continue;

            fprintf(stderr, "Engine: %s\n", eng->name);

            compile_rc = test_compile(eng, test->regex, test->cflags, &mem);
            fprintf(stderr, "Engine: %s compile=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                        compile_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);

            if (compile_rc == 1) {
                match_rc = test_match(eng, test->regex, test->cflags, test->text, 0, &mem);
                fprintf(stderr, "Engine: %s match=(rc=%d, allocs=%d, allocated=%d, stack=%d)\n", eng->name, 
                                            match_rc, mem.total_allocs, mem.total_allocated, mem.total_stack);

                if (match_rc && cf_show_matches) {
                    // We need to run another compile/match as we will have been freed by the above...
                    eng->compile(test->regex, test->cflags);
                    eng->match(test->text, test->mflags);

                    int res_groups = eng->res_count();
                    int max = (test->groups > res_groups ? test->groups : res_groups);
                    for (int i=0; i < max; i++) {
                        int tso = (i < test->groups ? test->res[i].so : -1);
                        int teo = (i < test->groups ? test->res[i].eo : -1);
                        int eso = (i < res_groups ? eng->res_so(i) : -1);
                        int eeo = (i < res_groups ? eng->res_eo(i) : -1);
                        char *status = ((tso == eso) && (teo == eeo)) ? "OK" : "FAIL";
                        fprintf(stderr, "\tExpected: %d: (%d, %d) got (%d, %d) -- %s\n", i, tso, teo, eso, eeo, status);
                    }

                    eng->free();
                }
            }

            if (!cf_one) {
                if (compile_rc ==1) {
                    compile_time = time_compile(eng, test->regex, test->cflags);
                    match_time = time_match(eng, test->regex, test->cflags, test->text, 0);
                    fprintf(stderr, "Engine: %s (compile=%u match=%u)\n", eng->name, compile_time, match_time);
                }
            }
        }
    }

    printf("here\n");

    return 0;
}

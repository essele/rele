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
 * Results for a given test...
 */
struct results {
    int         compile_stack;          // stack usage
    int         compile_allocs;         // how many allocs
    int         compile_allocated;      // how much allocated
    uint64_t    compile_time;           // ns to compile
    int         compile_rc;             // return value from compile
    int         compile_pass;           // did we compile as expected?

    int         match_stack;            // stack usage
    int         match_allocs;           // how many allocs
    int         match_allocated;        // how much allocated
    uint64_t    match_time;             // ns to match
    int         match_rc;               // return value from match
    int         match_resok;            // results match expectation
    int         match_pass;             // did we match as expected? (inc groups)


};



/**
 * Routines that time the execution of compiling and matching
 */
#define MAX_ALLOWED_NS (1000UL * 1000 * 1000 * 5)
#define MAX_ITERATIONS (200000)

int time_compile(struct engine *eng, const struct testcase *test, struct results *res) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    // Just to warm the cache up ...
    for (i = 0; i < 2; i++) {
        eng->compile(test->regex, test->cflags);
        eng->free();
    }

    i = 0;
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->compile(test->regex, test->cflags);
        clock_gettime(CLOCK_MONOTONIC, &end);
        eng->free();
        sum += timespec_to_ns(diff_timespec(start, end));
        i++;
    }
    res->compile_time = (sum/i);
    return 1;
}

int time_match(struct engine *eng, const struct testcase *test, struct results *res) {
    struct timespec start, end;
    uint64_t sum = 0;
    int32_t i = 0;

    if (!eng->compile(test->regex, test->cflags)) {
        fprintf(stderr, "Compile failed\n");
        return 0;
    }
    while (sum < MAX_ALLOWED_NS && i < MAX_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        eng->match(test->text, test->mflags);
        clock_gettime(CLOCK_MONOTONIC, &end);
        sum += timespec_to_ns(diff_timespec(start, end));
        i++;
    }
    eng->free();
    res->match_time = (sum/i);
    return 1;
}

/**
 * Test compilation, returns 1 on success, < 1 on failure.
 * Compile_pass will be ok if COMPFAIL is set.
 */
int test_compile(struct engine *eng, const struct testcase *test, struct results *res) {
    int rc;
    struct memstats mem;

    memstats_zero();
    rc = eng->compile(test->regex, test->cflags);
    memstats_get(&mem);
    eng->free();

    res->compile_stack = mem.total_stack;
    res->compile_allocs = mem.total_allocs;
    res->compile_allocated = mem.total_allocated;
    res->compile_rc = rc;

    if (test->error & E_COMPFAIL) {
        res->compile_pass = (rc == 1 ? 0 : 1);
    } else {
        res->compile_pass = (rc == 1 ? 1 : 0);
    }
    return rc;
}

int test_match(struct engine *eng, const struct testcase *test, struct results *res, int verbose) {
    int rc;
    struct memstats mem;
    int results_ok = 0;

    memstats_zero();
    rc = eng->compile(test->regex, test->cflags);
    // TODO: rc checking
    rc = eng->match(test->text, test->mflags);
    memstats_get(&mem);

    if (rc == 1) {
        int res_groups = eng->res_count();
        int max = (test->groups > res_groups ? test->groups : res_groups);
        results_ok = 1;
        for (int i=0; i < max; i++) {
            int tso = (i < test->groups ? test->res[i].so : -1);
            int teo = (i < test->groups ? test->res[i].eo : -1);
            int eso = (i < res_groups ? eng->res_so(i) : -1);
            int eeo = (i < res_groups ? eng->res_eo(i) : -1);
            char *status = ((tso == eso) && (teo == eeo)) ? "OK" : "FAIL";
            if (verbose) {
                fprintf(stderr, "\tExpected: %d: (%d, %d) got (%d, %d) -- %s\n", i, tso, teo, eso, eeo, status);
            }
            if ((tso != eso) || (teo != eeo)) results_ok = 0;
        }
    }
    eng->free();

    res->match_stack = mem.total_stack;
    res->match_allocs = mem.total_allocs;
    res->match_allocated = mem.total_allocated;
    res->match_rc = rc;
    res->match_resok = results_ok;

    if (test->error & E_MATCHFAIL) {
        res->match_pass = (rc == 1 ? 0 : 1);
    } else {
        res->match_pass = (rc == 1 ? 1 : 0);
        if (!results_ok) res->match_pass = 0;
    }
    return res->match_pass;
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

    enum {
        MODE_REGRESSION,
        MODE_CSV,
        MODE_NORMAL,
    };

    // Preparation for config
    char    *cf_groups = "all";
    char    *cf_tests = "all";
    char    *cf_engines = "all";
    int     cf_show_matches = 0;
    int     cf_build_tree = 0;
    int     cf_one = 0;
//    int     cf_regression = 0;
    int     cf_mode = MODE_NORMAL;

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
        } else if (strcmp(arg, "-R") == 0) {
            cf_mode = MODE_REGRESSION;
        } else if (strcmp(arg, "-c") == 0) {
            cf_mode = MODE_CSV;
        }
        ap++;
        args--;
    }

    if (cf_mode == MODE_CSV) {
            printf("engine,group,name,");
            printf("compile_pass,compile_rc,compile_stack,compile_allocs,compile_allocated,compile_time,");
            printf("match_pass,match_rc,match_results,match_stack,match_allocs,match_allocated,match_time");
            printf("\n");
    }

    const struct testcase **tcp = cases;
    while (*tcp) {
        const struct testcase *test = *tcp++;

        if (!is_in(test->group, cf_groups)) continue;
        if (!is_in(test->name, cf_tests)) continue;

        if (cf_mode == MODE_NORMAL) {
            fprintf(stderr, "Test: %s/%s\n", test->group, test->name);
            fprintf(stderr, "Regex: %s\n", test->regex);
            if (strlen(test->text) > 100) {
                fprintf(stderr, "Text: (long, %d chars)\n", (int)strlen(test->text));
            } else {
                fprintf(stderr, "Text: %s\n", test->text);
            }
        };

        struct engine **ep = engines;
        while (*ep) {
            struct engine *eng = *ep++;
            struct results res;

            if (!is_in(eng->name, cf_engines)) continue;

            if (cf_mode == MODE_NORMAL) {
                fprintf(stderr, "Engine: %s\n", eng->name);
            }
            // Make sure we start with all zero results...
            memset(&res, 0, sizeof(struct results));

            if (!test_compile(eng, test, &res)) goto results;
            if (test->error & E_COMPFAIL) goto results;
            if (res.compile_pass == 0) goto results;
            if (!test_match(eng, test, &res, cf_show_matches)) goto results;

            if (!cf_one) {
                time_compile(eng, test, &res);
                time_match(eng, test, &res);
            }


results:
            if (cf_mode == MODE_REGRESSION) {
                fprintf(stderr, "%s/%s - %s\n", test->group, test->name, res.match_pass ? "PASS" : "FAIL");
            } else if (cf_mode == MODE_CSV) {
                printf("%s,%s,%s,%s,%d,%d,%d,%d,%llu,%s,%d,%s,%d,%d,%d,%llu\n",
                        eng->name,
                        test->group, test->name,
                        res.compile_pass ? "PASS" : "FAIL",
                        res.compile_rc, res.compile_stack, res.compile_allocs, res.compile_allocated, res.compile_time,
                        res.match_pass ? "PASS" : "FAIL",
                        res.match_rc, res.match_resok ? "OK" : "FAIL",
                        res.match_stack, res.match_allocs, res.match_allocated, res.match_time);

            } else {
                fprintf(stderr, "Compile status: %s (rc=%d)\n", res.compile_pass ? "PASS" : "FAIL", res.compile_rc);
                fprintf(stderr, "Compile memory: stack [%d], allocs [%d], allocated [%d]\n", 
                                                res.compile_stack, res.compile_allocs, res.compile_allocated);
                if (!cf_one) {
                    fprintf(stderr, "Compile time:   %llu\n", res.compile_time);
                }
                if (res.compile_pass == 1 && res.compile_rc == 1) {
                    fprintf(stderr, "Match status:   %s (rc=%d) (res=%s)\n", res.match_pass ? "PASS" : "FAIL", res.match_rc,
                                                                                                    res.match_resok ? "OK" : "FAIL");
                    fprintf(stderr, "Match memory:   stack [%d], allocs [%d], allocated [%d]\n", 
                                                    res.match_stack, res.match_allocs, res.match_allocated);
                    if (!cf_one) {
                        fprintf(stderr, "Match time:     %llu\n", res.match_time);
                    }
                }
            }
        }
    }
    exit(0);
}

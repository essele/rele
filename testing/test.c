
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
extern char **environ;

#include <time.h>

#include <regex.h>

#include "test.h"
#include "memwrap.h"
#include "stack.h"

void (*memstats_zero)(void);
struct memstats *(*memstats_get)(void);

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
uint64_t timespec_to_ms(struct timespec t) {
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
uint64_t timespec_to_us(struct timespec t) {
    return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}


/**
 * Given a name, find the relevant test case...
 */
int find_test_index(char *name) {
    const struct testcase *t;
    int i = 0;

    while ((t = cases[i])) {
        if (strcmp((t)->name, name) == 0) return i;
        i++;
    }
    return -1;
}

const struct testcase *find_named_test(char *name) {
    int i = 0;
    if (name) i = find_test_index(name);
    if (i < 0) return NULL;
    return cases[i];
}

const struct testcase *find_next_test(char *name) {
    if (!name) return cases[0];

    int i = find_test_index(name);
    if (i < 0) return NULL;
    return cases[i+1];
}


extern struct engine funcs_libc;
extern struct engine funcs_rele;
extern struct engine funcs_pcre;
extern struct engine funcs_re2;

struct engine *engines[] = {
    &funcs_libc,
    &funcs_rele,
    &funcs_pcre,
    &funcs_re2,
    NULL
};


#define     TEST_OK                 0
#define     TEST_COMPILE_FAIL       1
#define     TEST_MATCH_FAIL         2
#define     TEST_GROUPNO_WRONG      3
#define     TEST_RESULTS_WRONG      4

char *errors[] = {
    "OK",
    "Compile Failed",
    "Match Failed",
    "Group Count Incorrect",
    "Results Incorrect",
};

int main(int argc, char *argv[]) {
    int skip_preload = 0;

    stack_init_bounds();

    if (argc >= 2 && strcmp(argv[1], "no") == 0) { skip_preload = 1; }

    // Make sure we are properly wrapped....
    if (!skip_preload) {
        if (!getenv("MEMWRAP_LOADED")) {
            setenv("LD_PRELOAD", "./memwrap.so", 1);
            setenv("MEMWRAP_LOADED", "1", 1);
            execvpe(argv[0], argv, environ);
            perror("execvpe");  // only reached if exec fails
            exit(EXIT_FAILURE);
        }
    }

    // We can also load the libary so we get access to the functions we need...
    void *handle = dlopen("./memwrap.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }

    // Resolve helper functions
    memstats_zero = dlsym(handle, "memstats_zero");
    memstats_get = dlsym(handle, "memstats_get");

    if (!memstats_zero || !memstats_get) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        return 1;
    }


    // OK, lets do some rudimentary command line parsing...

    char    *cf_test = "all";
    char    *cf_engine = "all";
    int     cf_show_matches = 0;
    int     cf_build_tree = 0;

    int args = argc;
    char **ap = &argv[1];
    while (args && *ap) {
        char *arg = *ap;
        fprintf(stderr, "Arg is: %s\n", *ap);
        
        if (strcmp(arg, "-t") == 0 && args > 1) {
            cf_test = *++ap;
        } else if (strcmp(arg, "-e") == 0 && args > 1) {
            cf_engine = *++ap;
        } else if (strcmp(arg, "-r") == 0) {
            cf_show_matches = 1;
        } else if (strcmp(arg, "-tree") == 0) {
            cf_build_tree = 1;
        }
        ap++;
        args--;
    }

    fprintf(stderr, "cf_test = %s\n", cf_test);
    fprintf(stderr, "cf_engine = %s\n", cf_engine);
    fprintf(stderr, "cf_show_results = %d\n", cf_show_matches);

    char *test_name = NULL;
    char *tnp = cf_test;

    // TEMPORARY FOR DEBUGGING
    if (cf_build_tree) {
        const struct testcase *t = find_named_test(cf_test);
        if (!t) { fprintf(stderr, "no test found\n"); exit (1); }

        funcs_rele.compile(t->regex);
        fprintf(stderr, "Building tree\n");

        funcs_rele.tree();
        exit(0);
    }


    // Ok, here is the test-case loop...
    while (1) {
        const struct testcase *t;
        
        // If we have one or more test specified then we need to find them...
        // Need to find a nicer way, this is awful!
        if (strcmp(cf_test, "all") != 0) {
            char    tn[128];
            while (*tnp == ',') tnp++;      // in case of double comma etc
            if (!*tnp) break;                        // no more left
            for (int i=0; i < 128; i++) {
                tn[i] = *tnp++;
                if (tn[i] == ',') { tn[i] = 0; break; }
                if (tn[i] == 0) { tnp--; }
            }
            fprintf(stderr, "test name is %s\n", tn);
            t = find_named_test(tn);
            if (!t) break;
        } else {
            t = find_next_test(test_name);
            if (!t) break;
        }
        // If we are looking at all tests, then get the next one...

        test_name = t->name;
        fprintf(stderr, "Test Case is: %s\n", test_name);

        fprintf(stderr, "Name: %s\n", t->name);
        fprintf(stderr, "Regex: %s\n", t->regex);
        fprintf(stderr, "Iterations: %d\n", t->iter);
        if (strlen(t->text) > 100) {
            fprintf(stderr, "Text: (long, %d chars)\n", (int)strlen(t->text));
        } else {
            fprintf(stderr, "Text: %s\n", t->text);
        }

        struct engine **es = engines;
        while (*es) {
            struct engine *e = *es;
            struct timespec start, compile, end;
            struct memstats mem;
            int used = 0;

            int err = TEST_OK;


            memstats_zero();
            stack_fill();

            // Stage 1 -- do compile, match, and check results
            //
            // If the match fails we still do the timings and memory stuff
            // to ensure we know how long these things take

            if (!e->compile(t->regex)) {
                err = TEST_COMPILE_FAIL;
                goto done;
            }
            if (!e->match(t->text)) {
                err = TEST_MATCH_FAIL;
                if (t->error == E_MATCHFAIL) goto timings;
                goto do_free;
            }

            int rg = e->res_count();
            if (rg != t->groups) {
                err = TEST_GROUPNO_WRONG;
                goto do_free;
            }
            for (int i = 0; i < t->groups; i++) {
                if (cf_show_matches) {
                    fprintf(stderr, "R: %d -> %d, %d  -- GOT: %d, %d\n", i, t->res[i].so, t->res[i].eo,
                                                                e->res_so(i), e->res_eo(i));
                }
//                fprintf(stderr, "R: %d -> %d, %d\n", i, e->res_so(i), e->res_eo(i));
                if (t->res[i].so != e->res_so(i) || t->res[i].eo != e->res_eo(i)) {
                    err = TEST_RESULTS_WRONG;
//                    goto timings;
                }
            }

timings:
            memcpy(&mem, memstats_get(), sizeof(struct memstats));
            used = stack_usage();

            e->free();
            
            // If we are successful, then lets try some timing tests...
            clock_gettime(CLOCK_MONOTONIC, &start);
            for (int i=0; i < t->iter; i++) {
                e->compile(t->regex);
                e->free();
            }
            e->compile(t->regex);
            clock_gettime(CLOCK_MONOTONIC, &compile);
            for (int i=0; i < t->iter; i++) {
                e->match(t->text);
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

       //     fprintf(stderr, "Elapsed time: %ld nsec\n", diff_timespec(start, end).tv_nsec);

    do_free:
            e->free();

    done:
            //mem = memstats_get();

            fprintf(stderr, "\t%s -> %s (stack=%d, mem=%d, allocs=%d) [compile_time=%dms, match_time=%dms, tot=%dms]\n", e->name, errors[err], used, 
                        (int)mem.total_allocated, (int)mem.total_allocs,
                        (int)timespec_to_ms(diff_timespec(start, compile)),
                        (int)timespec_to_ms(diff_timespec(compile, end)),
                        (int)timespec_to_ms(diff_timespec(start, end))
            );

    //        fprintf(stderr, "Status: %s\n", errors[err]);
    //        fprintf(stderr, "Used stack = %d\n", used);
    //        fprintf(stderr, "Allocated %d bytes, in %d allocs\n", (int)mem->total_allocated, (int)mem->total_allocs);

            es++;
        }
        //tc++;
    }

//    int used = stack_usage();
//    fprintf(stderr, "Used stack = %d\n", used);

//    struct memstats *mem = memstats_get();
//    fprintf(stderr, "Allocated %d bytes, in %d allocs\n", (int)mem->total_allocated, (int)mem->total_allocs);

    exit(0);

    /*
    char *x = malloc(100);

    fprintf(stderr, "x is %p\n", x);

    free(x);

    regex_t regex;
    regmatch_t  pmatch[5];

    #define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

    char *string = "adsfjhasdfjhabcasdfajskdh";
    if (regcomp(&regex, "abc", REG_EXTENDED)) exit(EXIT_FAILURE);
    int res = regexec(&regex, string, ARRAY_SIZE(pmatch), pmatch, 0);
    regfree(&regex);
    fprintf(stderr, "done\n");
    */
}
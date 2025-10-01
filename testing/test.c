
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


/**
 * Given a name, find the relevant test case...
 */
const struct testcase *find_case(char *name) {
    const struct testcase *t;
    int i = 0;

    while ((t = cases[i])) {
        if (strcmp((t)->name, name) == 0) return t;
        i++;
    }
    return NULL;
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


    const struct testcase **tc = cases;
    while (*tc)
    {
        const struct testcase *t = *tc;
        // OK, main memory_wrapped code starts here...


//        t = find_case("exponential");


    //    const struct testcase *t = cases[0];
        fprintf(stderr, "Name: %s\n", t->name);
        fprintf(stderr, "Regex: %s\n", t->regex);
        fprintf(stderr, "Text: %s\n", t->text);

        struct engine **es = engines;
        while (*es) {
            struct engine *e = *es;
            struct timespec start, end;
            struct memstats mem;
            int used = 0;

            int err = TEST_OK;


            memstats_zero();
            stack_fill();


            if (!e->compile(t->regex)) {
                err = TEST_COMPILE_FAIL;
                goto done;
            }
            if (!e->match(t->text)) {
                err = TEST_MATCH_FAIL;
                goto do_free;
            }

            int rg = e->res_count();
            if (rg != t->groups) {
                err = TEST_GROUPNO_WRONG;
                goto do_free;
            }
            for (int i = 0; i < rg; i++) {
//                fprintf(stderr, "R: %d -> %d, %d\n", i, e->res_so(i), e->res_eo(i));
                if (t->res[i].so != e->res_so(i) || t->res[i].eo != e->res_eo(i)) {
                    err = TEST_RESULTS_WRONG;
                    goto do_free;
                }
            }

            memcpy(&mem, memstats_get(), sizeof(struct memstats));
            used = stack_usage();

            // If we are successful, then lets try some timing tests...
            clock_gettime(CLOCK_MONOTONIC, &start);
            for (int i=0; i < 100000; i++) {
                e->free();
                e->compile(t->regex);
                e->match(t->text);
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

       //     fprintf(stderr, "Elapsed time: %ld nsec\n", diff_timespec(start, end).tv_nsec);

    do_free:
            e->free();

    done:
            //mem = memstats_get();

            fprintf(stderr, "\t%s -> %s (stack=%d, mem=%d, allocs=%d) [time=%dms]\n", e->name, errors[err], used, 
                        (int)mem.total_allocated, (int)mem.total_allocs,
                        (int)timespec_to_ms(diff_timespec(start, end)));

    //        fprintf(stderr, "Status: %s\n", errors[err]);
    //        fprintf(stderr, "Used stack = %d\n", used);
    //        fprintf(stderr, "Allocated %d bytes, in %d allocs\n", (int)mem->total_allocated, (int)mem->total_allocs);

            es++;
        }
        tc++;
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
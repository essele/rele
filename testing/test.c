
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>

#include <unistd.h>
#include <stdlib.h>
extern char **environ;

#include <regex.h>

#include "test.h"
#include "memwrap.h"
#include "stack.h"

void (*memstats_zero)(void);
struct memstats *(*memstats_get)(void);


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


extern struct relib funcs_libc;
extern struct relib funcs_rele;
extern struct relib funcs_pcre;



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

    // OK, main memory_wrapped code starts here...

    const struct testcase *t = find_case("another");
//    fprintf(stderr, "Regex: %s\n", c->regex);

//    const struct testcase *t = cases[0];
    fprintf(stderr, "Name: %s\n", t->name);
    fprintf(stderr, "Regex: %s\n", t->regex);
    fprintf(stderr, "Text: %s\n", t->text);

    memstats_zero();
    stack_fill();

//    struct relib *re = &funcs_pcre;
    struct relib *re = &funcs_rele;
//    struct relib *re = &funcs_libc;

    if (!re->compile(t->regex)) {
        fprintf(stderr, "compile failed\n");
        exit(0);
    }
    if (!re->match(t->text)) {
        fprintf(stderr, "match failed\n");
    }
    for (int i = 0; i < re->res_count(); i++) {
        fprintf(stderr, "res: %d -> %d, %d\n", i, re->res_so(i), re->res_eo(i));
    }
    re->free();

    int used = stack_usage();
    fprintf(stderr, "Used stack = %d\n", used);

    struct memstats *mem = memstats_get();
    fprintf(stderr, "Allocated %d bytes, in %d allocs\n", (int)mem->total_allocated, (int)mem->total_allocs);

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

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

int main(int argc, char *argv[]) {
    fprintf(stderr, "Hello\n");

    // Make sure we are properly wrapped....
    if (!getenv("MEMWRAP_LOADED")) {
        setenv("LD_PRELOAD", "./memwrap.so", 1);
        setenv("MEMWRAP_LOADED", "1", 1);
        execvpe(argv[0], argv, environ);
        perror("execvpe");  // only reached if exec fails
        exit(EXIT_FAILURE);
    }

    // OK, main memory_wrapped code starts here...

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
}
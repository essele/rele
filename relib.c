#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

int main(int argc, char *argv[]) {
    regex_t regex;
    regmatch_t  pmatch[1];

    if (regcomp(&regex, "abcd", 0))
        exit(EXIT_FAILURE);

    int res = regexec(&regex, "abcd", ARRAY_SIZE(pmatch), pmatch, 0);
    fprintf(stderr, "res = %d\n", res);

    regfree(&regex);
}
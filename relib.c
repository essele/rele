#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

int main(int argc, char *argv[]) {
    regex_t regex;
    regmatch_t  pmatch[5];

    if (0) {
    for (int j=0; j < 100; j++) {
        for (int i=0; i < 100000; i++) {
    
            if (regcomp(&regex, "abc", REG_EXTENDED))
                exit(EXIT_FAILURE);


            char *string = "blahblahabcd";

            int res = regexec(&regex, string, ARRAY_SIZE(pmatch), pmatch, 0);
            regfree(&regex);
        }
    }
    }
    char *string = "xxasdfasdfasdasdfabcbcd";
    if (regcomp(&regex, "(.|.|.).*abc", REG_EXTENDED)) exit(EXIT_FAILURE);
    int res = regexec(&regex, string, ARRAY_SIZE(pmatch), pmatch, 0);
    
    if (res == 0) {

        for (int i=0; i < ARRAY_SIZE(pmatch); i++) {
            int len = pmatch[i].rm_eo - pmatch[i].rm_so;
            fprintf(stderr, "%d:   %d -> %d  [%.*s]\n", i, pmatch[i].rm_so, pmatch[i].rm_eo, len, (char *)(string + pmatch[i].rm_so) );
        }
    }
    regfree(&regex);
}
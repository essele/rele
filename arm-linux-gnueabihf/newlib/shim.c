/**
 * Sample code for the newlib implementation (will need to move to a separate file)
 */
#include "../shim.h"
#include <stdio.h>
#include <regex.h>

#define LIBC_MAX_GROUPS     10
static regex_t         newlib_regex;
static regmatch_t      pmatch[LIBC_MAX_GROUPS];

static int newlib_compile(char *regex) {
    if (regcomp_nl(&newlib_regex, regex, REG_EXTENDED)) {
        // Compile Failed...
        return 0;
    }
    return 1;
}
static int newlib_match(char *text) {
    int res = regexec_nl(&newlib_regex, text, LIBC_MAX_GROUPS, pmatch, 0);
    if (res) return 0;  // match failed
    return 1;
}
static int newlib_res_count() {
    return newlib_regex.re_nsub + 1;
}
static int newlib_res_so(int res) {
    return pmatch[res].rm_so;
}
static int newlib_res_eo(int res) {
    return pmatch[res].rm_eo;
}
static int newlib_free() {
    regfree_nl(&newlib_regex);
    return 1;
}

struct engine newlib_engine = {
    .name = "newlib",
    .compile = newlib_compile,
    .match = newlib_match,
    .res_count = newlib_res_count,
    .res_so = newlib_res_so,
    .res_eo = newlib_res_eo,
    .free = newlib_free,
};

/**
 * Sample code for the libc implementation (will need to move to a separate file)
 */
#include "../test.h"
#include <regex.h>

#define LIBC_MAX_GROUPS     10
regex_t         libc_regex;
regmatch_t      pmatch[LIBC_MAX_GROUPS];

static int libc_compile(char *regex) {
    if (regcomp(&libc_regex, regex, REG_EXTENDED)) {
        // Compile Failed...
        return 0;
    }
    return 1;
}
static int libc_match(char *text) {
    int res = regexec(&libc_regex, text, LIBC_MAX_GROUPS, pmatch, 0);
    if (res) return 0;  // match failed
    return 1;
}
static int libc_res_count() {
    return libc_regex.re_nsub + 1;
}
static int libc_res_so(int res) {
    return pmatch[res].rm_so;
}
static int libc_res_eo(int res) {
    return pmatch[res].rm_eo;
}
static int libc_free() {
    regfree(&libc_regex);
    return 1;
}

struct relib funcs_libc = {
    .name = "libc",
    .compile = libc_compile,
    .match = libc_match,
    .res_count = libc_res_count,
    .res_so = libc_res_so,
    .res_eo = libc_res_eo,
    .free = libc_free,
};

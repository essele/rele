/**
 * Sample code for the tre implementation
 */
#include "../shim.h"
#define USE_LOCAL_TRE_H
#include "tre/local_includes/regex.h"

#define LIBC_MAX_GROUPS     10
static regex_t         tre_regex;
static regmatch_t      pmatch[LIBC_MAX_GROUPS];

static int tre_compile(char *regex, int flags) {
    if (tre_regcomp(&tre_regex, regex, REG_EXTENDED)) {
        // Compile Failed...
        return 0;
    }
    return 1;
}
static int tre_match(char *text, int flags) {
    int res = tre_regexec(&tre_regex, text, LIBC_MAX_GROUPS, pmatch, 0);
    if (res) return 0;  // match failed
    return 1;
}
static int tre_res_count() {
    return tre_regex.re_nsub + 1;
}
static int tre_res_so(int res) {
    return pmatch[res].rm_so;
}
static int tre_res_eo(int res) {
    return pmatch[res].rm_eo;
}
static int tre_free() {
    regfree(&tre_regex);
    return 1;
}

struct engine tre_engine = {
    .name = "tre",
    .compile = tre_compile,
    .match = tre_match,
    .res_count = tre_res_count,
    .res_so = tre_res_so,
    .res_eo = tre_res_eo,
    .free = tre_free,
};

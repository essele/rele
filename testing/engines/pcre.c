/**
 * Sample code for the rele implementation (will need to move to a separate file)
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#include <string.h>
#include <stdio.h>
#include "../test.h"

static pcre2_code *pcre_code;
static pcre2_match_data *match_data;
static int errornumber;
static PCRE2_SIZE erroroffset;

int libpcre_compile(char *regex) {
    pcre_code = pcre2_compile((PCRE2_SPTR8)regex, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL); 
    if (!pcre_code) return 0;

    match_data = pcre2_match_data_create_from_pattern(pcre_code, NULL);
    return 1;
}
int libpcre_match(char *text) {
    pcre2_match(pcre_code, (PCRE2_SPTR8)text, strlen(text), 0, 0, match_data, NULL);
    return 1;       // TODO
}
int libpcre_res_count() {
    return pcre2_get_ovector_count(match_data);
}
int libpcre_res_so(int res) {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    return (int)ovector[res * 2];
}
int libpcre_res_eo(int res) {
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    return (int)ovector[(res * 2) + 1];
}
int libpcre_free() {
    pcre2_match_data_free(match_data);
    pcre2_code_free(pcre_code);
    return 1;
}

struct engine funcs_pcre = {
    .name = "PCRE2",
    .compile = libpcre_compile,
    .match = libpcre_match,
    .res_count = libpcre_res_count,
    .res_so = libpcre_res_so,
    .res_eo = libpcre_res_eo,
    .free = libpcre_free,
};

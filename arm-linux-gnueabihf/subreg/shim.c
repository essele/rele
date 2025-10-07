#include <stdio.h>
#include <string.h>
#include "subreg/subreg.h"
#include "../shim.h"

#define SUBR_MAX_GROUPS     10
static subreg_capture_t      captures[SUBR_MAX_GROUPS];
static int 	capture_count;

static char	last_regex[1024];		// to adjust for ^ and $

static char 	*last_input;			// to keep for later!

static int subr_compile(char *regex) {
	strcpy(last_regex, regex);
//	strcpy(last_regex, ".*");
//	strcat(last_regex, regex);
//	strcat(last_regex, ".*");
// This doesn't work, bug raised on the github
	return 1;
}
static int subr_match(char *text) {
	last_input = text;
	capture_count = subreg_match(last_regex, text, captures, SUBR_MAX_GROUPS, 128);
	if (capture_count < 0) {
		fprintf(stderr, "SUBREG error: %d\n", capture_count);
		return 0;		// error
	}
	if (capture_count == 0) {
		return 0; 		// no match
	}
	return 1;
}
static int subr_res_count() {
	return capture_count;
}
static int subr_res_so(int res) {
	return (int)(captures[res].start - last_input);
}
static int subr_res_eo(int res) {
	return subr_res_so(res) + captures[res].length;
}
static int subr_free() {
    return 1;
}

struct engine subreg_engine = {
    .name = "subreg",
    .compile = subr_compile,
    .match = subr_match,
    .res_count = subr_res_count,
    .res_so = subr_res_so,
    .res_eo = subr_res_eo,
    .free = subr_free,
};

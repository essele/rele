#include <stdio.h>
#include <string.h>
#include "slre/slre.h"
#include "../shim.h"

static int 	offset;

static char	last_regex[1024];	
static char 	*last_input;			// to keep for later!

#define SLRE_MAX_CAPTURES	10
struct slre_cap	captures[SLRE_MAX_CAPTURES];

static int slree_compile(char *regex) {
	strcpy(last_regex, "(");
	strcat(last_regex, regex);
	strcat(last_regex, ")");

	for (int i=0; i < SLRE_MAX_CAPTURES; i++) {
		captures[i].ptr = NULL;
		captures[i].len = 0;
	}
	return 1;
}
static int slree_match(char *text) {
	last_input = text;

	offset = slre_match(last_regex, text, strlen(text), captures, SLRE_MAX_CAPTURES, 0);
	if (offset < 0) return 0;		// no match
	return 1;
}
static int slree_res_count() {
	for (int i=0; i < SLRE_MAX_CAPTURES; i++) {
		if (captures[i].ptr == NULL) return i;
	}
	return 0;
}
static int slree_res_so(int res) {
	return (int)(captures[res].ptr - last_input);
}
static int slree_res_eo(int res) {
	return slree_res_so(res) + captures[res].len;
}
static int slree_free() {
    return 1;
}

struct engine slre_engine = {
    .name = "slre",
    .compile = slree_compile,
    .match = slree_match,
    .res_count = slree_res_count,
    .res_so = slree_res_so,
    .res_eo = slree_res_eo,
    .free = slree_free,
};

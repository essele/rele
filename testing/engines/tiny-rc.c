#include "tiny-regex-c/re.h"
#include "../test.h"


static re_t ctx;
static int offset;
static int	mlen;

static int tinyrc_compile(char *regex) {
	ctx = re_compile(regex);
	if (!ctx) return 0;
	return 1;
}
static int tinyrc_match(char *text) {
	offset = re_matchp(ctx, text, &mlen);
	if (offset < 0) return 0;		// match failed
	return 1;
}
static int tinyrc_res_count() {
	if (offset >= 0) return 1;
	return 0;
}
static int tinyrc_res_so(int res) {
	if (res == 0) return offset;
	return -1;
}
static int tinyrc_res_eo(int res) {
	if (res == 0) return offset + mlen;
	return -1;
}
static int tinyrc_free() {
    return 1;
}

struct engine funcs_tinyrc = {
    .name = "tiny-regex-c",
    .compile = tinyrc_compile,
    .match = tinyrc_match,
    .res_count = tinyrc_res_count,
    .res_so = tinyrc_res_so,
    .res_eo = tinyrc_res_eo,
    .free = tinyrc_free,
};

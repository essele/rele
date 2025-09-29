/**
 * Sample code for the rele implementation (will need to move to a separate file)
 */
#include "../../librele/rele.h"
#include "../test.h"
struct rectx *rele_ctx;

int librele_compile(char *regex) {
    rele_ctx = rele_compile(regex, 0);
    if (!rele_ctx) return 0;
    return 1;
}
int librele_match(char *text) {
    rele_match(rele_ctx, text, 0, 0);
    return 1;       // TODO
}
int librele_res_count() {
    return rele_match_count(rele_ctx);
}
int librele_res_so(int res) {
    return rele_get_match(rele_ctx, res)->rm_so;
}
int librele_res_eo(int res) {
    return rele_get_match(rele_ctx, res)->rm_eo;
}
int librele_free() {
    rele_free(rele_ctx);
    return 1;
}

struct relib funcs_rele = {
    .name = "RELE",
    .compile = librele_compile,
    .match = librele_match,
    .res_count = librele_res_count,
    .res_so = librele_res_so,
    .res_eo = librele_res_eo,
    .free = librele_free,
};

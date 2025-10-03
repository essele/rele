/**
 * Sample code for the rele implementation (will need to move to a separate file)
 */
#include "../../librele/librele.h"
#include "../test.h"
struct rectx *rele_ctx;

int librele_compile(char *regex) {
    rele_ctx = rele_compile(regex, 0);
    if (!rele_ctx) return 0;
    //rele_export_tree(rele_ctx, "out.dot");
    return 1;
}
int librele_match(char *text) {
    if (rele_match(rele_ctx, text, 0, 0)) return 1;
    return 0;
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
int librele_tree() {
    rele_export_tree(rele_ctx, "tree.dot");
    return 1;
}

struct engine funcs_rele = {
    .name = "RELE",
    .compile = librele_compile,
    .match = librele_match,
    .res_count = librele_res_count,
    .res_so = librele_res_so,
    .res_eo = librele_res_eo,
    .free = librele_free,
    .tree = librele_tree,
};

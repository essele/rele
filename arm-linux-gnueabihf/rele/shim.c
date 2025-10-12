/**
 * Sample code for the rele implementation (will need to move to a separate file)
 */
#include "rele.h"
#include "../shim.h"
#include "../test.h"
static struct rectx *rele_ctx;

int librele_compile(char *regex, int flags) {
    int real_flags = 0;

    if (flags & F_ICASE) real_flags |= RELE_CASELESS;
    if (flags & F_NEWLINE) real_flags |= RELE_NEWLINE;

    rele_ctx = rele_compile(regex, real_flags);
    if (!rele_ctx) return 0;
    //rele_export_tree(rele_ctx, "out.dot");
    return 1;
}
int librele_match(char *text, int flags) {
    if (rele_match(rele_ctx, text, 0, flags)) return 1;
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

struct engine rele_engine = {
    .name = "rele",
    .compile = librele_compile,
    .match = librele_match,
    .res_count = librele_res_count,
    .res_so = librele_res_so,
    .res_eo = librele_res_eo,
    .free = librele_free,
    .tree = librele_tree,
};

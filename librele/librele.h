
#ifndef __RELE_H
#define __RELE_H

#include <stdint.h>

// Compile flags...
#define F_CASELESS          (1 << 0)            // caseless matching
#define F_NO_FASTSTART      (2 << 0)            // disable FASTSTART optimisation

// Match flags...
#define F_KEEP_TASKS        (1 << 16)

// A define for this, but it will be anonymous
struct rectx;

// A type used for matching groups... 
struct rele_match_t {
    int32_t     rm_so;
    int32_t     rm_eo;
};

struct rectx *rele_compile(char *regex, uint32_t flags);
int rele_match(struct rectx *ctx, char *p, int len, int flags);
void rele_free(struct rectx *ctx);

int rele_match_count(struct rectx *ctx);
struct rele_match_t *rele_get_match(struct rectx *ctx, int n);
struct rele_match_t *rele_get_matches(struct rectx *ctx);

void rele_export_tree(struct rectx *ctx, const char *filename);

#endif
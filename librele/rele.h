
#ifndef __RELE_H
#define __RELE_H

#include <stdint.h>


// TODO: this will need to be in an include file....
// Compile flags...
#define F_CASELESS  (1 << 0)

// Match flags...
#define F_KEEP_TASKS    (1 << 16)

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

#endif
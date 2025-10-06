#include <stdio.h>
#include <stdlib.h>
#include "memwrap.h"
#include <stdint.h>
#include <string.h>

#include <stdint.h>
#include "shim.h"

// Ensure the engines are all visible...
extern struct engine pcre_engine;
extern struct engine newlib_engine;

struct engine *eng = &pcre_engine;



int main(void) {
    printf("Hello from bare metal!\n");




    memstats_zero();
    int rc = eng->compile("aaa");
    fprintf(stderr, "rc=%d\n", rc);
    rc = eng->match("helloaaatheer");
    fprintf(stderr, "rc=%d\n", rc);
    if (rc) {
        int ng = eng->res_count();
        fprintf(stderr, "Have %d groups\n", ng);
        for (int i=0; i < ng; i++) {
            fprintf(stderr, "%d: %d,%d\n", i, eng->res_so(i), eng->res_eo(i));
        }
    }

    eng->free();

    struct memstats *ms = memstats_get();
    printf("Allocs = %d\n", ms->total_allocs);
    printf("Amount = %d\n", ms->total_allocated);


    printf("here\n");

    return 0;
}

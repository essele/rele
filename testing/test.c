
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test.h"





int main(int argc, char *argv[]) {
    fprintf(stderr, "Hello\n");

    char *x = malloc(100);

    fprintf(stderr, "x is %p\n", x);

    free(x);
}
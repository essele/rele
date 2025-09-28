#include <stdio.h>


// Testing struct type stuff....

struct result {
    int so;
    int eo;
};

struct item {
    char *name;
    char *regex;
    char *string;
    int rc;
    int groups;
    struct result res[];
};

const char s1[] = { 0x61, 0x62, 0x63, 0x63, 0x62, 0x61, 0x00 };

const struct item i1 = {
    .name ="test name",
    .regex = "abc",
    //.string = "match with abc here to do this",
    .string = (char *)s1,
    .rc = 0,
    .groups = 3,
    .res = { { 1, 2 }, { 2, 3 }, {3, 4} }
};

const struct item i2 = {
    "test 2",
    "abc.*",
    "match sadfwasdfiasdfth abc here to do this",
    0,
    2,
    { { 1, 2 }, { 2, 3 } }
};


const struct item *items[] = {
    &i1,
    &i2,
    NULL
};


int main(int argc, char *argv[]) {
    fprintf(stderr, "hello\n");

    printf("%s\n", items[0]->string);
}
#ifndef __TEST_H
#define __TEST_H

struct result {
    int     so;
    int     eo;
};

enum {
    E_OK = 0,
    E_MATCHFAIL,
};

/*
 * Main structure for holding test items
 */
struct testcase {
    char *group;
    char *name;
    char *desc;
    char *regex;
    char *text;
    int rc;
    int groups;
    int error;                  // expected errors
    int iter;                   // how many iterations
    struct result res[];
};

extern const struct testcase *cases[];

#endif
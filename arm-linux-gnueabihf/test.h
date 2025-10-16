#ifndef __TEST_H
#define __TEST_H

struct result {
    int     so;
    int     eo;
};

enum {
    F_ICASE = (1 << 0),
    F_NEWLINE = (1 << 1),
};

enum {
    E_OK = 0,
    E_MATCHFAIL = (1 << 0),
    E_COMPFAIL = (1 << 1),
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
    int cflags;
    int mflags;
    int rc;
    int groups;
    int error;                  // expected errors
    int iter;                   // how many iterations
    struct result res[];
};

extern const struct testcase *cases[];

#endif
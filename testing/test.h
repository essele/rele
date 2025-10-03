

struct engine {
    char    *name;
    int     (*compile)(char *regex);
    int     (*match)(char *text);
    int     (*res_count)();
    int     (*res_so)(int res);
    int     (*res_eo)(int res);
    int     (*free)();
    int     (*tree)();
};

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

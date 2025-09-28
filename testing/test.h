
/*
 * Results structure
 */
struct result {
    int so;
    int eo;
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
    struct result res[];
};


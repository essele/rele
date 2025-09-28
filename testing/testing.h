
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
struct item {
    char *name;
    char *regex;
    char *string;
    int rc;
    int groups;
    struct result res[];
};


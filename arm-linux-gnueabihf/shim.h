/**
 * Wrapper system to provide a common interface to all of the different engines
 */
struct engine {
    char    *name;
    int     (*compile)(char *regex, int flags);
    int     (*match)(char *text, int flags);
    int     (*res_count)();
    int     (*res_so)(int res);
    int     (*res_eo)(int res);
    int     (*free)();
    int     (*tree)();
};

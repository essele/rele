/*
 * RELE -- reelee ... a regular expression library for small embedded systems
 *
 * Writeen by Lee Essen, September 2025
 * 
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>      // remove, for debug only
#include <ctype.h>

#include "librele.h"

// Include an ID in the nodes to help with debugging and tree visualisation
#define DEBUG_ID    



// Internal flags ... created by compile stage
#define IF_NOT_LAZY (1 << 15)




enum {
    OP_NOP = 0, OP_CONCAT, OP_MATCH, OP_MATCHGRP, OP_DONE,
    OP_CRLF, OP_ANCHOR,
    OP_ALTERNATE = '|',
    OP_PLUS = '+', OP_STAR = '*', OP_QUESTION = '?', OP_GROUP = '(',
    OP_BEGIN = '^', OP_END = '$', OP_MULT = '{',
    OP_MATCHSET = '[',
};

// ============================================================================
// Binary tree bits...
// ============================================================================

#define NO_MAX              0xffff
#define NO_GROUP            0xff

struct node {
    union {
       struct node      *a;             // the first child
       struct {
            uint16_t    min;
            uint16_t    max;
        };
       uint8_t          group;          // for creating groups
    };
    union {
        struct node     *b;             // the second child
        struct set      *set;           // a possible set match
        uint8_t         mgrp;           // a possible group match
        char            ch[2];          // can store matched char
    };
    struct node         *parent;        // for a way back
    uint8_t             op;             // which operation?
    uint8_t             lazy;           // won't fit in a with minmax
    // Two bytes unused here (if 32bit)
};

// Where we have nodes that don't need children we need to mark the
// b-leg otherwise it will be used by something...
#define NOTUSED     (struct node *)1


// We have a 'context' which contains the root of the tree
struct rectx {
    struct node     *root;


    struct node     *nodes;     // current node pointer
    struct set      *sets;      // current set pointer

    struct task     *free_list; // free tasks list
    struct task     *done;      // the candiate completed task

    uint16_t        flags;
    uint8_t         groups;     // allows up to 255 groups
    uint8_t         pad;        // not used

    uint32_t        pad2;

    // Memory for nodes and sets will follow this...
};


#define NODE_ID(ctx, n)          (int)(((void *)n - ((void *)ctx + sizeof(struct rectx)))/sizeof(struct node))

static struct node *create_node_above(struct rectx *ctx, struct node *this, uint8_t op, struct node *a, struct node *b) {
    struct node *parent = this->parent;
    struct node *n = ctx->nodes++;          // alloc (kind of)
    n->parent = parent;
    this->parent = n;

    // Now update the correct leg of the parent
    if (parent) {
        if (parent->a == this) {
            parent->a = n;
        } else {
            parent->b = n;
        }
    } else {
        // Top of the tree
        ctx->root = n;
    }
    // Now set the right values...
    n->op = op;
    n->a = a;
    n->b = b;
    return n;
}


// Create a new node in the structure ... we'll need to work out where to link
// ourselfs. We can either be the first (last == NULL), or we'll be going on the
// b node of the last one, or if that's taken, we'll need to add a concat.
//
static struct node *create_node_here(struct rectx *ctx, struct node *last, uint8_t op, struct node *a, struct node *b) {
    struct node *n = ctx->nodes++;  // alloc - kind of
    n->op = op;
    n->a = a;
    n->b = b;

    if (!last) {
        // We are the top of the tree
        n->parent = NULL;
        ctx->root = n;
    } else if (!last->b) {
        // TODO: if b happens to be zero on the wrong type of node this could go wrong, need to protect
        // against it, probably by checking op type (only concat and alternate can use b as a node?)
        last->b = n;
        n->parent = last;
    } else {
        // We need a concat...
        n->parent = create_node_above(ctx, last, OP_CONCAT, last, n);
    }
    return n;
}


// -------------------------------------------------------------------------------
// TASKS
// -------------------------------------------------------------------------------
#define TASK_STACK_SIZE      3

struct task {
    struct task             *next;          // tasks are singly linked
    struct node             *n;             // current node
    union {
        struct node         *last;          // last one we processed (for direction)
        char                *p;             // pointer to the DONE index
    };

    // Stack mechanism for {x,y} counting
    uint16_t            sp;         // more an index than pointer (smaller)
    uint16_t            stack[TASK_STACK_SIZE];

    // Pointer to check for repeated matches of non-char things
    // (like empty groups), I hate the extra space need, but otherwise
    // it's easy to get infinite tasks spawning
    struct node         *lastghostmatch;

    // All of the group matches follow...
    struct rele_match_t   grp[];
};

static int tcount = 0;      // TODO: might want to remove and use #defined debug code


/*
 * Some helper functions
 */
int rele_match_count(struct rectx *ctx) { return ctx->groups; }
struct rele_match_t *rele_get_match(struct rectx *ctx, int n) { return &(ctx->done->grp[n]); }


// -------------------------------------------------------------------------------
// SETS (of characters or ranges etc)
// -------------------------------------------------------------------------------

// Given a set of characters [abc\d1-0] etc, create a 128 bit mask that we
// can use to do rapid comparisons. The set will be linked into node b
// of the supplied node, and the new pointer returned.
//
// A 128 byte setup would be quicker and we could use a mask which would
// allow us to use it 8 times (for different matches) but then we would
// need to store a ptr and a mask, so this may be a bit slower but I think
// it's more efficient.
//
struct set {
    uint32_t d[4];
};

static char *build_set(struct rectx *ctx, char *p, struct node *n) {
    // "Allocate" the space...
    struct set *set = ctx->sets++;
    int negate = 0;

    #define SET_VAL(v)                      set->d[(v)/32] |= (1 << ((v)%32))
    #define SET_CASELESS_VAL(v)             SET_VAL(v); \
                                            if (v >= 'a' && v <= 'z') { SET_VAL(v - ('a' - 'A')); } \
                                            else if (v >= 'A' && v <= 'Z') { SET_VAL(v + ('a' - 'A')); }
    #define SET_RANGE(beg, end)             for (uint8_t c = beg; c <= end; c++) { SET_VAL(c); }
    #define SET_CASELESS_RANGE(beg, end)    for (uint8_t c = beg; c <= end; c++) { SET_CASELESS_VAL(c); }

    p++;            // get past the '['
    if (*p == '^') { negate = 1; p++; }
    while (1) {
        if (!*p) goto fail;
        if (*p == ']') break;           // done
        if (p[1] == '-' && p[2] && p[2] != ']') {
            if (p[0] > p[2]) goto fail;
            if (ctx->flags & F_CASELESS) {
                SET_CASELESS_RANGE(p[0], p[2]);
            } else {
                SET_RANGE(p[0], p[2]);
            }
            p += 3;           
        } else {
            if (*p == '\\') {
                switch (*(++p)) {
                    case 'w':   SET_RANGE('a', 'z');
                                SET_RANGE('A', 'Z');    // fall through
                    case 'd':   SET_RANGE('0', '9'); break;
                    case 's':   SET_VAL(' '); SET_VAL('\f'); SET_VAL('\n'); 
                                SET_VAL('\r'); SET_VAL('\t'); SET_VAL('\v'); break;
                    case 'W':   SET_RANGE(0, '0'-1); SET_RANGE('9'+1, 'A'-1);
                                SET_RANGE('Z'+1, 'a'-1); SET_RANGE('z'+1, 126); break;
                    case 'D':   SET_RANGE(0, '0'-1); SET_RANGE('9'+1, 126); break;
                    case 'S':   SET_RANGE(0, 8); SET_RANGE(14, 31); SET_RANGE(33, 126); break;
                    case 0:     goto fail;
                    default:    SET_VAL(*p); break;
                }
            } else {
                if (ctx->flags & F_CASELESS) {
                    SET_CASELESS_VAL(*p);
                } else {
                    SET_VAL(*p);
                }
            }
            p++;
        }
    }
    if (negate) {
        set->d[0] = ~set->d[0];
        set->d[1] = ~set->d[1];
        set->d[2] = ~set->d[2];
        set->d[3] = ~set->d[3];
    }
    n->set = set;
    return p;

fail:
    return NULL;
}

// Used when parsing the regex to work out scale, we just need to get past
// the set syntax in the same way as above... we don't error check in here
// other than not running off the end...
char *dummy_set(char *p) {
    p++;            // get past the '['
    if (*p == '^') { p++; }
    while (*p) {
        if (*p == ']') break;           // done
        if (p[1] == '-' && p[2] && p[2] != ']') {
            p += 3;           
        } else {
            if (*p == '\\') {
                p++;
            }
            p++;
        }
    }
    return p;
}

int match_set(char ch, struct set *set) {
    if (set->d[ch/32] & ~((uint32_t)(1 << (ch % 32)))) {
        return 1;
    }
    return 0;
}

// Process a min/max spec and update the supplied node accordingly
char *minmax(char *p, struct node *n) {
    uint16_t min = 0, max = 0;
    p++;                // get past '{'

    // First check we have the required digit...
    if (!isdigit((int)*p)) return NULL;

    // Now work out the first number...
    while (isdigit((int)*p)) { min = (min * 10) + (*p++ - '0'); if (min > 1000) return NULL; }

    if (*p == '}') { max = min; goto done; }        // An exact number
    if (*p++ != ',') return NULL;                   // Need a comma
    if (*p == '}') { max = NO_MAX; goto done; }     // No second number

    // Now get the second number...
    while (isdigit((int)*p)) { max = (max * 10) + (*p++ - '0'); if (max > 1000) return NULL; }

    // Now must be a close bracket and right order...
    if (*p != '}' || max < min) return NULL;

done:
    if (n) {
        n->min = min;
        n->max = max;
    }
    return p;
}

// Check if a string could be a group identifier, these can be...
// \1, \10, \21, \200, \{12}, \g1, \g12, \g{15}
// We assume we have been called with p just after the backslash...
// We just process the number here, we don't check if the group is
// too high.
int is_group(char *p, uint8_t *gid, char **newp, char **errp) {
    int group = 0;
    int bracket = 0;

    if (*p == 'g') p++;
    if (*p == '{') { p++; bracket = 1; }
    if (*p == '0') goto error;      // cant have zero, or leading zeros
    while (*p >= '0' && *p <= '9') {
        group = (group * 10) + (int)(*p - '0');
        if (group > 255) goto error;
        p++;
    }
    if (group == 0) return 0;       // not a group!
    if (bracket) {
        if (*p != '}' || group == 0) goto error;
        p++;
    }
    // Was a group...
    if (gid) *gid = (uint8_t)group;
    if (newp) *newp = p-1;              // need to point at the last char of it.
    return 1;

error:
    if (newp) *newp = NULL;
    if (errp) *errp = p;
    return 1;
}


// ------------------------------------------------------------------------
// Dummy (and hopefully fast) version of the compiler that is purely used
// to measure how many nodes and sets this regex will need and then allocate
// the memory used for both in a single block.
// ------------------------------------------------------------------------
struct rectx *alloc_ctx(char *regex) {
    int matches = 0;
    int nodes = 0;
    int sets = 0;

    char *p = regex;
    while (*p) {
        // There's always a match at the end of a given brach, therefore matches
        // are the key. We will always have one less "splits" (i.e. concat or 
        // alternate) than we have matches, everything else is always a node.
        switch (*p) {
            case OP_MULT:
                p = minmax(p, NULL);
                if (!p) return NULL;        // min max error
                // drop through...

            // These are always a node...
            case OP_PLUS: case OP_STAR: case OP_QUESTION:
                if (p[1] == '?') p++;       // lazy version
                nodes++;
                break;

            // An empty group counts as a node and a match...
            case OP_GROUP:
                if (p[1] == ')' || (p[1] == '?' && p[2] == ':' && p[3] == ')')) { matches++; }
                nodes++;
                break; 

            // Ignore these (alternate we cover via matches)...
            case OP_ALTERNATE: case ')':
                break;

            case OP_MATCHSET:
                p = dummy_set(p);
                if (!p) return NULL;        // set error (not impl)
                sets++;
                // drop through

            // These are effectively matches...
            case OP_BEGIN: case OP_END:
                matches++;
                break;

            default:
                // Handle the \Q..\E case....
                if (p[0] == '\\' && p[1] == 'Q') {
                    p++;    // on the Q
                    while (p[1] && !(p[1] == '\\' && p[2] == 'E')) {
                        p++;
                        matches++;
                    }
                    if (!p[1]) return NULL;       // no end to \Q..\E
                    p += 2;
                    break;
                }

                matches++;
                if (*p == '\\' && is_group(p+1, NULL, &p, NULL)) {
                    // Check for group syntax/scale issues...
                    if (!p) return NULL;
                } else if (*p == '\\') p++;
        }
        p++;
    }

    // We need one less splitter than matches
    int splits = matches - 1;
    // We also need space for our extra added nodes
    nodes += matches + splits + 6;

//    fprintf(stderr, "Matches = %d, Splits = %d, Nodes = %d\n", matches, splits, nodes);

    struct rectx *ctx = malloc(sizeof(struct rectx) +
                                (nodes * sizeof(struct node)) + 
                                (sets * sizeof(struct set)));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(struct rectx) +
                                (nodes * sizeof(struct node)) + 
                                (sets * sizeof(struct set)));

    ctx->nodes = (struct node *)((void *)ctx + sizeof(struct rectx));
    ctx->sets = (struct set *)((void *)ctx + (sizeof(struct rectx) + (nodes * sizeof(struct node))));
    return ctx;
}


// ------------------------------------------------------------------------
// Simple compiler that turns a regular expression into a binary tree
// ------------------------------------------------------------------------

struct rectx *rele_compile(char *regex, uint32_t flags) {
    // First allocate the ctx structure including nodes and sets based on
    // the regex
    struct rectx *ctx = alloc_ctx(regex);
    if (!ctx) return NULL;
    ctx->flags = flags;
    ctx->groups = 1;

    // Now run through the regex...
    char        *p = regex;
    struct node *last = NULL;
    int         lazy;

    // Early part of the tree.... ".*(main)", so a DOT STAR (lazy) and
    // a GROUP 0...
//    last = create_node_here(ctx, last, OP_MATCH, NULL, (struct node *)'.');
//    last = create_node_above(ctx, last, OP_STAR, NULL, last);
//    last->lazy = 1;
    last = create_node_here(ctx, last, OP_GROUP, NULL, NULL);
    last->group = 0;

    while (*p) {
        switch (*p) {
            case OP_PLUS:
            case OP_STAR:
            case OP_QUESTION:
                lazy = (p[1] == '?' ? 1 : 0);
                last = create_node_above(ctx, last, (uint8_t)*p, NULL, last);
                last->lazy = lazy;
                p += lazy;              // skip the ? if we have it
                if (!lazy) ctx->flags |= IF_NOT_LAZY;
                break;

            case OP_ALTERNATE:
                // Get to the previous thing...
                while (last->parent && last->parent->op == OP_CONCAT) { last = last->parent; }
                last = create_node_above(ctx, last, OP_ALTERNATE, last, NULL);
                break;

            case OP_GROUP:
                last = create_node_here(ctx, last, OP_GROUP, NULL, NULL);
                if (p[1] == '?' && p[2] && p[2] == ':') {
                    last->group = NO_GROUP;
                    p += 2;
                } else {
                    last->group = ctx->groups++;
                }
                break;

            case ')':       // closing a group
                // Logic is a little complex....
                // We need to get back to the previous group...
                //  If we aren't a group, then just to back to the last one.
                //  If we are a used group, then go back to the prior one
                //  If we are an empty group, then mark it used, so we go back next time.
                if (last && last->op == OP_GROUP && !last->b) {
                    // This is an empty group, so just mark it NOTUSED
                    last->b = NOTUSED;
                    break;
                }
                if (last && last->op == OP_GROUP) {
                    // We need to ensure we go up at least one...
                    last = last->parent;
                }
                while(last && last->op != OP_GROUP) { last = last->parent; }
                break;

            case OP_MULT:
                last = create_node_above(ctx, last, OP_MULT, NULL, last);
                p = minmax(p, last);
                if (!p) goto fail;
                if (p[1] == '?') { 
                    last->lazy = 1; 
                    p++; 
                } else {
                    ctx->flags |= IF_NOT_LAZY;
                }
                break;

            case OP_BEGIN:
            case OP_END:
//                last = create_node_here(ctx, last, (uint8_t)*p, NULL, NOTUSED);
                last = create_node_here(ctx, last, OP_ANCHOR, NULL, NULL);
                last->ch[0] = *p;
                break;

            case OP_MATCHSET:
                last = create_node_here(ctx, last, OP_MATCHSET, NULL, NULL);
                if (!last) goto fail;
                p = build_set(ctx, p, last);
                if (!p) goto fail;
                break;

            default:
                // Handle the \Q..\E case....
                if (p[0] == '\\' && p[1] == 'Q') {
                    p++;    // on the Q
                    while (p[1] && !(p[1] == '\\' && p[2] == 'E')) {
                        p++;
                        last = create_node_here(ctx, last, OP_MATCH, NULL, NULL);
                        switch (*p) {
                            case '.':   last->ch[0] = '\\'; last->ch[1] = '.'; break;
                            case '\\':  last->ch[0] = '\\'; last->ch[1] = '\\'; break;
                            default:    last->ch[0] = *p;
                        }
                    }
                    if (!p[1]) goto fail;       // no end to \Q..\E
                    p += 2;
                    break;
                }

                // We are going to need a OP_MATCH or an OP_MATCHGRP
                last = create_node_here(ctx, last, OP_MATCH, NULL, NULL);
                if (!last) goto fail;
                if (*p == '\\' && is_group(p+1, &(last->mgrp), &p, NULL)) {
                    if (!p) goto fail;              // group syntax error
                    last->op = OP_MATCHGRP;
                } else {
                    last->ch[0] = *p;
                    if (*p == '\\') {
                        // Handle special cases and normal backslash chars...
                        switch (p[1]) {
                            case 'R':   last->op = OP_CRLF;
                                        break;
                            case 'b':   
                            case 'B':
                                        last->op = OP_ANCHOR;
                                        last->ch[0] = p[1];
                                        break;
                            default:
                                        last->ch[1] = p[1];
                                        if (!p[1]) goto fail;
                        }
                        p++;
                    }
                }
        }
        p++;
    }
    // Postprocessing just needs to ensure there's a DONE in the right place
    // We put it after the group b node to save one more parent move.
    //last = create_node_here(ctx, ctx->root->b, OP_DONE, NULL, NULL);
    last = create_node_here(ctx, ctx->root, OP_DONE, NULL, NULL);
    return ctx;

fail:
    free(ctx);
    return NULL;
}


// -------------------------------------------------------------------------------
// Simple matching with escapes and classes
// -------------------------------------------------------------------------------
// TODO: what if last char is a backslash, we will go over the length!
int matchone(char *p, char ch) {
    if (*p == '.') return 1;            // always match
    if (*p == '\\') {
        switch(p[1]) {
            // Types...
            case 'd':       return isdigit(ch);
            case 'D':       return !isdigit(ch);
            case 'w':       return isalnum(ch);
            case 'W':       return !isalnum(ch);
            case 's':       return isspace(ch);
            case 'S':       return !isspace(ch);

            // Normal escapes...
            case 't':       return ch == '\t';
            case 'n':       return ch == '\n';

            // Regex escapes...
            case '\\':
            case '*': case '+': case '?': case '.':
            case '$': case '^': case '(': case ')':
            case '[': case ']': case '{': case '}':
            case '-': case '|':
                            return p[1] == ch;
        }
        // TODO: needs to fail in compile rather than here
        return -1;
    }
    // TODO: needs case indepedence here
    return *p == ch;
}

// -------------------------------------------------------------------------------
// TASK EXECUTION
// -------------------------------------------------------------------------------

// Create a new task, optionally copying any state from the 'from' task
struct task *task_new(struct rectx *ctx, struct task *from, struct task *next, struct node *last, struct node *node) {
    struct task *task = ctx->free_list;

    if (task) {
        ctx->free_list = task->next;
    } else {
        // Allocate a task with enough space for gorup matching...
        task = (struct task *)malloc(sizeof(struct task) + (ctx->groups * sizeof(struct rele_match_t)));
        if (!task) return NULL;
        memset((void *)task, 0, sizeof(struct task));

        tcount++;
//        fprintf(stderr, "max task count is %d\n", tcount);
    }

    if (from) {
        // Copy the stack and group matches...
        memcpy(task->stack, from->stack, sizeof(task->stack));
        memcpy(task->grp, from->grp, sizeof(struct rele_match_t) * ctx->groups);
        task->sp = from->sp;
        task->lastghostmatch = from->lastghostmatch;
    } else {
        // Make sure matches are -1 to staret with...
        for (int i=0; i < ctx->groups; i++) {
            task->grp[i].rm_so = task->grp[i].rm_eo = (int32_t)-1;
        }
        task->sp = TASK_STACK_SIZE;
        task->lastghostmatch = NULL;
    }
    task->next = next;
    task->last = last;
    task->n = node;
    return task;
}

static void inline task_release(struct rectx *ctx, struct task *task) {
    task->next = ctx->free_list;
    ctx->free_list = task;
}

// Freeing the context is much simpler now since everything was allocated
// in a block, also the tasks are freed after the match, so we only need
// to worry about the successful one.
void rele_free(struct rectx *ctx) {
    // If we have kept our tasks then they will still be in the free list...
    while (ctx->free_list) { struct task *x = ctx->free_list->next; free(ctx->free_list); ctx->free_list = x; }

    // Free the result task if there is one...
    if (ctx->done) free(ctx->done);
    free(ctx);
}

// TODO: might be quicker using a memcpy(a->grp, b->grp, ctx->groups * sizeof(struct xxx))
static inline int has_same_groups(struct rectx *ctx, struct task *a, struct task *b) {
    /*
    int32_t *pa = (int32_t *)a->grp;
    int32_t *pb = (int32_t *)b->grp;
    int i = ctx->groups * 2;
    while (i--) {
        if (*pa++ != *pb++) return 0;
    }
    return 1;
    */
    /*
    for (int i=0; i < ctx->groups; i++) {
        if (a->grp[i].rm_so != b->grp[i].rm_so) return 0;
        if (a->grp[i].rm_eo != b->grp[i].rm_eo) return 0;
    }
    return 1;
    */
    if (memcmp(a->grp, b->grp, ctx->groups * sizeof(struct rele_match_t)) == 0) return 1;
    return 0;
}

static inline int has_same_stack(struct rectx *ctx, struct task *a, struct task *b) {
    if (a->sp != b->sp) return 0;
    for (int i=a->sp; i < TASK_STACK_SIZE; i++) {
        if (a->stack[i] != b->stack[i]) return 0;
    }
    return 1;
}

/**
 * Task Deduplication ... if we are have matchedsomething, then look at
 * all the tasks that went before us and see if any did the same match and
 * then, if the state is all the same, we can die.
 * 
 * TODO: is the state actually important or not??
 */
static inline int has_prior_match(struct rectx *ctx, struct task *run_list, struct node *n, struct task *t) {
    for (struct task *x = run_list; x != t; x = x->next) {
        if (x->last == n) {
            if (has_same_groups(ctx, x, t) && has_same_stack(ctx, x, t)) return 1;
            return 1;
        }
    }
    return 0;
}

static int rele_match_iter(struct rectx *ctx, char *start, char *p, char *end, int flags);

int rele_match(struct rectx *ctx, char *p, int len, int flags) {
    char *start = p;
    char *end = p + (len ? len : strlen(p));
    // TODO: < end or <= end (for $)
    while (p <= end) {
        if (rele_match_iter(ctx, start, p, end, flags)) return 1;
        p++;
    }

    if (!(flags & F_KEEP_TASKS)) {
        while (ctx->free_list) { struct task *x = ctx->free_list->next; free(ctx->free_list); ctx->free_list = x; }
    }

    return 0;
}

/**
 * Regular expression matching, returns 1 if a match is found or
 * 0 if not.
 */
//static int rele_match_iter(struct rectx *ctx, char *p, int len, int flags) {
static int rele_match_iter(struct rectx *ctx, char *start, char *p, char *end, int flags) {
    tcount = 0;

    // If we have a result left over from a prior run, free it.
    if (ctx->done) { task_release(ctx, ctx->done); ctx->done = NULL; }

    // Create the first task on the list...
    struct task *run_list = task_new(ctx, NULL, NULL, NULL, ctx->root);

    // Keep track of the previous one so we can remove items
    struct task *prev = NULL;

    struct task *t;
    struct set *set;
    //char *start = p;
    //char *end = p + (len ? len : strlen(p));        // TODO: remove and use len or zero check?

    do {
        char ch = (p < end ? *p : 0);

        // Get ready to run through for this char...
        t = run_list;
        prev = NULL;

        if (!t) goto done;

        // Now for each task go through the binary tree until we get to
        // a match type op, then we either die (match failed), or we stay
        // for next time.
        while (t) {
            struct node *n = t->n;

            switch(n->op) {
                // If we get to OP_DONE then we are done, but there might be other
                // tasks to continue. We keep the first task completed at each index,
                // then overwrite for the next one.
                //
                // However, since the tasks are prioritised based on lazyness etc, then
                // if there are no tasks before us, then we are the one!
                //
                // Old theory:
                // If our regex is ALL lazy, then the first completed is the answer!
                //
                case OP_DONE:
                    // If we have already completed at this index, then die...
                    if (ctx->done && ctx->done->p == p) goto die;

                    // Free the previous candidate if there was one...
                    if (ctx->done) task_release(ctx, ctx->done);

                    // TODO: testing for now .. first in task list to finish.
                    if (run_list == t) {
                        // remove from the running list...
                        run_list = t-> next;
                        prev = NULL;

                        // Mark us as the candidate...
                        t->next = NULL;
                        t->p = p;
                        ctx->done = t;
                        goto done;
                    }
                    // TODO: there might be another optimisation, if we die (at the top of this case)
                    // then we might be done. Or does that not work.

                    // Remove ourselves from the running list...
                    if (prev) { prev->next = t->next; } else { run_list = t->next; prev = NULL; }


                    // Mark us as the candidate...
                    t->next = NULL;
                    t->p = p;
                    ctx->done = t;

                    // If we are all lazy, the first to finish is the one!
//                    if (!(ctx->flags & IF_NOT_LAZY)) { fprintf(stderr, "XXXXX\n"); goto done; } 

                    // And move on to the next task...
                    if (prev) { t = prev->next; } else { t = run_list; /* goto tasks_done; */ }
                    continue;

                case OP_CONCAT:
                    if (t->last == n->a) goto leg_b;
                    if (t->last == n->b) goto parent;
                    goto leg_a;

                // If we get here from above then spin off a new task to go down leg b
                // and we go down leg a. Anything coming back up, goes to the parent.
                case OP_ALTERNATE:
                    if (t->last == n->parent) {
                        t->next = task_new(ctx, t, t->next, n, n->b);
                        goto leg_a;
                    }
                    goto parent;

                // If we get here from above, then go down the b leg. If we get here
                // from b, then it was successful and we spawn. Who goes where depends
                // on if we are lazy or not...
                case OP_PLUS:
                    if (t->last == n->parent) goto leg_b;
                    goto new_b_or_parent;

                // If we get here from above, we spawn to go back (zero) then we go
                // down b. If we get here from b, then carry on back up.
                case OP_QUESTION:
                    if (t->last == n->b) goto parent;
                    goto new_b_or_parent;

                // If we hit from above then spawn to go right back up (zero) and from
                // b we do the same.
                case OP_STAR:
                    goto new_b_or_parent;

                case OP_ANCHOR:
                    // CHeck for a ghost match on these...
                    if (t->lastghostmatch == n) goto die;
                    t->lastghostmatch = n;
                    switch (n->ch[0]) {
                        case 'b':       if (p == start) {
                                            if (isalnum(*p)) goto parent;
                                        } else if (p == end) {
                                            if (isalnum(p[-1])) goto parent;
                                        } else if (isalnum(p[-1]) ^ isalnum(p[0])) {
                                            goto parent;
                                        }
                                        goto die;
                        case 'B':       if (p == start) {
                                            if (!isalnum(*p)) goto parent;
                                        } else if (p == end) {
                                            if (!isalnum(p[-1])) goto parent;
                                        } else if (!(isalnum(p[-1]) & isalnum(p[0]))) {
                                            goto parent;
                                        }
                                        goto die;
                        case '^':       if (p == start) goto parent; 
                                        goto die;
                        case '$':       if (p == end) goto parent;
                                        goto die;
                        default:        goto die;       // shouldn't happen

                    }

                // If we come from above then get a new stack position and init, otherwise
                // count until we hit min, then spawn until max...
                case OP_MULT:
                    if (t->last == n->parent) {
                        if (t->sp == 0) {
                            // TODO: stack error
                            fprintf(stderr, "stack nesting too deep.\n");
                            goto die;
                        }
                        t->sp--;
                        t->stack[t->sp] = 0;
                    }
                    // If we've hit max, then go back up...
                    if (t->stack[t->sp] == n->max) { t->sp++; goto parent; }

                    // Normal op .. inc if under absolute max
                    if (t->stack[t->sp] < NO_MAX) t->stack[t->sp]++;
                    
                    // If we haven't hit min, then do b again...
                    if (t->stack[t->sp] <= n->min) goto leg_b;

                    // We must have hit min, so need to spawn...
                    if (n->lazy) {
                        t->next = task_new(ctx, t, t->next, n, n->b);
                        t->n = n->parent;
                        t->sp++;        // parent
                    } else {
                        t->next = task_new(ctx, t, t->next, n, n->parent);
                        t->next->sp++;  // parent
                        t->n = n->b;
                    }
                    t->last = n;
                    continue;

                case OP_GROUP:
                    // If we hit an empty group, then make sure we haven't just hit it,
                    // in which case we die otherwise we proceed back up to the parent
                    if (n->b == NOTUSED) {
                        if (t->lastghostmatch == n) {
                            // This is a second hit here, so we need to die to avoid
                            // inifinite tasks
                            goto die;
                        }
                        t->lastghostmatch = n;
                        t->n = n->parent;
                        t->last = n;
                        t->grp[n->group].rm_so = t->grp[n->group].rm_eo = (int32_t)(p - start);
                        continue;
                    }
                    if (t->last == n->b) {
                        // On the way back up... fill in the length
                        // TODO: do we want the first or last (i.e. only do it if it's currently -1?)
                        t->n = n->parent;
                        if (n->group != NO_GROUP) { t->grp[n->group].rm_eo = (int32_t)(p - start); }
                    } else {
                        // Going down leg b... mark the start
                        t->n = n->b;
                        // TODO: do we want the first or last (i.e. only do it if it's currently -1?)
                        if (n->group != NO_GROUP) { t->grp[n->group].rm_so = (int32_t)(p - start); }
                    }
                    t->last = n;
                    continue;

                // We always match a LF on either go around, but if not and we come from above then we must
                // match a CR and go again. On the second time around if doesn't matter if we don't match.
                case OP_CRLF:
                    if (ch == 10) {
                        t->last = n;
                        t->n = n->parent;
                        goto next;
                    }
                    if (t->last == n->parent) {
                        if (ch == 13) {
                            // Stay here for another go...
                            t->last = n;
                            goto next;
                        }
                        goto die;
                    }
                    goto parent;
                    
                // If we match we just go back up but let the next task run. If we
                // fail then we die.
                case OP_MATCH:
                    if (ch && matchone(n->ch, ch)) {
                        if (has_prior_match(ctx, run_list, n, t)) goto die;
                        t->last = n;
                        t->n = n->parent;
                        t->lastghostmatch = NULL;
                        goto next;
                    }
                    goto die;

                case OP_MATCHSET:
                    set = (struct set *)n->set;
                    if (match_set(ch, set)) {
                        if (has_prior_match(ctx, run_list, n, t)) goto die;
                        t->last = n;
                        t->n = n->parent;
                        t->lastghostmatch = NULL;
                        goto next;
                    }
                    goto die;

                // Match a previous group, coming from the top we initialise then stay
                // here iterating... originally we used a separate variable, but a stack
                // item should be fine.
                case OP_MATCHGRP:
                    if (t->last == n->parent) {
                        // A zero length group match is a ghost match...
                        if (t->grp[n->mgrp].rm_so == t->grp[n->mgrp].rm_eo) {
                            if (t->lastghostmatch == n) goto die;
                            t->lastghostmatch = n;
                            goto parent;
                        }
                        // We will use a stack entry for tracking position...
                        if (t->sp == 0) {
                            fprintf(stderr, "STACK OVERFLOW, MATCHGRP\n");
                            goto die;
                        }
                        t->sp--;
                        t->stack[t->sp] = t->grp[n->mgrp].rm_so;
                        t->lastghostmatch = NULL;
                    }
                    if (ch == start[t->stack[t->sp]]) {
                        if (has_prior_match(ctx, run_list, n, t)) goto die;
                        t->last = n;
                        t->stack[t->sp]++;
                        if (t->stack[t->sp] == t->grp[n->mgrp].rm_eo) { 
                            t->sp++; 
                            t->n = n->parent; 
                        }
                        goto next;
                    }
                    goto die;
                
                default:
                    fprintf(stderr, "unknown node op=%d (id=%d)\n", n->op, NODE_ID(ctx, n));
                    goto die;

// Reused outcomes for the different operations...

new_b_or_parent:    t->next = task_new(ctx, t, t->next, n, (n->lazy ? n->b : n->parent));
                    t->n = (n->lazy ? n->parent : n->b);
                    t->last = n;
                    continue;

leg_a:              t->n = n->a;
                    t->last = n;
                    continue;

leg_b:              t->n = n->b;
                    t->last = n;
                    continue;

parent:             t->n = n->parent;
                    t->last = n;
                    continue;

next:               prev = t;
                    t = t->next;
                    continue;

die:                if (prev) {
                        prev->next = t->next; task_release(ctx, t); t = prev->next;
                    } else {
                        run_list = t->next; task_release(ctx, t); t = run_list; continue;
                        continue;
                        goto tasks_done;
                        t = run_list;
                        // Run out of tasks for this go...
                        break;
                    }
                    continue;
            }
        }
tasks_done:
        p++;
    } while(p <= end);

    // Ok, we get here because we've run out of text or we've run out of tasks
    // or both.

done:
    // Move any tasks left on the run-list into the free list
    while (run_list) { struct task *x = run_list->next; task_release(ctx, run_list); run_list = x; }

//    if (!(flags & F_KEEP_TASKS)) {
//        while (ctx->free_list) { struct task *x = ctx->free_list->next; free(ctx->free_list); ctx->free_list = x; }
//    }
    if (ctx->done) return 1;
    return 0;
}




#include <stdio.h>

char *opmap(uint8_t op) {
    switch(op) {
        case OP_MATCH:      return "MATCH";
        case OP_CONCAT:     return "CONCAT";
        case OP_PLUS:       return "PLUS";
        case OP_STAR:       return "STAR";
        case OP_NOP:        return "NOP";
        case OP_QUESTION:   return "QUESTION";
        case OP_ALTERNATE:  return "ALTERNATE";
        case OP_DONE:       return "DONE";
        case OP_GROUP:      return "GROUP";
        case OP_MATCHSET:   return "MATCHSET";
        case OP_MULT:       return "MULT";
        case OP_MATCHGRP:   return "MATCHGRP";
        case OP_CRLF:       return "CRLF";
        case OP_ANCHOR:     return "ANCHOR";
        default:            return "UNKNOWN";
    }
}

void dump_dot(struct rectx *ctx, struct node *n, FILE *f) {
    if (!n) return;

    int chars;

#define GEND fprintf(f, "\"];\n")

#ifdef DEBUG_ID
    fprintf(f, "    n%p [label=\"(%d)\n%s\n", (void *)n, NODE_ID(ctx, n), opmap(n->op));
#else
    fprintf(f, "    n%p [label=\"%s\n", (void *)n, opmap(n->op));
#endif

    switch(n->op) {
        case OP_MATCH:
            if (n->ch[0] == '\\') {
                fprintf(f, "'\\%c'", n->ch[1]);
            } else {
                fprintf(f, "'%c'", n->ch[0]);
            }
            GEND;
            return;

        case OP_ANCHOR:
            fprintf(f, "'%c'", n->ch[0]);
            GEND;
            return;

        case OP_MATCHSET:
            chars = __builtin_popcount(n->set->d[0]);
            chars += __builtin_popcount(n->set->d[1]);
            chars += __builtin_popcount(n->set->d[2]);
            chars += __builtin_popcount(n->set->d[3]);
            fprintf(f, "%d chars", chars);
            GEND;
            return;

        case OP_MATCHGRP:
            fprintf(f, "%d", n->mgrp);
            GEND;
            return;

        case OP_GROUP:
            if (n->group == NO_GROUP) {
                fprintf(f, "nocapture");
            } else {
                fprintf(f, "%d", n->group);
            }
            GEND;
            goto bonly;

        case OP_MULT:
            if (n->lazy) { 
                fprintf(f, "min=%d max=%d lazy", n->min, n->max);
            } else {
                fprintf(f, "min=%d max=%d", n->min, n->max);
            }
            GEND;
            goto bonly;

        case OP_CRLF:
            GEND;
            return;

        case OP_PLUS:
        case OP_QUESTION:
        case OP_STAR:
            if (n->lazy) {
                fprintf(f, "lazy");
            }
            GEND;
            break;

        default:
            GEND;
    }

    if (n->a && (n->a != NOTUSED)) {
        fprintf(f, "    n%p -> n%p [label=\"a\"];\n", (void*)n, (void*)n->a);
        dump_dot(ctx, n->a, f);
    }

bonly:
    if (n->b && (n->b != NOTUSED)) {
        fprintf(f, "    n%p -> n%p [label=\"b\"];\n", (void*)n, (void*)n->b);
        dump_dot(ctx, n->b, f);
    }
}

void export_tree(struct rectx *ctx, const char *filename) {
    FILE *f = fopen(filename, "w");
    fprintf(f, "digraph tree {\n");
    dump_dot(ctx, ctx->root, f);
    fprintf(f, "}\n");
    fclose(f);
}


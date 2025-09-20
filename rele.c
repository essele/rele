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

// Include an ID in the nodes to help with debugging and tree visualisation
#define DEBUG_ID    


// TODO: this will need to be in an include file....
// Compile flags...
#define F_CASELESS  (1 << 0)



enum {
    OP_NOP = 0, OP_CONCAT, OP_MATCH, OP_MATCHGRP, OP_DONE,
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
#ifdef DEBUG_ID
    uint32_t            id;         // for debug purposes only
#endif

    union {
       struct node      *a;         // the first child
       struct {
            uint16_t    min;
            uint16_t    max;
        };
       uint8_t          group;      // for creating groups
       uint8_t          lazy;       // for PLUS, STAR, and QUESTION
    };
    union {
        struct node     *b;         // the second child
        struct set      *set;       // a possible set match
        uint8_t         mgrp;       // a possible group match
        char            ch[2];      // can store matched char
    };
    struct node         *parent;        // for a way back
    uint8_t             op;             // which operation?
    // Three bytes unused here...
};



// Where we have nodes that don't need children we need to mark the
// b-leg otherwise it will be used by something...
#define NOTUSED     (struct node *)1


// We have a 'context' which contains the root of the tree
struct rectx {
    struct node     *root;
    uint32_t        flags;
};


// We will need to consider more efficient memory management for the creation
// and destruction of nodes, but for now we will just use malloc.

struct node *node_alloc() {
    static uint32_t node_id = 1;

    struct node *n = (struct node *)malloc(sizeof(struct node));
    if (!n) return NULL;
    memset((void *)n, 0, sizeof(struct node));
#ifdef DEBUG_ID
    n->id = node_id++;
#endif
    return n;
}
void node_free(struct node *n) {
#ifdef DEBUG_ID
    fprintf(stderr, "Freeing node %d\n", n->id);
#endif
    free(n);
}

static struct node *create_node_above(struct rectx *ctx, struct node *this, uint8_t op, struct node *a, struct node *b) {
    struct node *parent = this->parent;
    struct node *n = node_alloc();
    if (!n) return NULL;
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
    struct node *n = node_alloc();
    if (!n) return NULL;
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
        if (!n->parent) goto fail;
    }
    return n;

fail:
    node_free(n);
    return NULL;
}

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
    // Allocate the space...
    struct set *set = malloc(sizeof(struct set));
    if (!set) return NULL;
    memset((void *)set, 0, sizeof(struct set));

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
                // TODO: case!
                SET_VAL(*p);
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
    free(set);
    return NULL;
}

static void set_free(struct set *set) {
    free(set);
}


// Process a min/max spec and update the supplied node accordingly
char *minmax(char *p, struct node *n) {
    uint16_t min = 0, max = 0;
    p++;                // get past '{'

    // First check we have the required digit...
    if (!isdigit((int)*p)) return NULL;

    // Now work out the first number...
    while (isdigit((int)*p)) min = (min * 10) + (*p++ - '0');

    if (*p == '}') { max = min; goto done; }        // An exact number
    if (*p++ != ',') return NULL;                   // Need a comma
    if (*p == '}') { max = NO_MAX; goto done; }     // No second number

    // Now get the second number...
    while (isdigit((int)*p)) max = (max * 10) + (*p++ - '0');

    // Now must be a close bracket and right order...
    if (*p != '}' || max < min) return NULL;

done:
    n->min = min;
    n->max = max;
    return p;
}


// ------------------------------------------------------------------------
// Simple compiler that turns a regular expression into a binary tree
// ------------------------------------------------------------------------

struct rectx *re_compile(char *regex, uint32_t flags) {
    // First allocate the ctx structure...
    struct rectx *ctx = (struct rectx *)malloc(sizeof(struct rectx));
    if (!ctx) return NULL;
    memset((void *)ctx, 0, sizeof(struct rectx));
    ctx->flags = flags;

    // Now run through the regex...
    char        *p = regex;
    struct node *last = NULL;
    uint8_t     group = 1;
    int         lazy;

    while (*p) {
        switch (*p) {
            case OP_PLUS:
            case OP_STAR:
            case OP_QUESTION:
                lazy = (p[1] == '?' ? 1 : 0);
                last = create_node_above(ctx, last, (uint8_t)*p, NULL, last);
                last->lazy = lazy;
                p += lazy;              // skip the ? if we have it
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
                    last->group = group++;
                }
                break;

            case OP_MULT:
                last = create_node_above(ctx, last, OP_MULT, NULL, last);
                p = minmax(p, last);
                break;

            case ')':       // ending a group
                while (last && last->op != OP_GROUP) { last = last->parent; }
                break;

            case OP_BEGIN:
            case OP_END:
                last = create_node_here(ctx, last, (uint8_t)*p, NULL, NOTUSED);
                break;

            case OP_MATCHSET:
                last = create_node_here(ctx, last, OP_MATCHSET, NULL, NULL);
                if (!last) goto fail;
                p = build_set(ctx, p, last);
                if (!p) goto fail;
                break;

            default:
                if (*p == '\\' && p[1] >= '1' && p[1] <= '9') {
                    // This is a \1 type match on a group
                    last = create_node_here(ctx, last, OP_MATCHGRP, NULL, NULL);
                    if (!last) goto fail;
                    last->mgrp = (uint8_t)p[1] - '0';
                    p++;                 
                } else {
                    // Normal char, escaped, or class match...
                    last = create_node_here(ctx, last, OP_MATCH, NULL, (struct node *)p);
                    last->ch[0] = *p;
                    if (*p == '\\') {
                        last->ch[1] = *++p;
                        if (!*p) goto fail;         // backslash on the end!
                    }
                }
            
        }
        p++;
    }
    return ctx;

fail:
    // todo -- destroy everything
    return NULL;
}

// Walk the tree and free all of the things that might have been allocated...
void tree_free(struct rectx *ctx) {
    struct node *n = ctx->root;
    struct node *last;

    while (n) {
        switch(n->op) {


            case OP_CONCAT:
            case OP_ALTERNATE:
                if (last == n->a) goto down_b;
                if (last == n->b) goto free_and_up;
                goto down_a;
            
            case OP_MULT:
            case OP_GROUP:
            case OP_PLUS:
            case OP_QUESTION:
            case OP_STAR:
                if (last == n->b) goto free_and_up;
                goto down_b;

            case OP_MATCHSET:
                set_free(n->set);       // fall through

            case OP_MATCH:
            case OP_MATCHGRP:
            case OP_BEGIN:
            case OP_END:
                goto free_and_up;


            default:
                fprintf(stderr, "ERROR: unknown node during tree_free (op=%d id=%d)\n", n->op, n->id);
                break;

down_a:
                last = n;
                n = n->a;
                break;

down_b: 
                last = n;
                n = n->b;
                break;

free_and_up:    
                last = n;
                n = n->parent;
                node_free(last);
                break;
        }
    }

}



#include <stdio.h>

char *opmap(uint8_t op) {
    switch(op) {
        case OP_MATCH:  return "MATCH";
        case OP_BEGIN:  return "BEGIN";
        case OP_END:    return "END";
        case OP_CONCAT: return "CONCAT";
        case OP_PLUS:   return "PLUS";
        case OP_NOP:    return "NOP";
        case OP_QUESTION:   return "QUESTION";
        case OP_ALTERNATE:  return "ALTERNATE";
        case OP_DONE:   return "DONE";
        case OP_GROUP:  return "GROUP";
        case OP_MATCHSET:   return "MATCHSET";
        case OP_MULT:   return "MULT";
        case OP_MATCHGRP: return "MATCHGRP";
        default:        return "UNKNOWN";
    }
}

void dump_dot(struct node *n, FILE *f) {
    if (!n) return;

    int chars;

    #define GEND fprintf(f, "\"];\n")

#ifdef DEBUG_ID
    fprintf(f, "    n%p [label=\"(%d)\n%s\n", (void *)n, n->id, opmap(n->op));
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
            fprintf(f, "min=%d max=%d", n->min, n->max);
            GEND;
            goto bonly;

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
        dump_dot(n->a, f);
    }

bonly:
    if (n->b && (n->b != NOTUSED)) {
        fprintf(f, "    n%p -> n%p [label=\"b\"];\n", (void*)n, (void*)n->b);
        dump_dot(n->b, f);
    }
}

void export_tree(struct node *root, const char *filename) {
    FILE *f = fopen(filename, "w");
    fprintf(f, "digraph tree {\n");
    dump_dot(root, f);
    fprintf(f, "}\n");
    fclose(f);
}

int main(int argc, char *argv[]) {
//    struct rectx *ctx = re_compile("abc?def+ghi", 0);
//    struct rectx *ctx = re_compile("abc(def|ghi)jkl[a-z]", 0);
//    struct rectx *ctx = re_compile("[a-z]", F_CASELESS);
    struct rectx *ctx = re_compile("ab(?:cd|ef){2,4}g+?\\1[a-z]$", 0);
    export_tree(ctx->root, "tree.dot");


    tree_free(ctx);
    free(ctx);
//    struct set *s = build_set("[a-zA-Z0-9]");
//    printf("Set: %08x %08x %08x %08x\n", (uint32_t)s->d[0], (uint32_t)s->d[1], (uint32_t)s->d[2], (uint32_t)s->d[3]);


}



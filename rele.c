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


enum {
    OP_NOP = 0, OP_CONCAT, OP_MATCH, OP_GMATCH, OP_DONE,
    OP_ALTERNATE = '|',
    OP_PLUS = '+', OP_STAR = '*', OP_QUESTION = '?', OP_GROUP = '(',
    OP_BEGIN = '^', OP_END = '$', OP_MULT = '{',
    OP_MATCHSET = '[',
};

// ============================================================================
// Binary tree bits...
// ============================================================================

struct node {
    struct node         *a;             // the first child
    union {
        struct node     *b;         // the second child
        struct set      *set;       // a possible set match
        char            ch[2];      // can store matched char
    };
    struct node         *parent;        // for a way back
    uint8_t             op;             // which operation?
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
    struct node *n = (struct node *)malloc(sizeof(struct node));
    if (!n) return NULL;
    memset((void *)n, 0, sizeof(struct node));
    return n;
}
void node_free(struct node *n) {
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
struct set {
    uint32_t d[4];
};

static char *build_set(char *p, struct node *n) {
    // Allocate the space...
    struct set *set = malloc(sizeof(struct set));
    if (!set) return NULL;
    memset((void *)set, 0, sizeof(struct set));

    int negate = 0;

    #define SET_VAL(v)      set->d[(v)/32] |= (1 << ((v)%32))
    #define SET_RANGE(beg, end)     for (uint8_t c = beg; c <= end; c++) set->d[c/32] |= (1 << (c%32))

    p++;            // get past the '['
    if (*p == '^') { negate = 1; p++; }
    while (1) {
        if (*p == ']') break;           // done
        if (p[1] == '-' && p[2] && p[2] != ']') {
            if (p[0] > p[2]) goto fail;
            // todo: case
            for (int i=p[0]; i <= p[2]; i++) {
                int word = i / 32;
                int bit = i % 32;
                set->d[word] |= (1 << bit);
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

    while (*p) {
        fprintf(stderr, "looking at [%c]\n", *p);
        switch (*p) {
            case OP_PLUS:
            case OP_STAR:
            case OP_QUESTION:
                last = create_node_above(ctx, last, (uint8_t)*p, NULL, last);
                break;

            case OP_ALTERNATE:
                // Get to the previous thing...
                while (last->parent && last->parent->op == OP_CONCAT) { last = last->parent; }
                last = create_node_above(ctx, last, OP_ALTERNATE, last, NULL);
                break;

            case OP_GROUP:
                last = create_node_here(ctx, last, OP_GROUP, NULL, NULL);
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
                p = build_set(p, last);
                fprintf(stderr, "HERE p is %d (%c)\n", *p, *p);
                if (!p) goto fail;
                break;

            default:
                if (*p == '\\' && p[1] >= '1' && p[1] <= '9') {
                    // This is a \1 type match on a group                 
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
        case OP_GMATCH: return "GMATCH";
        default:        return "UNKNOWN";
    }
}

void dump_dot(struct node *n, FILE *f) {
    if (!n) return;



    if (n->op == OP_MATCH) {
        fprintf(f, "    n%p [label=\"%s '%c", (void *)n, opmap(n->op), n->ch[0]);
        if (n->ch[0] == '\\') {
            fprintf(f, "%c", n->ch[1]);
        }
        fprintf(f, "'\"];\n");
//        fprintf(f, "    n%p [label=\"%d %c%c\"];\n", (void*)n, n->op, n->ch[0], n->ch[1]);
        return;
    } else if (n->op == OP_MATCHSET) {
        int chars = __builtin_popcount(n->set->d[0]);
        chars += __builtin_popcount(n->set->d[1]);
        chars += __builtin_popcount(n->set->d[2]);
        chars += __builtin_popcount(n->set->d[3]);
        fprintf(f, "    n%p [label=\"%s %d chars\"];\n", (void *)n, opmap(n->op), chars);
        return;
    } else {
        fprintf(f, "    n%p [label=\"%s\"];\n", (void *)n, opmap(n->op));
    }

    if (n->a && (n->a != NOTUSED)) {
        fprintf(f, "    n%p -> n%p [label=\"a\"];\n", (void*)n, (void*)n->a);
        dump_dot(n->a, f);
    }
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
    struct rectx *ctx = re_compile("abc(def|ghi)jkl[^a-z]", 0);
    export_tree(ctx->root, "tree.dot");

//    struct set *s = build_set("[a-zA-Z0-9]");
//    printf("Set: %08x %08x %08x %08x\n", (uint32_t)s->d[0], (uint32_t)s->d[1], (uint32_t)s->d[2], (uint32_t)s->d[3]);


}



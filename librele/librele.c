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

#include "rele.h"

// Include an ID in the nodes to help with debugging and tree visualisation
#define DEBUG_ID    


enum {
    // Order in terms of liklihood
    OP_CONCAT,
    OP_MATCH,
    OP_MATCHSTR,

    OP_PLUS,
    OP_STAR,
    OP_DOTSTAR,
    OP_QUESTION,

    OP_GROUP,
    OP_ALTERNATE,

    OP_ANCHOR, 
    OP_MATCHSET,
    OP_MATCHGRP,
    OP_MULT, 
    OP_CRLF,
 
    OP_DONE,
};

// ============================================================================
// Binary tree bits...
// ============================================================================

#define NO_MAX              0xffff
#define NO_GROUP            0xff

struct node {
    union {
        struct node      *a;            // the first child
        struct {
            uint16_t    min;
            uint16_t    max;
        };
        int             len;            // for string matches
        uint8_t         group;          // for creating groups
    };
    union {
        struct node     *b;             // the second child
        struct set      *set;           // a possible set match
        char            *string;        // a possible string match
        struct node     *match;         // a DOTSTAR next match node
        uint8_t         mgrp;           // a possible group match
        struct {
            char            ch1;        // normal char
            char            ch2;        // or special char
        };
    };
    struct node         *parent;        // for a way back
    uint8_t             op;             // which operation?
    uint8_t             lazy;           // won't fit in a with minmax
    // Two bytes unused here (if 32bit)

    // A 32bit value here to allow us to detect zero length matches
    uint32_t            iter;
};

// Where we have nodes that don't need children we need to mark the
// b-leg otherwise it will be used by something...
#define NOTUSED     (struct node *)1


// We have a 'context' which contains the root of the tree
struct rectx {
    struct node     *root;


    struct node     *nodes;         // current node pointer
    struct set      *sets;          // current set pointer
    char            *strings;       // for string matches

    struct task     *free_list;     // free tasks list
    struct task     *done;          // the candiate completed task

    struct node     *fast_start;    // used for optimisation

    uint16_t        flags;
    uint8_t         groups;         // allows up to 255 groups
    uint8_t         pad;            // not used

    // Memory for nodes and sets will follow this...
};

#define NOT_FLAG(v, f)           (!(v & f))
#define HAS_FLAG(v, f)           (v & f)

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
    uint16_t            sp;                 // more an index than pointer (smaller)
    uint16_t            stack[TASK_STACK_SIZE];

    // All of the group matches follow...
    struct rele_match_t   grp[];
};

static int tcount = 0;      // TODO: might want to remove and use #defined debug code


/*
 * Some helper functions
 */
int rele_match_count(struct rectx *ctx) { return ctx->groups; }
struct rele_match_t *rele_get_match(struct rectx *ctx, int n) { return &(ctx->done->grp[n]); }
struct rele_match_t *rele_get_matches(struct rectx *ctx) { return ctx->done->grp; }


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

//
// TODO: if caseless, just put lowercase stuff in!
// TODO: do we need to support hex to hex [0x30-0x39] ??
//
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
            if (ctx->flags & RELE_CASELESS) {
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
                if (ctx->flags & RELE_CASELESS) {
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
    return ++p;                 // get past the close bracket

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
    return ++p;         // get past the close bracket
}

static inline int match_set(char ch, struct set *set) {
    if (set->d[ch/32] & ((uint32_t)(1 << (ch % 32)))) {
        return 1;
    }
    return 0;
}

// Process a min/max spec and update the supplied node accordingly
char *minmax(char *p, struct node *n) {
    uint16_t min = 0, max = 0;
    p++;                // get past '{'

    // First check we have the required digit or a comma
    if (*p != ',' && !isdigit((int)*p)) return NULL;

    // Now work out the first number...
    while (isdigit((int)*p)) { min = (min * 10) + (*p++ - '0'); if (min > 1000) return NULL; }

    // Leading zero check for min
    if (*p == '0' && isdigit((int)*(p+1))) return NULL;

    if (*p == '}') { max = min; goto done; }        // An exact number
    if (*p++ != ',') return NULL;                   // Need a comma
    if (*p == '}') { max = NO_MAX; goto done; }     // No second number

    // Leading zero check for max
    if (*p == '0' && isdigit((int)*(p+1))) return NULL;

    // Now get the second number...
    while (isdigit((int)*p)) { max = (max * 10) + (*p++ - '0'); if (max > 1000) return NULL; }

    // Now must be a close bracket and right order...
    if (*p != '}' || max < min) return NULL;

done:
    if (n) {
        n->min = min;
        n->max = max;
    }
    return ++p;         // get past the bracket
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
    if (newp) *newp = p;
    return 1;

error:
    if (newp) *newp = NULL;
    if (errp) *errp = p;
    return 1;
}

static inline int hexval(unsigned char c) {
    unsigned d = c - '0';
    unsigned m = (unsigned)(c - 'A') <= 5; // true if 'A'..'F'
    unsigned n = (unsigned)(c - 'a') <= 5; // true if 'a'..'f'
    return (d <= 9) * d
         + m * (c - 'A' + 10)
         + n * (c - 'a' + 10);
}
static inline int tohex(const char *p) {
    return (hexval(p[0]) << 4) | hexval(p[1]);
}

// ------------------------------------------------------------------------
// NEW WORK ON DOTSTAR
//
// Walk the tree, and update dotstar optimisation if we can, in an ideal
// world, we might also be able to use this for faststart.
// ------------------------------------------------------------------------
struct node *optimiser(struct rectx *ctx) {
    struct node *n = ctx->root;
    struct node *dotstar = NULL;
    struct node *last = NULL;

    while (n) {
        switch (n->op) {
            case OP_MATCH:
            case OP_MATCHSTR:
                if (dotstar) { dotstar->match = n; dotstar = NULL; }
                goto parent;

            case OP_MATCHGRP:
            case OP_MATCHSET:
            case OP_ANCHOR:
            case OP_CRLF:
                goto parent;

            // TODO: If we have concurrent dotstar's can we somehow inherit
            // the match? Not sure if easy or possible? or sensible.
            case OP_DOTSTAR:
                dotstar = n;
                dotstar->match = NULL;
                goto parent;

            case OP_CONCAT:
                if (last == n->a) goto leg_b;
                if (last == n->b) goto parent;
                goto leg_a;

            case OP_ALTERNATE:
                dotstar = NULL;
                if (last == n->a) goto leg_b;
                if (last == n->b) goto parent;
                goto leg_a;

            case OP_QUESTION:
                dotstar = NULL;
                if (last == n->b) goto parent;
                goto leg_b;

            // If we're coming up from a plus then we can't continue with the
            // dotstar search as the next match could be a repeat of whatever is below.
            // Going down into a plus is fine, as whatever is below will need to match.
            case OP_PLUS:
                if (last == n->b) { dotstar = NULL; goto parent; }
                goto leg_b;

            // Either way on a star kills the dotstar search...
            case OP_STAR:
                dotstar = NULL;
                if (last == n->b) goto parent;
                goto leg_b;                

            case OP_GROUP:
                if (n->b == NOTUSED) goto parent;
                if (last == n->b) goto parent;
                goto leg_b;

            case OP_DONE:
                goto done;

            default:
                fprintf(stderr, "INVALID OP: %d\n", n->op);
                return NULL;
    

leg_a:      last = n;
            n = n->a;
            continue;

leg_b:      last = n;
            n = n->b;
            continue;

parent:     last = n;
            n = n->parent;
            continue;

        }
done:       break;
    }
    return NULL;
}



// ------------------------------------------------------------------------
// Optimisation detector...
//
// We will walk through the start of the tree and see if we can identify
// anything that will enable us to optimise in some way.
//
// Initially we'll look to see if there's an unconditional match, and then
// we can expand this to set matches and plus, and non-zero {m,n} items.
// If we know this then we can linearly search for sensible start points
// in the text rather than trying at each input char.
//
// This is quite simple as we only ever need to follow the left-most down
// path ... through group, plus, concat, but not star, ?, or alternate.
// ------------------------------------------------------------------------
struct node *fast_start(struct rectx *ctx) {
    struct node *n = ctx->root;

    while (n) {
        switch(n->op) {
            case OP_MATCH:
                if (n->ch2 == '.')  return NULL;    // match any makes no sense
                return n;

            case OP_MATCHSTR:
            case OP_MATCHSET:       return n;       // this should be fine

            // For a star we only support a .* construct to avoid running
            // the regex for each char.
            case OP_STAR:  
                if (n->b->op == OP_MATCH) {
                    if (n->b->ch2 == '.') return n;
                }
                return NULL;

            case OP_GROUP:          n = n->b; continue;

            case OP_CONCAT:         n = n->a; continue;

            case OP_PLUS:           n = n->b; continue;

            case OP_MULT:           if (n->min > 0) n = n->b; continue;

            // None of these are viable...
            case OP_DOTSTAR:
            case OP_ALTERNATE:
            case OP_ANCHOR:
            case OP_CRLF:
            case OP_DONE:
            case OP_MATCHGRP:
            case OP_QUESTION:
                return NULL;

            default:
                fprintf(stderr, "Missed NODE: %d op=%d\n", NODE_ID(ctx, n), n->op);
                return NULL;
        }
    }
    return NULL;        // shouldn't get here
}

/**
 * Some utility functions
 */

 /**
  * A fast tolower() implementation, should be branch-free on ARM...
  */
static inline unsigned char fast_tolower(unsigned char c)
{
    // If 'A' <= c <= 'Z', set bit 5 (0x20), else leave unchanged
    unsigned char is_upper = (unsigned)((c - 'A') <= ('Z' - 'A'));
    return c + (is_upper * 32);
}

/**
 * My version of strchr that doesn't do the null byte bit!
 */
static inline char *rele_strchr(char *str, int c) {
	while (*str) {
		if (*str == c) return str;
		str++;
	}
	return NULL;
}

/**
 * My version of casecmp that hopefully uses a faster tolower() and is written
 * so that its compiles well on ARM.
 */
static inline int rele_strncasecmp(const char *s1, const char *s2, int n)
{
    while (n--) {
        unsigned char c1 = fast_tolower((unsigned char)*s1++);
        unsigned char c2 = fast_tolower((unsigned char)*s2++);
        if (c1 != c2)
            return 0;
    }
    return 1;
}

// TODO: use a Boyer-Moore-Horspool version of this with a reduced cache
// can probably get away with 32 bytes.
static inline char *rele_strifind(const char *haystack, int hlen, const char *needle, int nlen)
{
    if (nlen <= 0 || hlen < nlen)
        return NULL;

    const char *end = haystack + hlen - nlen + 1;
    for (; haystack < end; haystack++) {
        if (rele_strncasecmp(haystack, needle, nlen))
            return (char *)haystack;
    }
    return NULL;
}

/**
 * Looks for a string of non-special chars that can form a string that we can
 * search for quickly. Returns a new p, and optionally copies into cpy and
 * puts a length in len.
 * 
 * TODO: at the moment this joins quoted strings with normal text which is
 *       great unless there is a + or something after the quoted string
 *       in which case we end up plussing the whole thing.
 *       Easy option would be to treat quoted strings as individual items
 *       then perhaps look for options to merge strings if they are just
 *       concatenated?
 * 
 * TODO: caseless
 */
char *find_string(char *p, char *str, int *len, char *ch, int icase) {
	char c;			// single return char
	int l = 0;		// len tracking
	int quoted = 0;	// are we in a quoted section

	while (*p) {
		// First deal with the quoted situation...
		if (quoted) {
			if (!*p) goto error;
			if (*p == '\\') {
				if (!p[1]) goto error;			// end after a backslash
				if (p[1] == 'E') { quoted = 0; p += 2; continue; }
			}
			goto normal_char;
		}

		// Do we need to end the current run...
		// Basic chars first...
		if (rele_strchr(".+?*|()[]{}^$", p[0])) break;

		// Then backslash variants...
		if (*p == '\\') {
			if (rele_strchr("dDwWsSbBRg{1234567890", p[1])) break;
			if (!p[1]) goto error;
		}

		// Ok, at this point we are fairly sure the character is valid, lets
		// figure out what it is...
		if (*p == '\\') {
			switch (p[1]) {
				case 'Q':		p += 2;			// point at first char of quote
								quoted = 1;
								continue;

				case 'x':		c = 'A'; break;		// TODO handle hex
				case 'n':		c = '\n'; break;
				case 't':		c = '\t'; break;
				case ',':		c = ','; break;
				default:	fprintf(stderr, "unknown backslash [%c]\n", p[1]); goto error;
			}
			p += 2;
		} else {
normal_char:
			c = *p++;
		}

		// Ok, so we have a candidate char, we need to make sure it's not followed
		// by something that would cause a problem (+?*), if so we need to roll it back.
        // If we are a single char though, we need to return that.
		if (rele_strchr("+?*", *p)) { 
            if (l) { p--; break; }
            if (ch) *ch = icase ? fast_tolower(c) : c;
			l = 1; break; 
        }

		// So here we think it's valid...
		if (ch) *ch = icase ? fast_tolower(c) : c;
		if (str) *str++ = icase ? fast_tolower(c) : c;
		l++;
	}
	if (len) *len = l;
	return p;

error:
	return NULL;
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
    int strings = 0;
    int slen;

    char *p = regex;
    while (*p) {
        // Start out by seeing if we have a string here ....
        p = find_string(p, NULL, &slen, NULL, 0);
        if (!p) return NULL;
        if (slen > 1) {
            matches++;
            strings += slen;
            continue;
        } else if (slen == 1) {
            matches++;
            continue;
        }
        // Otherwise we can deal with everything else...

        // There's always a match at the end of a given brach, therefore matches
        // are the key. We will always have one less "splits" (i.e. concat or 
        // alternate) than we have matches, everything else is always a node.
        switch (*p) {
            case '{':
                p = minmax(p, NULL);
                if (!p) return NULL;        // min max error
                if (*p == '?') p++;         // lazy version
                nodes++;
                continue;                   // p is already incrememented

            // These are always a node...
            // TODO: DOTSTAR!!!!
            case '*':
                if (p > regex && p[-1] == '.') nodes--;
                // Fall through...

            case '+': case '?':
                if (p[1] == '?') p++;       // lazy version
                nodes++;
                break;

            // An empty group counts as a node and a match...
            case '(':
                if (p[1] == ')' || (p[1] == '?' && p[2] == ':' && p[3] == ')')) { matches++; }
                nodes++;
                break; 

            // Ignore these (alternate we cover via matches)...
            case '|': case ')':
                break;

            case '[':
                p = dummy_set(p);
                if (!p) return NULL;        // set error (not impl)
                sets++;
                matches++;
                continue;                   // p will already be incremented

            // These are effectively matches...
            case '^': case '$':
                matches++;
                break;

            // With the new approach to strings, this can only be a few things
            // anchors, CRLF, \d, \w etc, dot, and group references
            case '.':
                matches++;
                break;

            case '\\':
                p++;
                matches++;
                if (!*p) return NULL;
                if (is_group(p, NULL, &p, NULL)) {
                    if (!p) return NULL;        // group syntax error
                    continue;                   // p will be correct
                }
                break;
                
            default:
                fprintf(stderr, "Unknown char in compile (pass1) [%c]\n", *p);
                return NULL;
        }
        p++;
    }

    // We need one less splitter than matches
    int splits = matches - 1;
    // We also need space for our extra added nodes
    //nodes += matches + splits + 6;
    nodes += matches + splits + 3;

    // Allow an extra char...
    strings++;

//    fprintf(stderr, "Matches = %d, Splits = %d, Nodes = %d\n", matches, splits, nodes);

    struct rectx *ctx = malloc(sizeof(struct rectx) +
                                (nodes * sizeof(struct node)) + 
                                (sets * sizeof(struct set)) + strings);
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(struct rectx) +
                                (nodes * sizeof(struct node)) + 
                                (sets * sizeof(struct set)) + strings);

    ctx->nodes = (struct node *)((void *)ctx + sizeof(struct rectx));
    ctx->sets = (struct set *)((void *)ctx + (sizeof(struct rectx) + (nodes * sizeof(struct node))));
    ctx->strings = (void *)ctx->sets + (sets * sizeof(struct set));
    return ctx;
}

// TODO: testing only
char sss[2048];

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
    int         icase = flags & RELE_CASELESS;

    // For string finding...
    int         slen;
    char        ch;

    // Early part of the tree....
    last = create_node_here(ctx, last, OP_GROUP, NULL, NULL);
    last->group = 0;

    while (*p) {
        // Start out by seeing if we have a string here ....
        p = find_string(p, ctx->strings, &slen, &ch, icase);
        if (!p) goto fail;
        if (slen > 1) {
            // TODO: add a string object
            last = create_node_here(ctx, last, OP_MATCHSTR, NULL, NULL);
            last->string = ctx->strings;
            last->len = slen;
            ctx->strings += slen;
            continue;
        } else if (slen == 1) {
            last = create_node_here(ctx, last, OP_MATCH, NULL, NULL);
            if (!last) goto fail;
            last->ch1 = icase ? fast_tolower(ch) : ch;
            continue;
        }

        // Otherwise we can deal with everything else...
        switch (*p) {
            case '+':
                last = create_node_above(ctx, last, OP_PLUS, NULL, last);
                goto star_plus_question;
            case '*':
                if (last && last->op == OP_MATCH && last->ch2 == '.') {
                    last->op = OP_DOTSTAR;
//                    last->match = NULL;
                } else {
                    last = create_node_above(ctx, last, OP_STAR, NULL, last);
                }
                goto star_plus_question;
            case '?':
                last = create_node_above(ctx, last, OP_QUESTION, NULL, last);
                // fall through

star_plus_question:
                lazy = (p[1] == '?' ? 1 : 0);
                last->lazy = lazy;
                p += lazy;              // skip the ? if we have it
                break;

            case '|':
                // Get to the previous thing...
                while (last->parent && last->parent->op == OP_CONCAT) { last = last->parent; }
                last = create_node_above(ctx, last, OP_ALTERNATE, last, NULL);
                break;

            case '(':
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

            case '{':
                last = create_node_above(ctx, last, OP_MULT, NULL, last);
                p = minmax(p, last);
                if (!p) goto fail;
                if (*p == '?') { 
                    last->lazy = 1; 
                    p++; 
                }
                continue;           // p is already incremented


            case '^':
                last = create_node_here(ctx, last, OP_ANCHOR, NULL, NULL);
                last->ch1 = (flags & RELE_NEWLINE) ? '^' : 'A';
                break;

            case '$':
                last = create_node_here(ctx, last, OP_ANCHOR, NULL, NULL);
                last->ch1 = (flags & RELE_NEWLINE) ? '$' : 'Z';
               break;

            case '[':
                last = create_node_here(ctx, last, OP_MATCHSET, NULL, NULL);
                if (!last) goto fail;
                p = build_set(ctx, p, last);
                if (!p) goto fail;
                continue;                   // p will already be incremented


            // With the new approach to strings, this can only be a few things
            // anchors, CRLF, \d, \w etc, dot, and group references
            case '.':
                last = create_node_here(ctx, last, OP_MATCH, NULL, NULL);
                if (!last) goto fail;
                last->ch2 = (flags & RELE_NEWLINE) ? ',' : '.';
                break;

            case '\\':
                p++;
                last = create_node_here(ctx, last, OP_MATCH, NULL, NULL);
                if (is_group(p, &(last->mgrp), &p, NULL)) {
                        if (!p) goto fail;          // group syntax error
                        last->op = OP_MATCHGRP;
                        continue;                   // p will be correct
                }
                switch (*p) {
                    case 0:     goto fail;
                    case 'R':   last->op = OP_CRLF; break;
                    case 'A':
                    case 'Z':
                    case 'b':
                    case 'B':   last->op = OP_ANCHOR; last->ch2 = *p; break;
                    default:    last->ch2 = *p; break;  // \w \d etc.
                }
                break;
            
            default:
                fprintf(stderr, "Unknown char in compile (pass2) [%c]\n", *p);
                goto fail;   
        }
        p++;
    }
    // Postprocessing just needs to ensure there's a DONE in the right place
    // We put it after the group b node to save one more parent move.
    //last = create_node_here(ctx, ctx->root->b, OP_DONE, NULL, NULL);
    last = create_node_here(ctx, ctx->root, OP_DONE, NULL, NULL);

    // Run the optimisation check...
    if (NOT_FLAG(flags, RELE_NO_FASTSTART)) {
        ctx->fast_start = fast_start(ctx);
    }

    optimiser(ctx);

    return ctx;

fail:
    free(ctx);
    return NULL;
}


// -------------------------------------------------------------------------------
// Simple matching with escapes and classes
// -------------------------------------------------------------------------------
static int matchone(char s, char ch) {
    if (s == '.')   return 1;           // TODO: newlines and stuff
    // NEED A SPECIAL VERSION OF DOT WHEN MULTILINE THAT DOESN"T MATECH NL ETC

    if (s == ',') return (ch != '\n');
    
    switch(s) {
        // Types...
        case 'd':       return isdigit(ch);
        case 'D':       return !isdigit(ch);
        case 'w':       return isalnum(ch);
        case 'W':       return !isalnum(ch);
        case 's':       return isspace(ch);
        case 'S':       return !isspace(ch);

        // TODO: needs to fail in compile rather than here
        default:
                        fprintf(stderr, "unknown matchone s=[%c] ch=[%c]\n", s, ch);
                        return -1;
    }
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
        //memcpy(task->stack, from->stack, sizeof(task->stack));
        // I think we have memory alignment issues with the memcpy (which is weird!)
        // Does seem to be the case on the 32bit qemu build!
        for (int i=0; i < TASK_STACK_SIZE; i++) {
            task->stack[i] = from->stack[i];
        }

        memcpy(task->grp, from->grp, sizeof(struct rele_match_t) * ctx->groups);
        task->sp = from->sp;
    } else {
        // Make sure matches are -1 to staret with...
        for (int i=0; i < ctx->groups; i++) {
            task->grp[i].rm_so = task->grp[i].rm_eo = (int32_t)-1;
        }
        task->sp = TASK_STACK_SIZE;
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
// in a block, so we have tasks freeing, successful task freeing and then
// the main block.
void rele_free(struct rectx *ctx) {
    // If we have kept our tasks then they will still be in the free list...
    while (ctx->free_list) { struct task *x = ctx->free_list->next; free(ctx->free_list); ctx->free_list = x; }

    // Free the result task if there is one...
    if (ctx->done) free(ctx->done);
    free(ctx);
}

// Compare the group structures between two tasks to see if they are the same
// We can do this with memcmp which should be optimised by the compiler given
// they are word-wide comparisons.
static inline int has_same_groups(struct rectx *ctx, struct task *a, struct task *b) {
    if (memcmp(a->grp, b->grp, ctx->groups * sizeof(struct rele_match_t)) == 0) return 1;
    return 0;
}

// Compare the stack (including sp) on two tasks to see if they are the same
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
        }
    }
    return 0;
}


static int rele_match_iter(struct rectx *ctx, char *start, char *p, char *end, int flags);

int rele_match(struct rectx *ctx, char *p, int len, int flags) {
    char *start = p;
    char *end = p + (len ? len : strlen(p));
    struct node *n = ctx->fast_start;

        // Used for caseless matching
    int icase = ctx->flags & RELE_CASELESS;

    // Now see if we have a faststart option...
    if (n) {
        if (n->op == OP_MATCH) {
            if (n->ch1) {
                // Ok, normal char ... need to see if icase or not
                if (icase) {
                    for (; p <= end; p++) {
                        char c1 = fast_tolower(*p);
                        char c2 = fast_tolower(n->ch1);     // TODO: not sure we need to do this?
                        if (c1 != c2) continue;
                    }
                    if (rele_match_iter(ctx, start, p, end, flags)) return 1;
                } else {
                    for (; p <= end; p++) {
                        p = memchr(p, n->ch1, end - p);
                        if (!p) return 0;
                        if (rele_match_iter(ctx, start, p, end, flags)) return 1;
                    }
                }
            } else {
                // Special char match, performance nightmare...
                for (; p <= end; p++) {
                    if (!matchone(n->ch2, *p)) continue;
                    if (rele_match_iter(ctx, start, p, end, flags)) return 1;
                }
            }
        } else if (n->op == OP_MATCHSTR) {
            if (icase) {
                for (; p <= end; p++) {
                    p = rele_strifind(p, end - p, n->string, n->len);
                    if (!p) return 0;
                    if (rele_match_iter(ctx, start, p, end, flags)) return 1;
                }                
            } else {
                for (; p <= end; p++) {
                    p = memmem(p, end - p, n->string, n->len);
                    if (!p) return 0;
                    if (rele_match_iter(ctx, start, p, end, flags)) return 1;
                }
            }
        } else if (n->op == OP_MATCHSET) {
            for (; p <= end; p++) {
                if (!match_set(*p, n->set)) continue;
                if (rele_match_iter(ctx, start, p, end, flags)) return 1;
            }
        } else if (n->op == OP_STAR) {
            if (rele_match_iter(ctx, start, p, end, flags)) return 1;
        } else {
            fprintf(stderr, "WEIRD FAST START\n");
        }
    } else {
        for (; p <= end; p++) {
            if (rele_match_iter(ctx, start, p, end, flags)) return 1;
        }
    }

    if (NOT_FLAG(flags, RELE_KEEP_TASKS)) {
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

    // Used to tracking zero length matches
    uint32_t    iter = 0;
    struct task *expected;

    // Used for caseless matching
    int icase = ctx->flags & RELE_CASELESS;

    do {
        // Get ready to run through for this char...
        t = run_list;
        if (!t) goto done;

        char ch;
        if (p < end) {
            ch = (icase ? fast_tolower(*p) : *p);
        } else {
            ch = 0;
        }
        prev = NULL;

        expected = t;

        // Now for each task go through the binary tree until we get to
        // a match type op, then we either die (match failed), or we stay
        // for next time.
        while (t) {
            // This is attempting to increase iter for every task but taking
            // into account child tasks and not incrememnting for them. It basically
            // looks at what's coming next, and then only increments iter when it
            // gets there, so any children should be ignored on the first time around.
            if (t == expected) {
                iter++;
                expected = t->next;
                if (expected == NULL) expected = run_list;
            }

            struct node *n = t->n;

            // Probablt the most likely... although less so with OP_MATCHSTR support
            if (n->op == OP_CONCAT) {
                if (t->last == n->a) goto leg_b;
                if (t->last == n->b) goto parent;
                goto leg_a;
            }

            // Attempt to optimise this a bit...
            if (n->op >= OP_GROUP) goto from_GROUP;

            // Probably the second most likely...
            if (n->op == OP_MATCH) {
                if (!ch) goto die;      // can't match NULL
                if ((n->ch1 && (n->ch1 == ch)) || (!n->ch1 && matchone(n->ch2, ch))) {
                    if (has_prior_match(ctx, run_list, n, t)) goto die;
                    goto match_ok;
                }
                goto die;
            }

            if (n->op == OP_MATCHSTR) {
                if (t->last == n->parent) {
                    // Ok, we need to do the comparison, and then either die or setup
                    // to hang around to the right end point.
                    if (icase) {
                        if (!rele_strncasecmp(n->string, p, n->len)) goto die;
                    } else {
                        if (memcmp(n->string, p, n->len) != 0) goto die;
//                        fprintf(stderr, "Have string match [%.*s]\n", n->len, n->string);
                    }
                    if (has_prior_match(ctx, run_list, n, t)) goto die;
                    // We need to stay here
                    t->last = n;                // horrible overloading
                    t->p = p + n->len - 1;
                    goto next;
                }
                if (p == t->p) {
                    t->n = n->parent;
                    t->last = n;
                }
                goto next;
            }


            // If we get here from above, then go down the b leg. If we get here
            // from b, then it was successful and we spawn. Who goes where depends
            // on if we are lazy or not...
            if (n->op == OP_PLUS) {
                if (t->last == n->parent) {
                    n->iter = iter;
                    goto leg_b;
                }
                if (n->iter == iter) goto parent;       // zero length match
                n->iter = iter;
                goto new_b_or_parent;
            }

            // If we get here from above, we spawn to go back (zero) then we go
            // down b. If we get here from b, then carry on back up.
            if (n->op == OP_QUESTION) {
                if (t->last == n->b) goto parent;
                goto new_b_or_parent;
            }

            // If we hit from above then spawn to go right back up (zero) and from
            // b we do the same.
            // TODO: zero length match support
            if (n->op == OP_STAR) {
                if (t->last == n->parent) {
                    n->iter = iter;
                } else {
                    if (n->iter == iter) goto parent;    // zero length match
                    n->iter = iter;
                }
                goto new_b_or_parent;
            }

            if (n->op == OP_DOTSTAR) {
                // Let's handle the match case first...
                if (n->match && n->match->op == OP_MATCHSTR) {
                    if (t->last == n->parent) {
                        // If we have a match, then we can try to match it.
                        t->p = memmem(p, (size_t)(end - p), n->match->string, n->match->len);
                        if (!t->p) goto die;        // we'll never match the next match
                        // If we did match then we need to wait here for it...
                        goto next;
                    } else {
                        // We are waiting...
                        if (p == t->p) {
                            // We matched, but there might be more, if greedy then we
                            // spawn a child to go up, and we come back here as if from the
                            // parent
                            // TODO: what does lazy mean in this context...
                            t->next = task_new(ctx, t, t->next, n, n->parent);
                            t->last = n->parent;
                            goto next;
                        }
                        goto next;
                    }
                }

                if (t->last == NULL) {
                    // Special lazy case, we spawned to come here as a matcher...
                    if (!ch) goto die;
                    t->last = n;
                    goto next;
                }
                // TODO: lazy version
                // greedy -- Spawn a new child to go back up...
                // and we also go backup but to next task, since we're a match.

                if (n->lazy) {
                    t->next = task_new(ctx, t, t->next, NULL, n);
                    goto parent;
                } else {
                    t->next = task_new(ctx, t, t->next, n, n->parent);
                    if (!ch) goto die;      // can't match NULL
                    t->last = n;
                    goto next;
                }
            }

from_GROUP:
            // If we hit an empty group, then make sure we haven't just hit it,
            // in which case we die otherwise we proceed back up to the parent
            if (n->op == OP_GROUP) {
                if (n->b == NOTUSED) {
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
            }

            // If we get here from above then spin off a new task to go down leg b
            // and we go down leg a. Anything coming back up, goes to the parent.
            if (n->op == OP_ALTERNATE) {
                if (t->last == n->parent) {
                    t->next = task_new(ctx, t, t->next, n, n->b);
                    goto leg_a;
                }
                goto parent;
            }

            // If we get to OP_DONE then we are done, but there might be other
            // tasks to continue. We keep the first task completed at each index,
            // then overwrite for the next one.
            //
            // However, since the tasks are prioritised based on lazyness etc, then
            // if there are no tasks before us, then we are the one!
            //
            if (n->op == OP_DONE) {
                // If we have already completed at this index, then die...
                if (ctx->done && ctx->done->p == p) goto die;

                // Free the previous candidate if there was one...
                if (ctx->done) task_release(ctx, ctx->done);

                // Prep and store as the candidate...
                t->p = p;
                ctx->done = t;

                // If we are the top of the task list we are completetly done
                if (run_list == t) {
                    run_list = t->next;
                    t->next = NULL;
                    goto done;
                }
                // Otherwise we aren't top, so go to next task...
                prev->next = t->next;
                t->next = NULL;
                t = prev->next;
                continue;
            }

            // CHeck for a ghost match on these...
            if (n->op == OP_ANCHOR) {
                switch (n->ch1) {
                    case 'b':       if (p == start) {
                                        if (isalnum((int)*p)) goto parent;
                                    } else if (p == end) {
                                        if (isalnum((int)p[-1])) goto parent;
                                    } else if (isalnum((int)p[-1]) ^ isalnum((int)p[0])) {
                                        goto parent;
                                    }
                                    goto die;
                    case 'B':       if (p == start) {
                                        if (!isalnum((int)*p)) goto parent;
                                    } else if (p == end) {
                                        if (!isalnum((int)p[-1])) goto parent;
                                    } else if (!(isalnum((int)p[-1]) & isalnum((int)p[0]))) {
                                        goto parent;
                                    }
                                    goto die;
                    case 'A':       if (p == start) goto parent;
                                    goto die;
                    case 'Z':       if (p == end) goto parent;
                                    goto die;

                    case '^':       if (p == start) goto parent; 
                                    if (p[-1] == '\n') goto parent;
                                    goto die;
                    case '$':       if (p == end) goto parent;
                                    if (*p == '\n') goto parent;
                                    goto die;
                    default:        goto die;       // shouldn't happen

                }
            }

            // TODO: ch is already lower() if icase, which is a waste
            //       could we do it later/here?
            if (n->op == OP_MATCHSET) {
                if (match_set(ch, n->set)) {
                    if (has_prior_match(ctx, run_list, n, t)) goto die;
                    t->last = n;
                    t->n = n->parent;
                    goto next;
                }
                goto die;
            }

            if (n->op == OP_MATCHGRP) {
                if (t->last == n->parent) {
                    // Ok, we need to do the comparison, and then either die or setup
                    // to hang around to the right end point.
                    int len = t->grp[n->mgrp].rm_eo - t->grp[n->mgrp].rm_so;
                    char *grpstr = start + t->grp[n->mgrp].rm_so;
                    
                    // A zero length group match is a ghost match...
                    if (!len) goto parent;

                    // One char match is simple
                    if (len == 1) {
                        if (icase) {
                            if (fast_tolower(*grpstr) != fast_tolower(*p)) goto die;
                        } else {
                            if (*grpstr != *p) goto die;
                        }
                        if (has_prior_match(ctx, run_list, n, t)) goto die;
                        goto match_ok;
                    }

                    // Otherwise see if match... TODO: this is an issue because the first
                    // group match won't be all in upper if case insensitive!!!
                    if (icase) {
                        if (!rele_strncasecmp(grpstr, p, len)) goto die;
                    } else {
                        if (memcmp(grpstr, p, len) != 0) goto die;
                    }
                    if (has_prior_match(ctx, run_list, n, t)) goto die;
                    // We need to stay here
                    t->last = n;                // horrible overloading
                    t->p = p + len - 1;
                    goto next;
                }
                if (p == t->p) goto match_ok;
                goto next;
            }

            // Match a previous group, coming from the top we initialise then stay
            // here iterating... originally we used a separate variable, but a stack
            // item should be fine.
            /*
                if (n->op == OP_MATCHGRP) {
                // TODO TODO TODO
                //
                // Switch this to the same model used in the string match
                // will simplify and speed up.
                //
                if (t->last == n->parent) {
                    // A zero length group match is a ghost match...
                    if (t->grp[n->mgrp].rm_so == t->grp[n->mgrp].rm_eo) {
                        goto parent;
                    }
                    // We will use a stack entry for tracking position...
                    if (t->sp == 0) {
                        fprintf(stderr, "STACK OVERFLOW, MATCHGRP\n");
                        goto die;
                    }
                    t->sp--;
                    t->stack[t->sp] = t->grp[n->mgrp].rm_so;
                }

                // TODO: why not turn this into a string match and then wait around until
                // the end?  Allows fast fail.

                // We need to support matching in a different case if we have caseless set...
                if (ch == CASEUP(start[t->stack[t->sp]], caseval)) {
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
            }
            */

            // If we come from above then get a new stack position and init, otherwise
            // count until we hit min, then spawn until max...
            //
            // TODO: zero length match support -- any zero length match is considered
            // to have met the requirement.
            // Need to ensure that a zero min doesn't get killed. Check from coming from b.
            if (n->op == OP_MULT) {
                if (t->last == n->parent) {
                    if (t->sp == 0) {
                        // TODO: stack error
                        fprintf(stderr, "stack nesting too deep.\n");
                        goto die;
                    }
                    t->sp--;
                    t->stack[t->sp] = 0;
                    n->iter = iter;
                }
                // If we come from below and have a zero length, then
                // we can consider this all done.
                if (t->last == n->b) {
                    if (n->iter == iter) { t->sp++; goto parent; }
                    n->iter = iter;
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
            }

            // We always match a LF on either go around, but if not and we come from above then we must
            // match a CR and go again. On the second time around if doesn't matter if we don't match.
            if (n->op == OP_CRLF) {
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
                goto die;
            }                    

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

match_ok:           t->n = n->parent;
                    t->last = n;
                    // fall through...

next:               prev = t;
                    t = t->next;
                    continue;

die:                if (prev) {
                        prev->next = t->next; task_release(ctx, t); t = prev->next;
                        continue;
                    } else {
                        run_list = t->next; task_release(ctx, t); t = run_list;
                        continue;
                    }
        }
        p++;
    } while(p <= end);

    // Ok, we get here because we've run out of text or we've run out of tasks
    // or both.

done:
    // Move any tasks left on the run-list into the free list
    while (run_list) { t = run_list->next; task_release(ctx, run_list); run_list = t; }

    // And return status...
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
        case OP_QUESTION:   return "QUESTION";
        case OP_ALTERNATE:  return "ALTERNATE";
        case OP_DONE:       return "DONE";
        case OP_GROUP:      return "GROUP";
        case OP_MATCHSET:   return "MATCHSET";
        case OP_MULT:       return "MULT";
        case OP_MATCHGRP:   return "MATCHGRP";
        case OP_MATCHSTR:   return "MATCHSTR";
        case OP_CRLF:       return "CRLF";
        case OP_ANCHOR:     return "ANCHOR";
        case OP_DOTSTAR:    return "DOTSTAR";
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

#define OUTC(c)     if(isprint((int)c)) { fprintf(f, "'%c'", c); } else { fprintf(f, "[0x%02x]", c); }

    fprintf(stderr, "NODEID: %d.  OP=%d\n", NODE_ID(ctx, n), n->op);

    switch(n->op) {
        case OP_MATCH:
            if (n->ch1 && n->ch2) {
                OUTC(n->ch1);
                fprintf(f, " | ");
                OUTC(n->ch2);
            } else if (n->ch1) {
                OUTC(n->ch1);
            } else if (n->ch2) {
                fprintf(f, "SPECIAL ");
                OUTC(n->ch2);
            } else {
                fprintf(f, "????");
            }
            GEND;
            return;

        case OP_MATCHSTR:
            fprintf(f, "'%.*s'", n->len, n->string);
            GEND;
            return;

        case OP_ANCHOR:
            fprintf(f, "'%c'", n->ch1);
            GEND;
            return;

        case OP_DOTSTAR:
            if (n->match) {
                fprintf(f, "[SRCH NODE %d]", NODE_ID(ctx, n->match));
            } else {
                fprintf(f, "none");
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

void rele_export_tree(struct rectx *ctx, const char *filename) {
    FILE *f = fopen(filename, "w");
    fprintf(f, "digraph tree {\n");
    fprintf(stderr, "x\n");
    dump_dot(ctx, ctx->root, f);
    fprintf(f, "}\n");
    fclose(f);
}


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned char *stack_low = NULL;
static unsigned char *stack_high = NULL;

/* Find stack mapping from /proc/self/maps */
void stack_init_bounds(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) { perror("fopen"); exit(1); }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "[stack]")) {
            unsigned long start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                stack_low  = (unsigned char *)start;
                stack_high = (unsigned char *)end;
                break;
            }
        }
    }
    fclose(f);

    if (!stack_low || !stack_high) {
        fprintf(stderr, "Could not find stack mapping\n");
        exit(1);
    }
}

/* Return current SP (x86_64 / gcc/clang inline asm) */

static inline void *get_sp(void) {
#if defined(__x86_64__) || defined(_M_X64)
    void *sp;
    asm volatile ("mov %%rsp, %0" : "=r"(sp));
    return sp;
#elif defined(__aarch64__)
    void *sp;
    asm volatile ("mov %0, sp" : "=r"(sp));
    return sp;
#elif defined(__arm__)
    void *sp;
    asm volatile ("mov %0, sp" : "=r"(sp));
    return sp;
#else
#   error "Unsupported architecture"
#endif
}

/* Fill from stack_low up to current SP with 0xAA */
void stack_fill(void) {
    unsigned char *sp = get_sp();
    size_t len = (size_t)(sp - stack_low);
    memset(stack_low, 0xAA, len);
}

/* Measure how much of the filled area has been touched */
size_t stack_usage(void) {
    unsigned char *sp = get_sp();
    unsigned char *p  = stack_low;
    while (p < sp && *p == 0xAA)
        p++;
    return (size_t)(sp - p);
}

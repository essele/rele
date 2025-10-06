#ifndef __PMU_H
#define __PMU_H

#include <stdint.h>

static inline void enable_pmu(void) {
    uint32_t value = 0;

    // 1. Enable user-mode access to performance counters
    value = 1;
    asm volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(value));

    // 2. Enable all counters including the cycle counter (PMCR E bit = 1)
    asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(value));  // Read PMCR
    value |= 1;                                                // Set E (enable all counters)
    value |= (1 << 2);                                         // Reset cycle counter
    value |= (1 << 1);                                         // Reset event counters
    // Set bit 3 (D) to 1 to divide by 64
    value |= (1 << 3);
    asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(value));  // Write PMCR

    // 3. Enable cycle counter specifically (bit 31 of PMCNTENSET)
    value = (1 << 31);
    asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(value));

    // 4. Optionally clear overflow flags
    value = 0x8000000f;
    asm volatile("mcr p15, 0, %0, c9, c12, 3" :: "r"(value));
}

// Read the 32-bit PMCCNTR
static inline uint32_t read_cycle_counter(void) {
    uint32_t val;
    asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
    return val;
}


#endif
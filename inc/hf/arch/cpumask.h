#pragma once

#include <hf/bitops.h>

typedef struct cpumask {
    DECLARE_BITMAP(bits, MAX_CPUS);
} cpumask_t;

static inline uint32_t cpumask_check(uint32_t cpu)
{
    return cpu;
}

static inline void cpumask_set_cpu(int cpu, cpumask_t *dstp)
{
    set_bit(cpumask_check(cpu), dstp->bits);
}

static inline void cpumask_clear_cpu(int cpu, cpumask_t *dstp)
{
    clear_bit(cpumask_check(cpu), dstp->bits);
}

static inline void cpumask_clearall(cpumask_t *dstp)
{
    bitmap_zero(dstp->bits, MAX_CPUS);
}

static inline void cpumask_setall(cpumask_t *dstp)
{
    bitmap_fill(dstp->bits, MAX_CPUS);
}

static inline uint32_t cpumask_first(const cpumask_t *dstp)
{
    return ffs(dstp->bits);
}

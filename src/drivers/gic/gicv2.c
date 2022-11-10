/*
 * xen/arch/arm/gic-v2.c
 *
 * Tim Deegan <tim@xen.org>
 * Copyright (c) 2011 Citrix Systems.
 *
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * LR register definitions are GIC v2 specific.
 * Moved these definitions from header file to here
 */
#include <hf/arch/arch.h>
#include <hf/io.h>
#include <hf/static_assert.h>
#include <hf/std.h>
#include <hf/errno.h>
#include <hf/dlog.h>
#include <hf/mm.h>
#include <hf/spinlock.h>
#include "gicv2.h"

#define GICH_V2_LR_VIRTUAL_MASK    0x3ff
#define GICH_V2_LR_VIRTUAL_SHIFT   0
#define GICH_V2_LR_PHYSICAL_MASK   0x3ff
#define GICH_V2_LR_PHYSICAL_SHIFT  10
#define GICH_V2_LR_STATE_MASK      0x3
#define GICH_V2_LR_STATE_SHIFT     28
#define GICH_V2_LR_PENDING         (1U << 28)
#define GICH_V2_LR_ACTIVE          (1U << 29)
#define GICH_V2_LR_PRIORITY_SHIFT  23
#define GICH_V2_LR_PRIORITY_MASK   0x1f
#define GICH_V2_LR_HW_SHIFT        31
#define GICH_V2_LR_HW_MASK         0x1
#define GICH_V2_LR_GRP_SHIFT       30
#define GICH_V2_LR_GRP_MASK        0x1
#define GICH_V2_LR_MAINTENANCE_IRQ (1U << 19)
#define GICH_V2_LR_GRP1            (1U << 30)
#define GICH_V2_LR_HW              (1U << GICH_V2_LR_HW_SHIFT)
#define GICH_V2_LR_CPUID_SHIFT     10
#define GICH_V2_LR_CPUID_MASK      0x7
#define GICH_V2_VTR_NRLRGS         0x3f

#define GICH_V2_VMCR_PRIORITY_MASK   0x1f
#define GICH_V2_VMCR_PRIORITY_SHIFT  27

#define GIC_PRI_LOWEST     0xf0
#define GIC_PRI_IRQ        0xa0
#define GIC_PRI_IPI        0x90 /* IPIs must preempt normal interrupts */
#define GIC_PRI_HIGHEST    0x80 /* Higher priorities belong to Secure-World */

#define min(a, b)          ((a) < (b) ? (a) : (b))

static int gicv2_nr_lines;

struct gicv2_device {
    uintpaddr_t gicd_base;
    struct spinlock lock;
    uint32_t nr_lines;
};

static struct gicv2_device g_gicv2_device = {0};

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *des, const char *src);

extern int vgicv2_init(uint64_t *data, int len);

/* Maximum cpu interface per GIC */
#define NR_GIC_CPU_IF 8

static struct gicv2_device* gicv2_get_device(void)
{
    return &g_gicv2_device;
}

static void gicv2_device_init(void)
{
    struct gicv2_device *device = gicv2_get_device();

    memset(device, 0, sizeof(struct gicv2_device));
    sl_init(&device->lock);
    device->gicd_base = GICD_BASE;

    /* TODO: calculate nr_lines */
    device->nr_lines = 64;
}

static inline void writeb_gicd(uint8_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writeb_relaxed(val, device->gicd_base + offset);
}

static inline void writel_gicd(uint32_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writel_relaxed(val, device->gicd_base + offset);
}

static inline uint32_t readl_gicd(unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();
	return readl_relaxed(device->gicd_base + offset);
}

void gicv2_clear_pending(uint32_t irq)
{
	writel_gicd(1UL << (irq % 32), GICD_ICPENDR + (irq / 32) * 4);
	dsb();
}

int gicv2_set_irq_priority(uint32_t irq, uint32_t pr)
{
    struct gicv2_device *device = gicv2_get_device();

	sl_lock(&device->lock);

	/* Set priority */
	writeb_gicd(pr, GICD_IPRIORITYR + irq);

	sl_unlock(&device->lock);

	return 0;
}

int gicv2_set_irq_affinity(uint32_t irq, uint32_t pcpu)
{
    struct gicv2_device *device = gicv2_get_device();

	if (pcpu > NR_GIC_CPU_IF || irq < 32)
		return -EINVAL;

	sl_lock(&device->lock);
	writeb_gicd(1 << pcpu, GICD_ITARGETSR + irq);
	sl_unlock(&device->lock);

	return 0;
}


void gicv2_mask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

	sl_lock(&device->lock);
	writel_gicd(1UL << (irq % 32), GICD_ICENABLER + (irq / 32) * 4);
	dsb();
	sl_unlock(&device->lock);
}

void gicv2_unmask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

	sl_lock(&device->lock);
	writel_gicd(1UL << (irq % 32), GICD_ISENABLER + (irq / 32) * 4);
	dsb();
	sl_unlock(&device->lock);
}

void gicv2_mask_irq_cpu(uint32_t irq, int cpu)
{
	dlog_info("not support mask irq_percpu\n");
}

void gicv2_unmask_irq_cpu(uint32_t irq, int cpu)
{
	dlog_info("not support unmask irq_percpu\n");
}

static void gicv2_cpu_init(void)
{
}

static void gicv2_dist_init(void)
{
	uint32_t type;
	uint32_t cpumask;
	uint32_t gic_cpus;
	unsigned int nr_lines;
	int i;

	cpumask = readl_gicd(GICD_ITARGETSR) & 0xff;
	cpumask = (cpumask == 0) ? (1 << 0) : cpumask;
	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	/* Disable the distributor */
	writel_gicd(0, GICD_CTLR);

	type = readl_gicd(GICD_TYPER);
	nr_lines = 32 * ((type & GICD_TYPE_LINES) + 1);
	gic_cpus = 1 + ((type & GICD_TYPE_CPUS) >> 5);
	dlog_info("GICv2: %d lines, %d cpu%s%s (IID %x).\n",
		nr_lines, gic_cpus, (gic_cpus == 1) ? "" : "s",
		(type & GICD_TYPE_SEC) ? ", secure" : "",
		readl_gicd(GICD_IIDR));


	/* Default all global IRQs to level, active low */
	for ( i = 32; i < nr_lines; i += 16 )
		writel_gicd(0x0, GICD_ICFGR + (i / 16) * 4);

	/* Route all global IRQs to this CPU */
	for ( i = 32; i < nr_lines; i += 4 )
		writel_gicd(cpumask, GICD_ITARGETSR + (i / 4) * 4);

	/* Default priority for global interrupts */
	for ( i = 32; i < nr_lines; i += 4 )
		writel_gicd(GIC_PRI_IRQ << 24 | GIC_PRI_IRQ << 16 |
			GIC_PRI_IRQ << 8 | GIC_PRI_IRQ,
			GICD_IPRIORITYR + (i / 4) * 4);

	/* Disable all global interrupts */
	for ( i = 32; i < nr_lines; i += 32 )
		writel_gicd(~0x0, GICD_ICENABLER + (i / 32) * 4);

	/* Only 1020 interrupts are supported */
	gicv2_nr_lines = min(1020U, nr_lines);

	/* Turn on the distributor */
	writel_gicd(GICD_CTL_ENABLE, GICD_CTLR);
	dsb();
}

int gicv2_init(void)
{
    struct gicv2_device *device = gicv2_get_device();

	dlog_info("*** gicv2 init ***\n");

    gicv2_device_init();

	sl_lock(&device->lock);

	gicv2_dist_init();
	gicv2_cpu_init();

	sl_unlock(&device->lock);

	return 0;
}

int gicv2_secondary_init(void)
{
    struct gicv2_device *device = gicv2_get_device();

	sl_lock(&device->lock);

	gicv2_cpu_init();

	sl_unlock(&device->lock);

	return 0;
}

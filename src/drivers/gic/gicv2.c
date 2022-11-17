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
#include <hf/bitops.h>
#include <hf/interrupt.h>
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

struct gicv2_device {
    uintpaddr_t gicd_base;
    uint32_t frame_size;
    struct spinlock lock;

    uint32_t nr_lines;
    uint32_t cpus;
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
    device->frame_size = 0x10000;
}

void writeb_gicd(uint8_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writeb_relaxed(val, device->gicd_base + offset);
}

void writew_gicd(uint16_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writew_relaxed(val, device->gicd_base + offset);
}

void writel_gicd(uint32_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writel_relaxed(val, device->gicd_base + offset);
}

void writeq_gicd(uint64_t val, unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();

	writeq_relaxed(val, device->gicd_base + offset);
}

uint8_t readb_gicd(unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();
	return readb_relaxed(device->gicd_base + offset);
}

uint16_t readw_gicd(unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();
	return readw_relaxed(device->gicd_base + offset);
}

uint32_t readl_gicd(unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();
	return readl_relaxed(device->gicd_base + offset);
}

uint64_t readq_gicd(unsigned int offset)
{
    struct gicv2_device *device = gicv2_get_device();
	return readq_relaxed(device->gicd_base + offset);
}

/* interface for virtual device */

void gicv2_clear_pending(uint32_t irq)
{
	writel_gicd(1UL << (irq % 32), GICD_ICPENDR + (irq / 32) * 4);
	//dsb();
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
	//dsb();
	sl_unlock(&device->lock);
}

void gicv2_unmask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

	sl_lock(&device->lock);
	writel_gicd(1UL << (irq % 32), GICD_ISENABLER + (irq / 32) * 4);
	//dsb();
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

/* interfaces for vdev */
uint32_t gicv2_vdev_get_ise(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ISENABLER + (irq / 32) * 4);
    sl_unlock(&device->lock);

    for (int i = 0; i < 32; i++) {
        if (!hwirq_is_belongs_current(irq + i)) {
            value &= ~(1 << i);
        }
    }

    return value;
}

uint32_t gicv2_vdev_get_ice(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ICENABLER + (irq / 32) * 4);
    sl_unlock(&device->lock);

    for (int i = 0; i < 32; i++) {
        if (!hwirq_is_belongs_current(irq + i)) {
            value &= ~(1 << i);
        }
    }

    return value;
}

uint32_t gicv2_vdev_get_isp(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ISPENDR + (irq / 32) * 4);
    sl_unlock(&device->lock);

    for (int i = 0; i < 32; i++) {
        if (!hwirq_is_belongs_current(irq + i)) {
            value &= ~(1 << i);
        }
    }

    return value;
}

#define DEFINE_IRQ_DEAL1_FUNC(name)  \
    uint32_t gicv2_vdev_get_##name(uint32_t irq)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t value; \
        \
        irq = align_down(irq, 32);  \
        sl_lock(&device->lock); \
        value = readl_gicd(GICD_##name + (irq / 32) * 4); \
        sl_unlock(&device->lock); \
        \
        for (int i = 0; i < 32; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                value &= ~(1 << i); \
            }   \
        }   \
        \
        return value;   \
    }   \
    \
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value;    \
        \
        irq = align_down(irq, 32);  \
        for (int i = 0; i < 32; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(1 << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        writel_gicd(real_value, GICD_##name + (irq / 32) * 4); \
        sl_unlock(&device->lock);   \
    }

#define DEFINE_IRQ_DEAL2_FUNC(name)  \
    uint32_t gicv2_vdev_get_##name(uint32_t irq)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t value; \
        \
        irq = align_down(irq, 16);  \
        sl_lock(&device->lock); \
        value = readl_gicd(GICD_##name + (irq / 16) * 4); \
        sl_unlock(&device->lock); \
        \
        for (int i = 0; i < 16; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                value &= ~(0x3 << i); \
            }   \
        }   \
        \
        return value;   \
    }   \
    \
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value;    \
        \
        irq = align_down(irq, 16);  \
        for (int i = 0; i < 16; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(0x3 << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        writel_gicd(real_value, GICD_##name + (irq / 16) * 4); \
        sl_unlock(&device->lock);   \
    }

#define DEFINE_IRQ_DEAL4_FUNC(name)  \
    uint32_t gicv2_vdev_get_##name(uint32_t irq)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t value; \
        \
        irq = align_down(irq, 4);  \
        sl_lock(&device->lock); \
        value = readl_gicd(GICD_##name + (irq / 4) * 4); \
        sl_unlock(&device->lock); \
        \
        for (int i = 0; i < 4; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                value &= ~(0xff << i); \
            }   \
        }   \
        \
        return value;   \
    }   \
    \
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value;    \
        \
        irq = align_down(irq, 4);  \
        for (int i = 0; i < 4; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(0xff << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        writel_gicd(real_value, GICD_##name + (irq / 4) * 4); \
        sl_unlock(&device->lock);   \
    }

DEFINE_IRQ_DEAL1_FUNC(IGROUPR)
DEFINE_IRQ_DEAL1_FUNC(ISENABLER)
DEFINE_IRQ_DEAL1_FUNC(ICENABLER)
DEFINE_IRQ_DEAL1_FUNC(ISPENDR)
DEFINE_IRQ_DEAL1_FUNC(ICPENDR)
DEFINE_IRQ_DEAL1_FUNC(ISACTIVER)
DEFINE_IRQ_DEAL1_FUNC(ICACTIVER)
DEFINE_IRQ_DEAL4_FUNC(IPRIORITYR)
DEFINE_IRQ_DEAL4_FUNC(ITARGETSR)
DEFINE_IRQ_DEAL4_FUNC(ITARGETSRR)
DEFINE_IRQ_DEAL2_FUNC(ICFGR)
DEFINE_IRQ_DEAL2_FUNC(NSACR)

uint32_t gicv2_get_ctrl(void)
{
	return readl_gicd(GICD_CTLR);
}

uint32_t gicv2_get_typer(void)
{
	return readl_gicd(GICD_TYPER);
}

uint32_t gicv2_get_iidr(void)
{
	return readl_gicd(GICD_IIDR);
}

static void gicv2_dist_init(void)
{
	uint32_t type;
	uint32_t cpumask;
	uint32_t gic_cpus;
	uint32_t nr_lines;
    uint32_t default_priority;
    struct gicv2_device *device = gicv2_get_device();
	int i;

	cpumask = readl_gicd(GICD_ITARGETSR) & 0xff;
	cpumask = (cpumask == 0) ? (1 << 0) : cpumask;
	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	writel_gicd(0, GICD_CTLR);

	type = readl_gicd(GICD_TYPER);
	nr_lines = 32 * ((type & GICD_TYPE_LINES) + 1);
	gic_cpus = ((type & GICD_TYPE_CPUS) >> 5) + 1;

	/* Default all global IRQs to level, active low */
	for ( i = 32; i < nr_lines; i += 16 )
		writel_gicd(0x0, GICD_ICFGR + (i / 16) * 4);

	for ( i = 32; i < nr_lines; i += 4 )
		writel_gicd(cpumask, GICD_ITARGETSR + (i / 4) * 4);

	/* Default priority for global interrupts */
    default_priority = \
            ((GIC_PRI_IRQ << 24) | (GIC_PRI_IRQ << 16) | \
            (GIC_PRI_IRQ << 8) | GIC_PRI_IRQ);
	for ( i = 32; i < nr_lines; i += 4 )
		writel_gicd(default_priority, GICD_IPRIORITYR + (i / 4) * 4);

	/* Disable all global interrupts */
	for ( i = 32; i < nr_lines; i += 32 )
		writel_gicd(0xffffffff, GICD_ICENABLER + (i / 32) * 4);

	/* Only 1020 interrupts are supported */
	device->nr_lines = min(1020U, nr_lines);

	/* Turn on the distributor */
	writel_gicd(GICD_CTL_ENABLE, GICD_CTLR);
}

void gicv2_mm_init(
    struct mm_stage1_locked stage1_locked, struct mpool *ppool)
{
    struct gicv2_device *device = NULL;

    gicv2_device_init();
    device = gicv2_get_device();

	/* Map deivce space for interrupt controller */
	mm_identity_map(stage1_locked, pa_init(device->gicd_base),
			pa_add(pa_init(device->gicd_base), device->frame_size),
			MM_MODE_R | MM_MODE_W | MM_MODE_D, ppool);
}

int gicv2_init(void)
{
    struct gicv2_device *device = gicv2_get_device();

	dlog_info("*** gicv2 init ***\n");

	sl_lock(&device->lock);

	gicv2_dist_init();

	sl_unlock(&device->lock);

    spi_map_init();

	return 0;
}

int gicv2_secondary_init(void)
{
	return 0;
}

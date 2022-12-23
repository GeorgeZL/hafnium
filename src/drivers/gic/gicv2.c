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
    device->frame_size = 0x1000;
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

int gicv2_set_irq_affinity(uint32_t irq, uint32_t affinity)
{
    struct gicv2_device *device = gicv2_get_device();

	if (irq < 32)
		return -EINVAL;

	sl_lock(&device->lock);
	writeb_gicd(affinity, GICD_ITARGETSR + irq);
	sl_unlock(&device->lock);

	return 0;
}

uint8_t gicv2_get_irq_affinity(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint8_t value = 0;

    sl_lock(&device->lock);
    value = readb_gicd(GICD_ITARGETSR + irq);
    sl_unlock(&device->lock);

    return value;
}

void gicv2_mask_irq_cpu(uint32_t irq, int cpu)
{
	dlog_info("not support mask irq_percpu\n");
}

void gicv2_unmask_irq_cpu(uint32_t irq, int cpu)
{
	dlog_info("not support unmask irq_percpu\n");
}

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

void gicv2_set_irq_type(uint32_t irq, uint32_t type)
{
    struct gicv2_device* device = gicv2_get_device();
    uint32_t cfg, edgebit;

    if (irq < 16)
        return ;

    sl_lock(&device->lock);

    cfg = readl_gicd(GICD_ICFGR + (irq / 16) * 4);
    edgebit = 2u << (2 * (irq % 16));
    if (type & IRQ_FLAGS_LEVEL_BOTH) {
        cfg &= ~edgebit;
    } else if (type & IRQ_FLAGS_EDGE_BOTH) {
        cfg |= edgebit;
    }
    writel_gicd(cfg, GICD_ICFGR + (irq / 16) * 4);

    sl_unlock(&device->lock);
}

uint32_t gicv2_get_irq_type(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();
    uint32_t cfg, value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ICFGR + (irq / 16) * 4);
    cfg = (value >> ((irq % 16) * 2)) & 0x3;
    sl_unlock(&device->lock);

    return cfg;
}

void gicv2_clear_irq_pending(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd(1UL << (irq % 32), GICD_ICPENDR + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint32_t gicv2_get_clear_pending_state(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();
    uint32_t pending, value;

    sl_lock(&device->lock);
    pending = readl_gicd(GICD_ICPENDR + (irq / 32) * 4);
    value = !!(pending & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

uint32_t gicv2_get_irq_pending_state(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();
    uint32_t pending, value;

    sl_lock(&device->lock);
    pending = readl_gicd(GICD_ISPENDR + (irq / 32) * 4);
    value = !!(pending & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

void gicv2_set_irq_pending(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();
    
    sl_lock(&device->lock);
    writel_gicd(1UL << (irq % 32), GICD_ISPENDR + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint8_t gicv2_get_irq_priority(uint32_t irq)
{
    struct gicv2_device* device = gicv2_get_device();
    uint8_t value;

    sl_lock(&device->lock);

    value = readb_gicd(GICD_IPRIORITYR + irq);

    sl_unlock(&device->lock);

    return value;
}

void gicv2_set_irq_priority(uint32_t irq, uint32_t priority)
{
    struct gicv2_device* device = gicv2_get_device();

    sl_lock(&device->lock);

    writeb_gicd(priority, GICD_IPRIORITYR + irq);

    sl_unlock(&device->lock);
}

void gicv2_send_sgi(uint32_t sgi, uint32_t mode, uint32_t *mask)
{

}

void gicv2_sgi_set_conf(uint32_t conf)
{
    struct gicv2_device* device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd(conf, GICD_SGIR);
    sl_unlock(&device->lock);
}

void gicv2_sgi_clear(uint32_t sgi, uint8_t mask)
{
    struct gicv2_device* device = gicv2_get_device();

    sl_lock(&device->lock);
    writeb_gicd(mask, GICD_CPENDSGIR + sgi);
    sl_unlock(&device->lock);
}

uint8_t gicv2_sgi_pending_state(uint32_t sgi)
{
    struct gicv2_device* device = gicv2_get_device();
    uint8_t pending;

    sl_lock(&device->lock);
    pending = readb_gicd(GICD_CPENDSGIR + sgi);
    sl_unlock(&device->lock);

    return pending;
}

void gicv2_sgi_active(uint32_t sgi, uint8_t mask)
{
    struct gicv2_device* device = gicv2_get_device();

    sl_lock(&device->lock);
    writeb_gicd(mask, GICD_SPENDSGIR + sgi);
    sl_unlock(&device->lock);
}

uint8_t gicv2_sgi_get_active_state(uint32_t sgi)
{
    struct gicv2_device* device = gicv2_get_device();
    uint8_t active;

    sl_lock(&device->lock);
    active = readb_gicd(GICD_SPENDSGIR + sgi);
    sl_unlock(&device->lock);

    return active;
}

void gicv2_mask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd((1UL << (irq % 32)), GICD_ICENABLER + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint32_t gicv2_get_mask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ICENABLER + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

uint32_t gicv2_get_unmask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ISENABLER + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

void gicv2_unmask_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd((1UL << (irq % 32)), GICD_ISENABLER + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint32_t gicv2_get_irq_state(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ISENABLER + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

uint32_t gicv2_irq_is_active(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ISACTIVER + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

void gicv2_active_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd((1UL << (irq % 32)), GICD_ISACTIVER + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint32_t gicv2_irq_is_deactive(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_ICACTIVER + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

void gicv2_deactive_irq(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd((1UL << (irq % 32)), GICD_ICACTIVER + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

/* default is group0, this helper is to set irq to group1 */
void gicv2_set_irq_group(uint32_t irq, uint32_t group)
{
    struct gicv2_device *device = gicv2_get_device();

    sl_lock(&device->lock);
    writel_gicd((1UL << (irq % 32)), GICD_IGROUPR + (irq / 32) * 4);
    sl_unlock(&device->lock);
}

uint32_t gicv2_get_irq_group(uint32_t irq)
{
    struct gicv2_device *device = gicv2_get_device();
    uint32_t value;

    sl_lock(&device->lock);
    value = readl_gicd(GICD_IGROUPR + (irq / 32) * 4);
    value = !!(value & (1 << (irq % 32)));
    sl_unlock(&device->lock);

    return value;
}

void dump_registers(void)
{
    uint32_t reg = 0;

    dlog_info("DUMP GIC Registers...\n");
    while (reg < 0xF30) {
        dlog_info("REG-%3x: %08x  %08x  %08x  %08x\n", reg,
            readl_gicd(reg), readl_gicd(reg + 4), readl_gicd(reg+8), readl_gicd(reg+0xc));
        reg += 0x10;
    }
    dlog_info("\n");
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
    uint32_t gicv2_vdev_get_##name(uint32_t irq, uint32_t offset, uint32_t size)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t hw_value, value, offset1; \
        \
        irq = align_down(irq, 32);  \
        offset1 = GICD_##name + (irq / 32) * 4; \
        if (offset1 != offset) {    \
            dlog_info("' Read' DIFF: (0x%03x : 0x%03x(size: %#x))\n", offset1, offset, size);   \
        }   \
        sl_lock(&device->lock); \
        hw_value = value = readl_gicd(GICD_##name + (irq / 32) * 4); \
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
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value, uint32_t offset, uint32_t size) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value, offset1;    \
        \
        irq = align_down(irq, 32);  \
        for (int i = 0; i < 32; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(1 << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        if (value != real_value) { \
            dlog_info("'Write' 0x%03x: DIFF(0x%08x : 0x%08x)\n", offset, real_value, value); \
            gicv2_set_direct(offset, value, 4); \
        } else {    \
            offset1 = GICD_##name + (irq / 32) * 4; \
            if (offset1 != offset) {    \
                dlog_info("Offset(%s) is DIFF (0x%08x : 0x%08x(size: %#x))\n", #name, offset1, offset, size);   \
            }   \
            writel_gicd(real_value, GICD_##name + (irq / 32) * 4); \
        } \
        sl_unlock(&device->lock);   \
    }

#define DEFINE_IRQ_DEAL2_FUNC(name)  \
    uint32_t gicv2_vdev_get_##name(uint32_t irq, uint32_t offset, uint32_t size)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t value, hw_value, offset1; \
        \
        irq = align_down(irq, 16);  \
        sl_lock(&device->lock); \
        offset1 = GICD_##name + (irq / 16) * 4; \
        if (offset1 != offset) {    \
            dlog_info("' Read' DIFF: (0x%03x : 0x%03x(size: %#x))\n", offset1, offset, size);   \
        }   \
        hw_value = value = readl_gicd(GICD_##name + (irq / 16) * 4); \
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
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value, uint32_t offset, uint32_t size) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value, offset1;    \
        \
        irq = align_down(irq, 16);  \
        for (int i = 0; i < 16; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(0x3 << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        if (value != real_value) { \
            dlog_info("'Write' 0x%03x: DIFF(0x%08x : 0x%08x)\n", offset, real_value, value); \
            gicv2_set_direct(offset, value, 4); \
        } else {    \
            offset1 = GICD_##name + (irq / 16) * 4; \
            if (offset1 != offset) {    \
                dlog_info("Offset(%s) is DIFF (0x%08x : 0x%08x(size: %#x))\n", #name, offset1, offset, size);   \
            }   \
            writel_gicd(real_value, GICD_##name + (irq / 16) * 4); \
        } \
        sl_unlock(&device->lock);   \
    }

#define DEFINE_IRQ_DEAL4_FUNC(name)  \
    uint32_t gicv2_vdev_get_##name(uint32_t irq, uint32_t offset, uint32_t size)   \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t value, hw_value, offset1; \
        \
        irq = align_down(irq, 4);  \
        sl_lock(&device->lock); \
        offset1 = GICD_##name + (irq / 4) * 4; \
        if (offset1 != offset) {    \
            dlog_info("' Read' DIFF: (0x%03x : 0x%03x(size: %#x))\n", offset1, offset, size);   \
        }   \
        hw_value = value = readl_gicd(GICD_##name + (irq / 4) * 4); \
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
    void gicv2_vdev_set_##name(uint32_t irq, uint32_t value, uint32_t offset, uint32_t size) \
    {   \
        struct gicv2_device *device = gicv2_get_device();   \
        uint32_t real_value = value, offset1;    \
        \
        irq = align_down(irq, 4);  \
        for (int i = 0; i < 4; i++) {  \
            if (!hwirq_is_belongs_current(irq + i)) {   \
                real_value &= ~(0xff << i);    \
            }   \
        }   \
        \
        sl_lock(&device->lock); \
        if (value != real_value) { \
            dlog_info("'Write' 0x%03x: DIFF(0x%08x : 0x%08x)\n", offset, real_value, value); \
            gicv2_set_direct(offset, value, 4); \
        } else {    \
            offset1 = GICD_##name + (irq / 4) * 4; \
            if (offset1 != offset) {    \
                dlog_info("Offset(%s) is DIFF (0x%08x : 0x%08x(size: %#x))\n", #name, offset1, offset, size);   \
            }   \
            writel_gicd(real_value, GICD_##name + (irq / 4) * 4); \
        } \
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
DEFINE_IRQ_DEAL2_FUNC(ICFGR)
DEFINE_IRQ_DEAL2_FUNC(NSACR)

uint32_t gicv2_get_direct(uint32_t reg, uint32_t size)
{
    uint32_t value = 0;

    switch (size) {
        case 1:
            value = readb_gicd(reg);
            break;

        case 2:
            value = readw_gicd(reg);
            break;

        case 4:
            value = readl_gicd(reg);
            break;

        case 8:
            value = readq_gicd(reg);
            break;

        default:
            break;
    }

    return value;
}

void gicv2_set_direct(uint32_t reg, uint32_t value, uint32_t size)
{
    switch (size) {
        case 1:
            writeb_gicd(value, reg);
            break;

        case 2:
            writew_gicd(value, reg);
            break;

        case 4:
            writel_gicd(value, reg);
            break;

        case 8:
            writeq_gicd(value, reg);
            break;

        default:
            break;
    }
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

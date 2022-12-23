#include <hf/arch/arch.h>
#include <hf/io.h>
#include <hf/static_assert.h>
#include <hf/std.h>
#include <hf/errno.h>
#include <hf/dlog.h>
#include <hf/spinlock.h>
#include "gicv3.h"

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *des, const char *src);

static struct gicv3_device g_device = {0};

extern int vgicv3_init(uint64_t *data, int len);

uint64_t cpus_affinity[MAX_CPUS];

#define DEFINE_GIC_DOMAIN_RW_FUNC(name) \
void name##_write32(uint32_t reg, uint32_t value) \
{   \
    iowrite32(value, g_device.name##_base + reg); \
}   \
\
uint32_t name##_read32(uint32_t reg)  \
{   \
    return ioread32(g_device.name##_base + reg);  \
}   \
\
void name##_write64(uint32_t reg, uint64_t value) \
{   \
    iowrite64(value, g_device.name##_base + reg); \
}   \
\
uint64_t name##_read64(uint32_t reg)    \
{   \
    return ioread64(g_device.name##_base + reg);  \
}

DEFINE_GIC_DOMAIN_RW_FUNC(gicd)
DEFINE_GIC_DOMAIN_RW_FUNC(gicr_rd)
DEFINE_GIC_DOMAIN_RW_FUNC(gicr_sgi)

int smp_processor_id()
{
	return 0;
}

struct gicv3_device *get_gicv3_device(void)
{
	return &g_device;
}

static void gicv3_gicd_wait_for_rwp(void)
{
	while (gicd_read32(GICD_CTLR) & (1 << 31));
}

static void gicv3_gicr_wait_for_rwp(void)
{
	while (gicd_read32(GICD_CTLR) & (1 << 31));
}

static inline uint64_t read_icc_sre(void)
{
	return 0x0;
}

static int gicv3_gicc_init(void)
{
	unsigned char aff0, aff1, aff2, aff3;
	uint64_t reg_value;
	uint64_t mpidr = 0x0;             //= read_mpidr_el1();
	int cpu = smp_processor_id();

	aff0 = mpidr & 0xff;
	aff1 = (mpidr & 0xff00) >> 8;
	aff2 = (mpidr & 0xff0000) >> 16;
	aff3 = (mpidr & 0xff00000000) >> 32;

	if (aff0 > 16)
		panic("mpidr 0x%x for cpu%d is wrong\n", mpidr, cpu);

	/*
	 * MPDIR SHIFT means each cluster has one core
	 */
	cpus_affinity[cpu] = (1 << aff0) | (aff1 << 16) |
		((uint64_t)aff2 << 32) | ((uint64_t)aff3 << 48);

	/* enable sre */
	reg_value = read_icc_sre();
	reg_value |= (1 << 0);
	//write_icc_sre(reg_value);

#if 0
	write_sysreg32(0, ICC_BPR1_EL1);
	write_sysreg32(0xff, ICC_PMR_EL1);
	write_sysreg32(1 << 1, ICC_CTLR_EL1);
	write_sysreg32(1, ICC_IGRPEN1_EL1);
#endif
	isb();

	return 0;
}

static int  gicv3_hyp_init(void)
{
	return 0;
}

static int  gicv3_gicr_init(void)
{
	int i;
	uint64_t pr;

	//gicv3_wakeup_gicr();

	/* set the priority on PPI and SGI */
	pr = (0x90 << 24) | (0x90 << 16) | (0x90 << 8) | 0x90;
	for (i = 0; i < GICV3_NR_SGI; i += 4) {
		gicr_sgi_write32(GICR_IPRIORITYR0 + (i / 4) * 4, pr);
	}

	pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
	for (i = GICV3_NR_SGI; i < GICV3_NR_LOCAL_IRQS; i += 4) {
		gicr_sgi_write32(GICR_IPRIORITYR0 + (i / 4) * 4, pr);
	}

	/* disable all PPI and enable all SGI */
	gicr_sgi_write32(GICR_ICENABLER, 0xffff0000);
	gicr_sgi_write32(GICR_ISENABLER, 0x0000ffff);

	/* configure SGI and PPI as non-secure Group-1 */
	gicr_sgi_write32(GICR_IGROUPR0, 0xffffffff);

	gicv3_gicr_wait_for_rwp();
	isb();

	return 0;
}

static void  gicv3_icc_sre_init(void)
{
#ifdef CONFIG_VIRT
	write_sysreg(0xf, ICC_SRE_EL2);
#endif
	write_sysreg(0xf, ICC_SRE_EL1);
	//read_sysreg(ICC_SRE_EL1);
}

int gicv3_init(void)
{
	uint32_t i;
	uint32_t type;
	uint32_t nr_lines, pr;

	dlog_info("*** gicv3 init ***\n");

	memset(&g_device, 0, sizeof(struct gicv3_device));
	sl_init(&g_device.lock);

	g_device.gicd_base          = GICD_BASE;
	g_device.gicd_size          = 0x10000;
	g_device.gicr_frame_size    = 0x10000;
	g_device.gicr_rd_base       = GICD_BASE;
	g_device.gicr_sgi_base      = GICD_BASE + g_device.gicr_frame_size;
	g_device.gicr_vlpi_base     = GICD_BASE + 2 * g_device.gicr_frame_size;
	g_device.gic_base           = GICD_BASE;
	g_device.gic_size          = 0x300000;

	//dlog_info("gicv3 gicd@0x%x gicr@0x%x\n", g_device.gicd_base, g_device.gicr_base);

	sl_lock(&g_device.lock);

	/* disable gicd */
	gicd_write32(GICD_CTLR, 0x0);

	type = gicd_read32(GICD_TYPER);
	nr_lines = 32 * ((type & 0x1f) + 1);
	dlog_info("gicv3 typer-0x%x nr_lines-%d\n", type, nr_lines);

	/* default all golbal IRQS to level, active low */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 16) {
		gicd_write32(GICD_ICFGR + (i/16) * 4, 0x0);
	}

	/* default priority for global interrupts */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 4) {
		pr = (0xa0 << 24) | (0xa0 << 16) | (0xa0 << 8) | 0xa0;
		gicd_write32(GICD_IPRIORITYR + (i/4) *4, pr);
		pr = gicd_read32(GICD_IPRIORITYR + (i/4) *4);
	}

	/* disable all global interrupt */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32) {
		gicd_write32(GICD_ICENABLER + (i/32) * 4, 0xffffffff);
	}

	/* configure SPIs as non-secure GROUP-1 */
	for (i = GICV3_NR_LOCAL_IRQS; i < nr_lines; i += 32) {
		gicd_write32(GICD_IGROUPR + (i/32) * 4, 0xffffffff);
	}

	gicv3_gicd_wait_for_rwp();

	/* enable the gicd */
	gicd_write32(GICD_CTLR,
			GICD_CTLR_ENABLE_GRP1 | GICD_CTLR_ENABLE_GRP1A | GICD_CTLR_ARE_NS | 1);

	isb();

	gicv3_icc_sre_init();
	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	sl_unlock(&g_device.lock);

	return 0;
}

int gicv3_secondary_init(void)
{
	sl_lock(&g_device.lock);

	gicv3_icc_sre_init();
	gicv3_gicr_init();
	gicv3_gicc_init();
	gicv3_hyp_init();

	sl_unlock(&g_device.lock);

	return 0;
}

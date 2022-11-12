#include "hf/device/vgic.h"
#include "hf/device/gic.h"
#include "hf/mm.h"
#include "gicv2.h"

int vgicv2_init(struct vm *vm, struct mpool *ppool);
int gicv2_init(void);
int gicv2_secondary_init(void);
void gicv2_mm_init(
    struct mm_stage1_locked stage1_locked, struct mpool *ppool);

int vgic_init(struct vm *vm, struct mpool *ppool)
{
    return vgicv2_init(vm, ppool);
}

int gic_init(void)
{
    return gicv2_init();
}

int gic_secondary_init(void)
{
    return gicv2_secondary_init();
}

void gic_mm_init(
    struct mm_stage1_locked stage1_locked, struct mpool *ppool)
{
	/* Map page for gic */
    gicv2_mm_init(stage1_locked, ppool);
}

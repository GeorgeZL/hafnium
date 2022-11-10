#include "hf/device/gic.h"

#if GIC_VERSION == 3 || GIC_VERSION ==4
#include "gicv3.h"
#elif GIC_VERSION == 2
#include "gicv2.h"
#endif

#if GIC_VERSION == 3 || GIC_VERSION ==4
#if 0
int vgicv3_init(struct vm *vm, struct mpool *ppool);
int gicv3_init(void);
int gicv3_secondary_init(void);
#else
int vgicv3_init(struct vm *vm, struct mpool *ppool)
{
    return 0;
}

int gicv3_init(void)
{
    return 0;
}

int gicv3_secondary_init(void)
{
    return 0;
}

#endif
#elif GIC_VERSION == 2
int vgicv2_init(struct vm *vm, struct mpool *ppool);
int gicv2_init(void);
int gicv2_secondary_init(void);
#endif

int vgic_init(struct vm *vm, struct mpool *ppool)
{
#if GIC_VERSION == 3 || GIC_VERSION ==4
    return vgicv3_init(vm, ppool);
#elif GIC_VERSION == 2
    return vgicv2_init(vm, ppool);
#endif
}

int gic_init(void)
{
#if GIC_VERSION == 3 || GIC_VERSION ==4
    return gicv3_init();
#elif GIC_VERSION == 2
    return gicv2_init();
#endif
}

int gic_secondary_init(void)
{
#if GIC_VERSION == 3 || GIC_VERSION ==4
    return gicv3_secondary_init();
#elif GIC_VERSION == 2
    return gicv2_secondary_init();
#endif
}


#include <hf/list.h>
#include <hf/device/vdev.h>
#include <hf/vcpu.h>
#include <hf/dlog.h>
#include <hf/check.h>
#include <hf/interrupt.h>
#include <hf/bitops.h>
#include "gicv2.h"

#define MMIO_GET_SIZE(mmio)     (1 << ((mmio)->sas))
#define MMIO_IS_READ(mmio)      (((mmio)->wnr == 0) ? true : false)

#define ENABLE_VGIC 0
#define NAME_LNE    20

enum GICD_TYPE_GROUP {
    GICD_TYPE_INVAL = 0x0,
    GICD_TYPE_CTLR = 0x1,
    GICD_TYPE_TYPER,
    GICD_TYPE_IIDR,
    GICD_TYPE_IGROUPR,
    GICD_TYPE_ISENABLER,
    GICD_TYPE_ICENABLER,
    GICD_TYPE_ISPENDR,
    GICD_TYPE_ICPENDR,
    GICD_TYPE_ISACTIVER,
    GICD_TYPE_ICACTIVER,
    GICD_TYPE_IPRIORITYR,
    GICD_TYPE_ITARGETSR,
    GICD_TYPE_ICFGR,
    GICD_TYPE_NSACR,
    GICD_TYPE_SGIR,
    GICD_TYPE_CPENDSGIR,
    GICD_TYPE_SPENDSGIR,

    GICD_TYPE_MAX,
};

struct vgicv2_reg_info {
    char name[NAME_LNE];
    uint32_t start;
    uint32_t end;
    uint32_t stride;
};

#define VGICV2_MODE_HWA 0x0 // hardware accelerate mode
#define VGICV2_MODE_SWE 0x1 // software emulate mode
#define IN_RANGE(r, s, e)   ((((r) >= (s)) && ((r) <= (e))) ? 1 : 0)
#define OffsetAlign(offset, size_align)   align_down((offset), (size_align))
#define ValueAlign32(value, offset, size) \
    ((value & (1 << ((size * BITS_PER_BYTE) - 1))) << (offset * BITS_PER_BYTE))

#define GET_IRQ_BASE(o, s, e, l) \
    ((l) != 0 ? ((align_down((o) - (s), 4) * BITS_PER_BYTE) / (l)) : INVALID_INTRID)

#define GET_IRQ(o, s, e, l) \
    ((INVALID_INTRID != GET_IRQ_BASE(o, s, e , l)) ? \
        (GET_IRQ_BASE(o, s, e, l) + ((o) - align_down((o) - (s), 4)) / (l)): INVALID_INTRID)

static struct vgicv2_reg_info reg_infos[GICD_TYPE_MAX] = {
#define REG_INFO(name, s, e, stride)    \
    [GICD_TYPE_##name] = {"GICD_TYPE_"#name, s, e, stride}
    REG_INFO(INVAL,     0xffffffff,         0xffffffff,     0x0),
    REG_INFO(CTLR,      GICD_CTLR,          GICD_CTLR,      0x0),
    REG_INFO(TYPER,     GICD_TYPER,         GICD_TYPER,     0x0),
    REG_INFO(IIDR,      GICD_IIDR,          GICD_IIDR,      0x0),
    REG_INFO(IGROUPR,   GICD_IGROUPR,       GICD_IGROUPRN,  0x1),
    REG_INFO(ISENABLER, GICD_ISENABLER,     GICD_ISENABLERN,    0x1),
    REG_INFO(ICENABLER, GICD_ICENABLER,     GICD_ICENABLERN,    0x1),
    REG_INFO(ISPENDR,   GICD_ISPENDR,       GICD_ISPENDRN,  0x1),
    REG_INFO(ICPENDR,   GICD_ICPENDR,       GICD_ICPENDRN,  0x1),
    REG_INFO(ISACTIVER, GICD_ISACTIVER,     GICD_ISACTIVERN,    0x1),
    REG_INFO(ICACTIVER, GICD_ICACTIVER,     GICD_ICACTIVERN,    0x1),
    REG_INFO(IPRIORITYR,    GICD_IPRIORITYR,    GICD_IPRIORITYRN,   0x8),
    //REG_INFO(ITARGETSRR,    GICD_ITARGETSRR,    GICD_ITARGETSR7,    0x8),
    REG_INFO(ITARGETSR, GICD_ITARGETSR,    GICD_ITARGETSRN,    0x8),
    REG_INFO(ICFGR,     GICD_ICFGR,         GICD_ICFGRN,    0x2),
    REG_INFO(NSACR,     GICD_NSACR,         GICD_NSACRN,    0x2),
    REG_INFO(SGIR,      GICD_SGIR,          GICD_SGIR,      0x0),
    REG_INFO(CPENDSGIR, GICD_CPENDSGIR,     GICD_CPENDSGIRN,    0x8),
    REG_INFO(SPENDSGIR, GICD_SPENDSGIR,     GICD_SPENDSGIRN,    0x8),
#undef REG_INFO
};

#define vdev_to_vgicv2(vdev) \
	(struct vgicv2_dev *)CONTAINER_OF(vdev, struct vgicv2_dev, vdev)

struct vgicv2_dev {
	struct vdev vdev;
    struct spinlock lock;

    /* GIC information */
	uint32_t ctlr;
	uint32_t typer;
	uint32_t iidr;

	uintpaddr_t gicd_base;
	uint32_t gicd_size;
};

struct vgic_set {
    struct vgicv2_dev devices[MAX_VMS];
    bool flag[MAX_VMS];

    struct spinlock lock;
};

static struct vgic_set vgic_set = {0};
void *memset(void *s, int c, size_t n);

char *vgic_get_irq_name(uint32_t type)
{
    if (type < GICD_TYPE_MAX)
        return reg_infos[type].name;

    return NULL;
}

static uint32_t vgicv2_get_reg_type(uint32_t offset)
{
    uint32_t type;

    for (type = GICD_TYPE_CTLR; type < GICD_TYPE_MAX; type++) {
        if (IN_RANGE(offset, reg_infos[type].start, reg_infos[type].end))
            return type;
    }

    return GICD_TYPE_INVAL;
}

static struct vgic_set *get_vgic_set(void)
{
    return &vgic_set;
}

static void vgic_set_init(struct vgic_set *set)
{
    uint32_t index = 0;

    if (set != NULL) {
        memset(set, 0, sizeof(struct vgic_set));
        sl_init(&set->lock);

        for (index = 0; index < MAX_VMS; index++) {
            set->flag[index] = false;
        }
    }
}

static void vgicv2_device_init(struct vgicv2_dev *device)
{
    memset(device, 0, sizeof(struct vgicv2_dev));
    sl_init(&device->lock);

    device->ctlr = gicv2_get_ctrl();
    device->typer = gicv2_get_typer();
    device->iidr = gicv2_get_iidr();
}

static struct vgicv2_dev *request_vgic_device(struct vgic_set *set)
{
    struct vgicv2_dev *device = NULL;

    if (set != NULL) {
        uint32_t index = 0;

        sl_lock(&set->lock);
        while (set->flag[index]) index++;
        device = (index < MAX_VMS) ? &set->devices[index] : NULL;
        sl_unlock(&set->lock);
    }

    if (device) {
        vgicv2_device_init(device);
    }

    return device;
}

static void release_vgic_device(struct vgic_set *set, struct vgicv2_dev *device)
{
    if (set != NULL) {
        int32_t index = -1;

        sl_lock(&set->lock);
        index = (device - set->devices) / sizeof(struct vgicv2_dev);
        if (index >= 0 && index < MAX_VMS && set->flag[index]) {
            set->flag[index] = false;
            memset(device, 0x0, sizeof(struct vgicv2_dev));
        }
        sl_unlock(&set->lock);
    }
}

void vgicv2_set_irq_type(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint32_t type;

    irq = ((offset - GICD_ICFGR) * BITS_PER_BYTE) / 2;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * 4; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            type = (value >> (index * 2)) & 0x3;
            gicv2_set_irq_type(irq, type);
        }
    }
}

uint32_t vgicv2_get_irq_type(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t type;
    uint32_t value = 0;

    irq = ((offset - GICD_ICFGR) * BITS_PER_BYTE) / 2;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * 4; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            type = gicv2_get_irq_type(irq);
            value |= (type << (index * 2));
        }
    }

    return value;
}

void vgicv2_clear_pending(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;

    irq = (offset - GICD_ICPENDR) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if ((value & (1 << index)) && hwirq_is_belongs_current(irq)) {
            gicv2_clear_irq_pending(irq);
        }
    }
}

uint32_t vgicv2_get_clear_pending_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t pending;
    uint32_t value = 0;

    irq = (offset - GICD_ICPENDR) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            pending = gicv2_get_clear_pending_state(irq);
            value |= (!!pending << index);
        }
    }

    return value;
}

uint32_t vgicv2_get_pending_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t pending;
    uint32_t value = 0;

    irq = (offset - GICD_ISPENDR) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            pending = gicv2_get_irq_pending_state(irq);
            value |= (!!pending << index);
        }
    }

    return value;
}

void vgicv2_set_irq_pending(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;

    irq = (offset - GICD_ISPENDR) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if ((value & (1 << index)) && hwirq_is_belongs_current(irq)) {
            gicv2_set_irq_pending(irq);
        }
    }
}

void vgicv2_set_irq_affinity(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint8_t affinity = 0;

    irq = (offset - GICD_ITARGETSR);
    if (irq < 32) {
        dlog_error("ITARGETSR0 ~ ITARGETSR7 is RO\n");
    } else {
        for (int index = 0; index < MMIO_GET_SIZE(mmio); index++, irq++) {
            if (hwirq_is_belongs_current(irq)) {
                affinity = (value >> (index * BITS_PER_BYTE)) & 0xff;
                gicv2_set_irq_affinity(irq, affinity);
            }
        }   
    }
}

uint32_t vgicv2_get_irq_affinity(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t value = 0;
    uint8_t affinity = 0;

    irq = (offset - GICD_ITARGETSR);
    if (irq < 32) { // SGI / PPI
        value = gicv2_get_direct(offset, MMIO_GET_SIZE(mmio));
    } else {
        for (int index = 0; index < MMIO_GET_SIZE(mmio); index++, irq++) {
            if (hwirq_is_belongs_current(irq)) {
                affinity = gicv2_get_irq_affinity(irq);
                value |= (affinity << (index * BITS_PER_BYTE));
            }
        }
    }

    return value;
}

uint32_t vgicv2_get_irq_mask(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t state;
    uint32_t value = 0;

    irq = (offset - GICD_ICENABLER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            state = gicv2_get_mask_irq(irq);
            value |= ((!!state) << index);
        }
    }
   
    return value;
}

void vgicv2_set_irq_mask(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint32_t state;

    irq = (offset - GICD_ICENABLER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        state = (value & (1 << index));
        if (state && hwirq_is_belongs_current(irq))
            gicv2_mask_irq(irq);
    }
}

uint32_t vgicv2_get_irq_unmask(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t state;
    uint32_t value = 0;

    irq = (offset - GICD_ISENABLER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            state = gicv2_get_unmask_irq(irq);
            value |= ((!!state) << index);
        }
    }
   
    return value;
}

void vgicv2_set_irq_unmask(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint32_t state;

    irq = (offset - GICD_ISENABLER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        state = (value & (1 << index));
        if (state && hwirq_is_belongs_current(irq))
            gicv2_unmask_irq(irq);
    }
}

void vgicv2_set_irq_active(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;

    irq = (offset - GICD_ISACTIVER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if ((value & (1 << index)) && hwirq_is_belongs_current(irq)) {
            gicv2_active_irq(irq);
        }
    }
}

uint32_t vgicv2_get_irq_active_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t value = 0;
    uint32_t state = 0;
    
    irq = (offset - GICD_ISACTIVER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            state = gicv2_irq_is_active(irq);
            value |= ((!!state) << index);
        }
    }

    return value;
}

void vgicv2_set_irq_deactive(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;

    irq = (offset - GICD_ICACTIVER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if ((value & (1 << index)) && hwirq_is_belongs_current(irq)) {
            gicv2_deactive_irq(irq);
        }
    }
}

uint32_t vgicv2_get_irq_deactive_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t value = 0;
    uint32_t state = 0;
    
    irq = (offset - GICD_ICACTIVER) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            state = gicv2_irq_is_deactive(irq);
            value |= ((!!state) << index);
        }
    }

    return value;
}

uint32_t vgicv2_get_irq_priority(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t value = 0;
    uint8_t priority = 0;

    irq = (offset - GICD_IPRIORITYR);
    for (int index = 0; index < MMIO_GET_SIZE(mmio); index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            priority = gicv2_get_irq_priority(irq);
            value |= (priority << (index * BITS_PER_BYTE));
        }
    }

    return value;
}

void vgicv2_set_irq_priority(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint8_t priority = 0;

    irq = (offset - GICD_IPRIORITYR);
    for (int index = 0; index < MMIO_GET_SIZE(mmio); index++, irq++) {
        if (hwirq_is_belongs_current(irq)) {
            priority = (value >> (index * BITS_PER_BYTE)) & 0xff;
            gicv2_set_irq_priority(irq, priority);
        }
    }
}

void vgicv2_sgi_set_conf(MMIOInfo_t *mmio, uint32_t value)
{
    uint32_t mode, targets, sgi, config;

    /* TODO: SGI should not omit to the cput that not belonged to current vm */
    mode = (value >> 24) & 0x3;
    targets = (value >> 16) & 0xff;
    sgi = value & 0xf;

    config = ((mode << 24) | (targets << 16) | (sgi));
    gicv2_sgi_set_conf(config);
}

void vgicv2_sgi_clear_pending(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t sgi;
    uint8_t mask;

    sgi = (offset - GICD_CPENDSGIR);
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio); index++, sgi++) {
        mask = (value >> (index * BITS_PER_BYTE)) & 0xff;
        gicv2_sgi_clear(sgi, mask);
    }
}

uint32_t vgicv2_sgi_get_pending_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t sgi;
    uint8_t mask;
    uint32_t value = 0;

    sgi = (offset - GICD_CPENDSGIR);
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio); index++, sgi++) {
        mask = gicv2_sgi_pending_state(sgi);
        value |= (mask << (index * BITS_PER_BYTE));
    }

    return value;
}

void vgicv2_sgi_set_active(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t sgi;
    uint8_t mask;

    sgi = (offset - GICD_SPENDSGIR);
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio); index++, sgi++) {
        mask = (value >> (index * BITS_PER_BYTE)) & 0xff;
        gicv2_sgi_active(sgi, mask);
    }
}

uint32_t vgicv2_sgi_get_active_state(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t sgi;
    uint8_t mask;
    uint32_t value = 0;

    sgi = (offset - GICD_SPENDSGIR);
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio); index++, sgi++) {
        mask = gicv2_sgi_get_active_state(sgi);
        value |= (mask << (index * BITS_PER_BYTE));
    }

    return value;
}

uint32_t vgicv2_get_irq_group_info(MMIOInfo_t *mmio, uint32_t offset)
{
    uint32_t irq;
    uint32_t group = 0;
    uint32_t value = 0;
    
    if (offset == GICD_IGROUPR) {
        /* one register per cpu, not shared */
        value = gicv2_get_direct(GICD_IGROUPR, MMIO_GET_SIZE(mmio));
    } else {
        irq = (offset - GICD_IGROUPR);
        for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
            if (hwirq_is_belongs_current(irq)) {
                group = gicv2_get_irq_group(irq);
                value |= ((!!group) << index);
            }
        }
    }

    return value;
}

void vgicv2_set_irq_group(MMIOInfo_t *mmio, uint32_t offset, uint32_t value)
{
    uint32_t irq;
    uint8_t group = 0;

    irq = (offset - GICD_IGROUPR) * BITS_PER_BYTE;
    for (uint32_t index = 0; index < MMIO_GET_SIZE(mmio) * BITS_PER_BYTE; index++, irq++) {
        group = (value >> index);
        if (group && hwirq_is_belongs_current(irq)) {
            gicv2_set_irq_group(irq, group);
        }
    }
}

static int vgicv2_read(struct vcpu *vcpu,
    struct vgicv2_dev *vgic, MMIOInfo_t *mmio, uint32_t offset, uint32_t *v)
{
    struct vgicv2_reg_info *info = NULL;
    uint32_t type = GICD_TYPE_INVAL;
    uint32_t value = 0;

    type = vgicv2_get_reg_type(offset);
    info = &reg_infos[type];

    switch (type) {
        case GICD_TYPE_CTLR:
            value = vgic->ctlr;
            break;

        case GICD_TYPE_TYPER:
            value = vgic->typer;
            break;

        case GICD_TYPE_IIDR:
            value = vgic->iidr;
            break;

        case GICD_TYPE_SGIR:        /* SGIR: WO, RAZ */
            value = 0;
            break;

        case GICD_TYPE_CPENDSGIR:   /* CPENDSGIR */
            value = vgicv2_sgi_get_pending_state(mmio, offset);
            break;

        case GICD_TYPE_SPENDSGIR:   /* SPENDSGIR */
            value = vgicv2_sgi_get_active_state(mmio, offset);
            break;

        case GICD_TYPE_ITARGETSR:   /* ITARGETSR */
            value = vgicv2_get_irq_affinity(mmio, offset);
            break;

        case GICD_TYPE_ISENABLER:   /* ISENABLER */
            value = vgicv2_get_irq_unmask(mmio, offset);
            break;

        case GICD_TYPE_ICENABLER:   /* ICENABLER */
            value = vgicv2_get_irq_mask(mmio, offset);
            break;

        case GICD_TYPE_ISPENDR:     /* ISPENDR */
            value = vgicv2_get_pending_state(mmio, offset);
            break;

        case GICD_TYPE_ICPENDR:     /* ICPENDR */
            value = vgicv2_get_clear_pending_state(mmio, offset);
            break;

        case GICD_TYPE_ISACTIVER:   /* ISACTIVER */
            value = vgicv2_get_irq_active_state(mmio, offset);
            break;
            
        case GICD_TYPE_ICACTIVER:   /* ICACTIVER */
            value = vgicv2_get_irq_deactive_state(mmio, offset);
            break;

        case GICD_TYPE_IPRIORITYR:  /* IPRIORITYR */
            value = vgicv2_get_irq_priority(mmio, offset);
            break;

        case GICD_TYPE_ICFGR:       /* ICFGR */
            value = vgicv2_get_irq_type(mmio, offset);
            break;

        case GICD_TYPE_NSACR:       /* NSACR */
        default:
            value = gicv2_get_direct(offset, MMIO_GET_SIZE(mmio));
            break;
    }

    *v = value;

	return 0;
}

static int vgicv2_write(struct vcpu *vcpu,
    struct vgicv2_dev *gic, MMIOInfo_t *mmio, uint32_t offset, uint32_t *v)
{
    struct vgicv2_reg_info *info = NULL;
    uint32_t type = GICD_TYPE_INVAL;
    uint32_t value;

    type = vgicv2_get_reg_type(offset);
    info = &reg_infos[type];
    value = *v;

    switch (type) {
        case GICD_TYPE_CTLR:
            gic->ctlr = value;
            break;

        case GICD_TYPE_SGIR:
            vgicv2_sgi_set_conf(mmio, value);
            break;

        case GICD_TYPE_CPENDSGIR:
            vgicv2_sgi_clear_pending(mmio, offset, value);
            break;

        case GICD_TYPE_SPENDSGIR:
            vgicv2_sgi_set_active(mmio, offset, value);
            break;

        case GICD_TYPE_IGROUPR:
            vgicv2_set_irq_group(mmio, offset, value);
            break;

        case GICD_TYPE_ISENABLER:
            vgicv2_set_irq_unmask(mmio, offset, value);
            break;

        case GICD_TYPE_ICENABLER:
            vgicv2_set_irq_mask(mmio, offset, value);
            break;

        case GICD_TYPE_ISPENDR:
            vgicv2_set_irq_pending(mmio, offset, value);
            break;

        case GICD_TYPE_ICPENDR:
            vgicv2_clear_pending(mmio, offset, value);
            break;

        case GICD_TYPE_ISACTIVER:
            vgicv2_set_irq_active(mmio, offset, value);
            break;

        case GICD_TYPE_ICACTIVER:
            vgicv2_set_irq_deactive(mmio, offset, value);
            break;

        case GICD_TYPE_IPRIORITYR:
            vgicv2_set_irq_priority(mmio, offset, value);
            break;

        case GICD_TYPE_ITARGETSR:
            vgicv2_set_irq_affinity(mmio, offset, value);
            break;

        case GICD_TYPE_ICFGR:
            vgicv2_set_irq_type(mmio, offset, value);
            break;

        case GICD_TYPE_IIDR:
        case GICD_TYPE_TYPER:
            dlog_error(
                "'WRITE' to %x '%s' is not Invalid\n", offset, vgic_get_irq_name(type));
            break;

        default:
            gicv2_set_direct(offset, value, MMIO_GET_SIZE(mmio));
    }

	return 0;
}

static int vgicv2_read_direct(struct vcpu *vcpu, struct vgicv2_dev *vgic,
		unsigned long offset, uint8_t size, uint32_t *v)
{
    uint32_t value = 0;

    switch (offset) {
        case GICD_CTLR:
            value = vgic->ctlr;
            break;

        case GICD_TYPER:
            value = vgic->typer;
            break;
            
        case GICD_IIDR:
            value = vgic->iidr;
            break;

        default:
            value = gicv2_get_direct(offset, size);
            break;

    }
    *v = value;

    return 0;
}

static int vgicv2_write_direct(struct vcpu *vcpu, struct vgicv2_dev *vgic,
		uint32_t offset, uint8_t size, uint32_t *v)
{
    uint32_t value = *v;

    switch (offset) {
        case GICD_CTLR:
            value = vgic->ctlr;
            break;

        case GICD_TYPER:
        case GICD_IIDR:
            dlog_error("reg-%x write is not supported\n", offset);
            break;

        default:
            gicv2_set_direct(offset, value, size);
            break;
    }

    return 0;
}

int vgicv2_mmio_direct_align32_read(
    struct vdev *vdev, struct vcpu *vcpu, uint64_t address, uint8_t size, uint32_t *value)
{
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);
    uint32_t offset = address - gic->gicd_base;
    uint32_t offset_align, size_align, value_align, value_tmp, mask;
    int retval;

    size_align = sizeof(uint32_t);
    offset_align = align_down(offset, sizeof(uint32_t));
    retval = vgicv2_read_direct(vcpu, gic, offset_align, size_align, &value_align);
    if (retval) {
        dlog_error("failed to read - 0x%03x (0x%03x)\n", offset_align, offset);
    }

    value_tmp = value_align;
    if (offset_align != offset) {
        mask = ((1 << (size * BITS_PER_BYTE)) - 1);
        value_tmp = value_align >> ((offset - offset_align) * BITS_PER_BYTE);
        value_tmp &= mask;
    }
    *value = value_tmp;

    return retval;
}

int vgicv2_mmio_direct_align32_write(
    struct vdev *vdev, struct vcpu *vcpu, uint64_t address, uint8_t size, uint32_t *value)
{
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);
    uint32_t offset = address - gic->gicd_base;
    uint32_t offset_align, size_align, value_align, mask;
    int retval;

    if (offset >= 0x800 && offset < 0xc00) {
        vgicv2_write_direct(vcpu, gic, offset, size, value);
        return 0;
    }

    size_align = sizeof(uint32_t);
    offset_align = align_down(offset, sizeof(uint32_t));
    value_align = *value;
    if (offset_align != offset) {
        mask = ((1 << (size * BITS_PER_BYTE)) - 1);
        value_align &= mask;
        value_align = (value_align << ((offset - offset_align) *BITS_PER_BYTE));
    }
    
    retval = vgicv2_write_direct(vcpu, gic, offset_align, size_align, &value_align);
    if (retval) {
        dlog_error("failed to write: 0x%03x (0x%03x)\n", offset_align, offset);
    }

    if ((offset != offset_align) || (*value != value_align)) {
        dlog_info("(offset : value): (0x%03x : 0x%08x) --> (0x%03x : 0x%08x)\n", offset, *value, offset_align, value_align);
    }

    return retval;
}

static int vgicv2_mmio_direct_no_align32(struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio, 
		int read, uint64_t address, uint8_t size, uint32_t *value)
{
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);
    uint32_t offset = address - gic->gicd_base;
    int retval;

    if (read) {
        retval = vgicv2_read_direct(vcpu, gic, offset, size, value);
    } else {
        retval = vgicv2_write_direct(vcpu, gic, offset, size, value);
    }

    return retval;
}

int vgicv2_mmio_direct_align32(struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio,
		int read, uint64_t address, uint8_t size, uint32_t *value)
{
    if (read) {
        //return vgicv2_mmio_direct_no_align32(vdev, vcpu, read, address, size, value);
        return vgicv2_mmio_direct_align32_read(vdev, vcpu, address, size, value);
    } else {
        //return vgicv2_mmio_direct_no_align32(vdev, vcpu, read, address, size, value);
        return vgicv2_mmio_direct_align32_write(vdev, vcpu, address, size, value);
    }
}

int vgicv2_mmio_direct_handler(struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio,
		int read, uint64_t address, uint8_t size, uint32_t *value)
{
    int retval = 0;

    retval = vgicv2_mmio_direct_align32(vdev, vcpu, mmio, read, address, size, value);
    if (retval) {
        dlog_error("direct align32 ops failed\n");
    }

    retval = vgicv2_mmio_direct_no_align32(vdev, vcpu, mmio, read, address, size, value);
    if (retval) {
        dlog_error("direct non-align32 ops failed\n");
    }

    return retval;
}

static int vgicv2_mmio_handler(struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio,
		int read, uint64_t address, uint8_t size, uint32_t *value)
{
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);
    uint32_t offset = address - gic->gicd_base;
    int retval = 0;

    if (read) {
        retval = vgicv2_read(vcpu, gic, mmio, offset, value);
    } else {
        retval = vgicv2_write(vcpu, gic, mmio, offset, value);
    }

    return retval;
}

int vgicv2_mmio_read(
    struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio, uint64_t address, uint8_t size, uint32_t *read_value)
{
#if ENABLE_VGIC
    bool vgic = true;
#else
    bool vgic = false;
#endif
    int retval = 0;

    if ((size != 1) && (size != 2) && (size != 4)) {
        return -EINVAL;
    }

    *read_value = 0;

    if (vgic) {
        retval = vgicv2_mmio_handler(vdev, vcpu, mmio, 1, address, size, read_value);
    } else {
        retval =  vgicv2_mmio_direct_handler(vdev, vcpu, mmio, 1, address, size, read_value);
    }

    return retval;
}

int vgicv2_mmio_write(
    struct vdev *vdev, struct vcpu *vcpu, MMIOInfo_t *mmio, uint64_t address, uint8_t size, uint32_t *write_value)
{
#if ENABLE_VGIC
    bool vgic = true;
#else
    bool vgic = false;
#endif
    int retval = 0;

    if ((size != 1) && (size != 2) && (size != 4)) {
        return -EINVAL;
    }

    if (vgic) {
        retval = vgicv2_mmio_handler(vdev, vcpu, mmio, 0, address, size, write_value);
    } else {
        retval = vgicv2_mmio_direct_handler(vdev, vcpu, mmio, 0, address, size, write_value);
    }

    return retval;
}

void vgicv2_reset(struct vdev *vdev)
{
	dlog_info("vgicv2 device reset\n");
}

void vgicv2_deinit(struct vdev *vdev)
{
	struct vgicv2_dev *dev = vdev_to_vgicv2(vdev);

	if (!dev)
		return;
}

#if 0
static int get_vgicv2_info(struct device_node *node, struct vgicv2_info *vinfo)
{
	memset(vinfo, 0, sizeof(struct vgicv2_info));
	ret = translate_device_address_index(node, &vinfo->gicd_base,
			&vinfo->gicd_size, 0);
	if (ret) {
		dlog_error("no gicv3 address info found\n");
		return -ENOENT;
	}

	ret = translate_device_address_index(node, &vinfo->gicc_base,
			&vinfo->gicc_size, 1);
	if (ret) {
		dlog_error("no gicc address info found\n");
		return -ENOENT;
	}

	if (vinfo->gicd_base == 0 || vinfo->gicd_size == 0 ||
			vinfo->gicc_base == 0 || vinfo->gicc_size == 0) {
		dlog_error("gicd or gicc address info not correct\n");
		return -EINVAL;
	}

	translate_device_address_index(node, &vinfo->gich_base,
			&vinfo->gich_size, 2);
	translate_device_address_index(node, &vinfo->gicv_base,
			&vinfo->gicv_size, 3);
	dlog_info("vgicv2: address 0x%x 0x%x 0x%x 0x%x\n",
			vinfo->gicd_base, vinfo->gicd_size,
			vinfo->gicc_base, vinfo->gicc_size);

	return 0;

}
#endif

int vgicv2_init(struct vm *vm, struct mpool *ppool)
{
	int ret = 0;
	struct vdev *vdev = NULL;
	struct vgicv2_dev *dev = NULL;
    struct vgic_set *set = NULL;

	dlog_info("create vgicv2 for vm-%d\n", vm->id);

    set = get_vgic_set();
    vgic_set_init(set);
	CHECK(dev = request_vgic_device(set));

	dev->gicd_base = GICD_BASE;
    dev->gicd_size = 0x1000;
	vdev = &dev->vdev;
	ret = vdev_init(vdev, dev->gicd_base, dev->gicd_size);
	if (ret) {
		dlog_error("Virtual device initialization failed");
		goto init_failed;
	}

    vdev_set_name(vdev, "vgicv2");
	vdev->read = vgicv2_mmio_read;
	vdev->write = vgicv2_mmio_write;
	vdev->deinit = vgicv2_deinit;
	vdev->reset = vgicv2_reset;

	if (!register_one_vdev(vm, vdev, ppool)) {
		dlog_error("Registe virtual device '%s' failed", vdev->name);
        ret = -EINVAL;
		goto registe_failed;
	}

	dlog_info("vgicv2: create vdev for vm done\n");
out:
	return ret;

registe_failed:
	vdev_deinit(vdev);
init_failed:
	release_vgic_device(set, dev);

	goto out;
}

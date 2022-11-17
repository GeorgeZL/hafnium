/*
 * Copyright (C) 2018 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <hf/list.h>
#include <hf/device/vdev.h>
#include <hf/vcpu.h>
#include <hf/dlog.h>
#include <hf/check.h>
#include <hf/interrupt.h>
#include <hf/bitops.h>
#include "gicv2.h"

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
    GICD_TYPE_ITARGETSRR,
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
    REG_INFO(ITARGETSRR,    GICD_ITARGETSRR,     GICD_ITARGETSR7,    0x8),
    REG_INFO(ITARGETSR, GICD_ITARGETSR8,    GICD_ITARGETSRN,    0x8),
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

static int vgicv2_read(struct vcpu *vcpu, struct vgicv2_dev *vgic,
		unsigned long offset, uint8_t size, unsigned long *v)
{
    uint32_t type = GICD_TYPE_INVAL;
    uint32_t value = 0;
    uint32_t irq;
    uint32_t off;
    struct vgicv2_reg_info *info = NULL;

    type = vgicv2_get_reg_type(offset);
    info = &reg_infos[type];
    irq = (info->stride != 0) ? \
        ((align_down(offset - info->start, 4) * BITS_PER_BYTE) / info->stride) : INVALID_INTRID;

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

#define CASEX(name) \
        case GICD_TYPE_##name:  \
            value = gicv2_vdev_get_##name(irq); \
            break

        CASEX(IGROUPR);
        CASEX(ISENABLER);
        CASEX(ICENABLER);
        CASEX(ISPENDR);
        CASEX(ICPENDR);
        CASEX(ISACTIVER);
        CASEX(ICACTIVER);
        CASEX(IPRIORITYR);
        CASEX(ITARGETSR);
        CASEX(ITARGETSRR);
        CASEX(ICFGR);
        CASEX(NSACR);
#undef CASEX

       default:
            dlog_error("Invalid gicd register (0x%x - %s) to read\n", offset, vgic_get_irq_name(type));
            break;
    }

    if (size != sizeof(uint32_t)) {
        off = (offset - info->start) - align_down(offset - info->start, 4);
        value >>= off * BITS_PER_BYTE;
        value &= (1 << (size * BITS_PER_BYTE)) - 1;
    }
    *v = value;

	return 0;
}

static int vgicv2_write(struct vcpu *vcpu, struct vgicv2_dev *gic,
		unsigned long offset, uint8_t size, unsigned long *v)
{
    uint32_t type = GICD_TYPE_INVAL;
    uint32_t value, value_orin = *v;
    uint32_t irq;
    uint32_t off;
    struct vgicv2_reg_info *info = NULL;

    type = vgicv2_get_reg_type(offset);
    info = &reg_infos[type];
    irq = (info->stride != 0) ? \
        ((align_down(offset - info->start, 4) * BITS_PER_BYTE) / info->stride) : INVALID_INTRID;
    if (size != sizeof(uint32_t)) {
        value_orin &= (1 << (size * BITS_PER_BYTE)) - 1;
        off = (offset - info->start) - align_down(offset - info->start, 4);
        value_orin <<= off * BITS_PER_BYTE;
    }
    value = value_orin;

    switch (type) {
        case GICD_TYPE_CTLR:
            gic->ctlr = value;
            break;

        case GICD_TYPE_IIDR:
        case GICD_TYPE_TYPER:
            dlog_error("reg-%x (%s) write is not supported\n", offset, vgic_get_irq_name(type));
            break;

#define CASEX(name) \
        case GICD_TYPE_##name:  \
            gicv2_vdev_set_##name(irq, value); \
            break

        CASEX(IGROUPR);
        CASEX(NSACR);
        CASEX(ISACTIVER);
        CASEX(ICACTIVER);
        CASEX(IPRIORITYR);
        CASEX(ITARGETSRR);
        CASEX(ISPENDR);
        CASEX(ICPENDR);
        CASEX(ICENABLER);
        CASEX(ICFGR);
        CASEX(ISENABLER);
        CASEX(ITARGETSR);
#undef CASEX

        default:
            dlog_error("Invalid gicd register (0x%x) to write\n", offset, vgic_get_irq_name(type));
            break;
    }

	return 0;
}

static int vgicv2_mmio_handler(struct vdev *vdev, struct vcpu *vcpu,
		int read, uint64_t address, uint8_t size, unsigned long *value)
{
	struct vgicv2_dev *gic = vdev_to_vgicv2(vdev);
    uint64_t offset = address - gic->gicd_base;

	if (read)
		return vgicv2_read(vcpu, gic, offset, size, value);
	else
		return vgicv2_write(vcpu, gic, offset, size, value);
}

int vgicv2_mmio_read(
    struct vdev *vdev, struct vcpu *vcpu, uint64_t address, uint8_t size, uint64_t *read_value)
{
	return vgicv2_mmio_handler(vdev, vcpu, 1, address, size, read_value);
}

int vgicv2_mmio_write(
    struct vdev *vdev, struct vcpu *vcpu, uint64_t address, uint8_t size, uint64_t *write_value)
{
	return vgicv2_mmio_handler(vdev, vcpu, 0, address, size, write_value);
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
    dev->gicd_size = 0x10000;
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

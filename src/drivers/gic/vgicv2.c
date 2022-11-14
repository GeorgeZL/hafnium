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
#include "gicv2.h"

#define NAME_LNE    20

enum GICD_TYPE_GROUP {
    GICD_TYPE_INVAL = 0x0,
    GICD_TYPE_CTLR = 0x1,
    GICD_TYPE_TYPER,
    GICD_TYPE_IIDR,
    GICD_TYPE_IGRPn,
    GICD_TYPE_ISEn,
    GICD_TYPE_ICEn,
    GICD_TYPE_ISPn,
    GICD_TYPE_ICPn,
    GICD_TYPE_ISAn,
    GICD_TYPE_ICAn,
    GICD_TYPE_IPRn,
    GICD_TYPE_ITAnR,
    GICD_TYPE_ITAn,
    GICD_TYPE_ICFn,
    GICD_TYPE_NSAn,
    GICD_TYPE_SGIR,
    GICD_TYPE_CPSGIn,
    GICD_TYPE_SPSGIn,

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
    REG_INFO(CTLR,  0x0,    0x0,    0x0),
    REG_INFO(TYPER, 0x4,    0x4,    0x0),
    REG_INFO(IIDR,  0x8,    0x8,    0x0),
    REG_INFO(IGRPn, 0x80,   0xFC,   0x1),
    REG_INFO(ISEn,  0x100,  0x17C,  0x1),
    REG_INFO(ICEn,  0x180,  0x1FC,  0x1),
    REG_INFO(ISPn,  0x200,  0x27C,  0x1),
    REG_INFO(ICPn,  0x280,  0x2FC,  0x1),
    REG_INFO(ISAn,  0x300,  0x37C,  0x1),
    REG_INFO(ICAn,  0x380,  0x3FC,  0x1),
    REG_INFO(IPRn,  0x400,  0x7F8,  0x8),
    REG_INFO(ITAnR, 0x800,  0x81C,  0x8),
    REG_INFO(ITAn,  0x820,  0xBF8,  0x8),
    REG_INFO(ICFn,  0xC00,  0xCFC,  0x2),
    REG_INFO(NSAn,  0xE00,  0xEFC,  0x2),
    REG_INFO(SGIR,  0xF00,  0xF00,  0x0),
    REG_INFO(CPSGIn,    0xF10,  0xF1C,  0x8),
    REG_INFO(SPSGIn,    0xF20,  0xF2C,  0x8),
#undef REG_INFO
};

struct vgicv2_dev {
	struct vdev vdev;
    struct spinlock lock;

    /* GIC information */
	uint32_t ctlr;
	uint32_t typer;
	uint32_t iidr;

	uintpaddr_t gicd_base;
	uint32_t gicd_size;

    struct spi_table irq_map;
};

struct vgic_set {
    struct vgicv2_dev devices[MAX_VMS];
    bool flag[MAX_VMS];

    struct spinlock lock;
};

static struct vgic_set vgic_set = {0};

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *des, const char *src);

#define vdev_to_vgicv2(vdev) \
	(struct vgicv2_dev *)CONTAINER_OF(vdev, struct vgicv2_dev, vdev)

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

static uint32_t vgicv2_get_mask(struct vgicv2_dev *vgic, uint32_t offset)
{
    uint32_t type = vgicv2_get_reg_type(offset);
    uint32_t stride = reg_infos[type].stride;
    uint32_t irq = (offset - reg_infos[type].start) * BITS_PER_BYTE / stride;
    uint8_t token = (1 << stride) - 1;
    uint8_t count = (BITS_PER_LONG / stride);
    uint32_t mask = 0;
    uint32_t map = vgic->irq_map.map[BIT_WORD(irq)];

    for (int i = 0; i < count; i++) {
        if (map & (1 << i))
            mask |= token << (stride * i);
    }

    return mask;
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
	uint32_t *value = (uint32_t *)v;
    uint32_t type = GICD_TYPE_INVAL;
    uint32_t mask = vgicv2_get_mask(vgic, offset);

    type = vgicv2_get_reg_type(offset);

    switch (type) {
        case GICD_TYPE_CTLR:
            break;
        case GICD_TYPE_TYPER:
            break;
        case GICD_TYPE_IIDR:
            break;

        case GICD_TYPE_IGRPn:
            break;
        case GICD_TYPE_ISEn:
            break;
        case GICD_TYPE_ICEn:
            break;
        case GICD_TYPE_ISPn:
            break;
        case GICD_TYPE_ICPn:
            break;
        case GICD_TYPE_ISAn:
            break;
        case GICD_TYPE_ICAn:
            break;
        case GICD_TYPE_IPRn:
            break;
        case GICD_TYPE_ITAnR:
            break;
        case GICD_TYPE_ITAn:
            break;
        case GICD_TYPE_ICFn:
            break;
        default:
            dlog_error("Invalid gicd register (0x%x) to read\n", offset);
            break;
    }

    dlog_error("gic to read register: '%s : 0x%x'\n", vgic_get_irq_name(type), offset);
    switch (size) {
        case 0:
            *value = mask & readb_gicd(offset);
            break;
        case 1:
            *value = mask & readw_gicd(offset);
            break;
        case 2:
            *value = mask & readl_gicd(offset);
            break;
        case 3:
            *value = mask & readq_gicd(offset);
            break;
        default:
            break;
    }

	return 0;
}

static int vgicv2_write(struct vcpu *vcpu, struct vgicv2_dev *gic,
		unsigned long offset, uint8_t size, unsigned long *v)
{
    uint32_t type = GICD_TYPE_INVAL;

    type = vgicv2_get_reg_type(offset);

    switch (type) {
        case GICD_TYPE_CTLR:
            break;
        case GICD_TYPE_IGRPn:
            break;
        case GICD_TYPE_ISEn:
            break;
        case GICD_TYPE_ICEn:
            break;
        case GICD_TYPE_ISPn:
            break;
        case GICD_TYPE_ICPn:
            break;
        case GICD_TYPE_ISAn:
            break;
        case GICD_TYPE_ICAn:
            break;
        case GICD_TYPE_IPRn:
            break;
        case GICD_TYPE_ITAn:
            break;
        case GICD_TYPE_ITAnR:
            break;
        case GICD_TYPE_ICFn:
            break;

        case GICD_TYPE_TYPER:
        case GICD_TYPE_IIDR:
            dlog_error("Register(%s) could not be written\n", vgic_get_irq_name(type));
            break;

        default:
            dlog_error("Invalid gicd register (0x%x) to write\n", offset);
            break;
    }

    dlog_error("gic to write register: '%s : 0x%x': 0x%x\n", vgic_get_irq_name(type), offset, *v);
    switch (size) {
        case 0:
            writeb_gicd(*v & 0xff, offset);
            break;
        case 1:
            writew_gicd(*v & 0xffff, offset);
            break;
        case 2:
            writel_gicd(*v & 0xffffffff, offset);
            break;
        case 3:
            writeq_gicd(*v, offset);
            break;
        default:
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

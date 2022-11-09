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
#include "gicv3.h"

#define SIZE_64K    (1 << 16)

struct vgic_gicd {
	uint32_t gicd_ctlr;
	uint32_t gicd_typer;
	uint32_t gicd_pidr2;
	struct spinlock gicd_lock;
	unsigned long base;
	unsigned long end;
};

struct vgic_gicr {
	uint32_t gicr_ctlr;
	uint32_t gicr_pidr2;
	uint64_t gicr_typer;
	uint32_t gicr_ispender;
	uint32_t gicr_enabler0;
	uint32_t vcpu_id;
	unsigned long rd_base;
	unsigned long sgi_base;
	unsigned long vlpi_base;
	struct list_entry list;
	struct spinlock gicr_lock;
};

struct vgicv3_dev {
	struct vdev vdev;
	struct vgic_gicd gicd;
	struct vgic_gicr *gicr[MAX_CPUS];
};

#define GIC_TYPE_GICD		(0x0)
#define GIC_TYPE_GICR_RD	(0x1)
#define GIC_TYPE_GICR_SGI	(0x2)
#define GIC_TYPE_GICR_VLPI	(0x3)
#define GIC_TYPE_INVAILD	(0xff)

#define vdev_to_vgic(vdev) \
	(struct vgicv3_dev *)CONTAINER_OF(vdev, struct vgicv3_dev, vdev)

struct vgicv3_info {
	unsigned long gicd_base;
	unsigned long gicd_size;
	unsigned long gicr_base;
	unsigned long gicr_size;
	unsigned long gicc_base;
	unsigned long gicc_size;
	unsigned long gich_base;
	unsigned long gich_size;
	unsigned long gicv_base;
	unsigned long gicv_size;
};

#define VGICV3_IDX_GICD 0x0
#define VGICV3_IDX_GICR 0x1
#define VGICV3_IDX_GICC 0x2
#define VGICV3_IDX_GICH 0x3
#define VGICV3_IDX_GICV 0x4

struct vgic_set {
    struct vgicv3_dev devices[MAX_VMS];
    bool flag[MAX_VMS];

    struct spinlock lock;
};

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *des, const char *src);

static struct vgic_set vgic_mgm = {0};

static struct vgic_set *get_vgic_set(void)
{
    return &vgic_mgm;
}

void vgic_set_init(struct vgic_set *set)
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

static struct vgicv3_dev *request_vgic_device(struct vgic_set *set)
{
    struct vgicv3_dev *device = NULL;

    if (set != NULL) {
        uint32_t index = 0;

        sl_lock(&set->lock);
        while (set->flag[index]) index++;
        device = (index < MAX_VMS) ? &set->devices[index] : NULL;
        sl_unlock(&set->lock);
    }

    return device;
}

static void release_vgic_device(struct vgic_set *set, struct vgicv3_dev *device)
{
    if (set != NULL) {
        int32_t index = -1;

        sl_lock(&set->lock);
        index = (device - set->devices) / sizeof(struct vgicv3_dev);
        if (index >= 0 && index < MAX_VMS && set->flag[index]) {
            set->flag[index] = false;
            memset(device, 0x0, sizeof(struct vgicv3_dev));
        }
        sl_unlock(&set->lock);
    }
}

static int offset_to_gicr_type(struct vgic_gicr *gicr, unsigned long *poffset)
{
	unsigned long offset = *poffset;

	if ((offset >= gicr->rd_base) &&
		(offset < (gicr->rd_base + SIZE_64K))) {
		*poffset = offset - gicr->rd_base;
		return GIC_TYPE_GICR_RD;
	}

	if ((offset >= gicr->sgi_base) &&
		(offset < (gicr->sgi_base + SIZE_64K))) {
		*poffset = offset - gicr->sgi_base;
		return GIC_TYPE_GICR_SGI;
	}
#if 0
	if ((address >= gicr->vlpi_base) &&
		(address < (gicr->vlpi_base + SIZE_64K))) {

		*offset = address - gicr->vlpi_base;
		return GIC_TYPE_GICR_VLPI;
	}
#endif
	return GIC_TYPE_INVAILD;
}

static int vgic_gicd_mmio_read(struct vcpu *vcpu,
			struct vgic_gicd *gicd,
			unsigned long offset,
			unsigned long *v)
{
	uint32_t *value = (uint32_t *)v;

	switch (offset) {
		case GICD_CTLR:
			*value = gicd->gicd_ctlr & ~(1 << 31);
			break;
		case GICD_TYPER:
			*value = gicd->gicd_typer;
			break;
		case GICD_STATUSR:
			*value = 0;
			break;
		case GICD_ISENABLER:
			*value = 0;
			break;
		case GICD_ICENABLER:
			*value = 0;
			break;
		case GICD_PIDR2:
			*value = gicd->gicd_pidr2;
			break;
		case GICD_ICFGR:
			*value = 0;
			break;
		default:
			*value = 0;
			break;
	}

	return 0;
}

static int vgic_gicd_mmio_write(struct vcpu *vcpu,
			struct vgic_gicd *gicd,
			unsigned long offset,
			unsigned long *value)
{
	sl_lock(&gicd->gicd_lock);

	switch (offset) {
	case GICD_CTLR:
		gicd->gicd_ctlr = *value;
		break;
	case GICD_TYPER:
		break;
	case GICD_STATUSR:
		break;
	case GICD_ISENABLER:
		break;
	case GICD_ICENABLER:
		break;
	case GICD_IPRIORITYR:
		break;
	case GICD_ICFGR:
		break;

	default:
		break;
	}

	sl_unlock(&gicd->gicd_lock);
	return 0;
}

static int vgic_gicd_mmio(struct vcpu *vcpu, struct vgic_gicd *gicd,
		int read, unsigned long offset, unsigned long *value)
{
	if (read)
		return vgic_gicd_mmio_read(vcpu, gicd, offset, value);
	else
		return vgic_gicd_mmio_write(vcpu, gicd, offset, value);
}

static int vgic_gicr_rd_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *value)
{
	if (read) {
		switch (offset) {
		case GICR_PIDR2:
			*value = gicr->gicr_pidr2;
			break;
		case GICR_TYPER:
			*value = gicr->gicr_typer;
			break;
		case GICR_TYPER_HIGH:
			*value = gicr->gicr_typer >> 32;	/* for aarch32 assume 32bit read */
			break;
		default:
			*value = 0;
			break;
		}
	} else {

	}

	return 0;
}

static int vgic_gicr_sgi_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *value)
{
	if (read) {
		switch (offset) {
		case GICR_CTLR:
			break;
		case GICR_ISPENDR0:
			break;
		case GICR_PIDR2:
			break;
		case GICR_ISENABLER:
		case GICR_ICENABLER:
			break;
		default:
			break;
		}
	} else {
		switch (offset) {
		case GICR_ICPENDR0:
			break;
		case GICR_ISENABLER:
			break;
		case GICR_ICENABLER:
			break;
		}
	}

	return 0;
}

static int vgic_gicr_vlpi_mmio(struct vcpu *vcpu, struct vgic_gicr *gicr,
		int read, unsigned long offset, unsigned long *v)
{
	return 0;
}

static int vgic_check_gicr_access(struct vcpu *vcpu, struct vgic_gicr *gicr,
			int type, unsigned long offset)
{
    if (type == GIC_TYPE_GICR_RD) {
        switch (offset) {
            case GICR_TYPER:
            case GICR_PIDR2:
            case GICR_TYPER_HIGH:		// for aarch32 gicv3
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }

	return 1;
}

static struct vgic_gicd *vdev_get_gicd(struct vdev *vdev)
{
	struct vgicv3_dev *vgic_dev = vdev_to_vgic(vdev);

	return (vgic_dev != NULL) ? &vgic_dev->gicd : NULL;
}

static struct vgic_gicr *vdev_get_gicr(struct vdev *vdev)
{
	struct vgicv3_dev *vgic_dev = vdev_to_vgic(vdev);

	/* TODO: gicr device mapping */
	return (vgic_dev != NULL) ? vgic_dev->gicr[0] : NULL;
}

static int vgic_mmio_handler(struct vdev *vdev, struct vcpu *vcpu,
		int read, int idx, unsigned long offset,
		unsigned long *value)
{
	int type = GIC_TYPE_INVAILD;
	struct vgic_gicd *gicd = vdev_get_gicd(vdev);
	struct vgic_gicr *gicr = vdev_get_gicr(vdev);

	if (idx == VGICV3_IDX_GICD) {
		type = GIC_TYPE_GICD;
	} else if (idx == VGICV3_IDX_GICR) {
		type = offset_to_gicr_type(gicr, &offset);
		if (type != GIC_TYPE_INVAILD)
			goto out;

		/* master vcpu may access other vcpu's gicr */
	} else {
		dlog_error("only support GICD and GICR emulation\n");
		return -EINVAL;
	}
out:
	if (type == GIC_TYPE_INVAILD) {
		dlog_error("invaild gicr type and address\n");
		return -EINVAL;
	}

	if (type != GIC_TYPE_GICD) {
		if (!vgic_check_gicr_access(vcpu, gicr, type, offset))
			return -EACCES;
	}

	switch (type) {
	case GIC_TYPE_GICD:
		return vgic_gicd_mmio(vcpu, gicd, read, offset, value);
	case GIC_TYPE_GICR_RD:
		return vgic_gicr_rd_mmio(vcpu, gicr, read, offset, value);
	case GIC_TYPE_GICR_SGI:
		return vgic_gicr_sgi_mmio(vcpu, gicr, read, offset, value);
	case GIC_TYPE_GICR_VLPI:
		return vgic_gicr_vlpi_mmio(vcpu, gicr, read, offset, value);
	default:
		dlog_error("unsupport gic type %d\n", type);
		return -EINVAL;
	}

	return 0;
}

static int vgic_mmio_read(struct vdev *vdev, struct vcpu *vcpu,
		uint64_t address, uint64_t *read_value)
{
	return vgic_mmio_handler(vdev, vcpu, 1, 0, address, read_value);
}

static int vgic_mmio_write(struct vdev *vdev, struct vcpu *vcpu,
		uint64_t address, uint64_t *write_value)
{
	return vgic_mmio_handler(vdev, vcpu, 0, 0, address, write_value);
}

#if 0
static void vgic_gicd_init(struct vm *vm, struct vgic_gicd *gicd,
		unsigned long base, size_t size)
{
	uint32_t typer = 0;
	int nr_spi;

	/*
	 * when a vm is created need to create
	 * one vgic for each vm since gicr is percpu
	 * but gicd is shared so created it here
	 */
	memset(gicd, 0, sizeof(*gicd));

	gicd->base = base;
	gicd->end = base + size;

	spin_lock_init(&gicd->gicd_lock);

	gicd->gicd_ctlr = 0;

	/* GICV3 and provide vm->virq_nr interrupt */
	gicd->gicd_pidr2 = (0x3 << 4);

	typer |= vm->vcpu_nr << 5;
	typer |= 9 << 19;
	nr_spi = ((vm->vspi_nr + 32) >> 5) - 1;
	typer |= nr_spi;
	gicd->gicd_typer = typer;
}

static void vgic_gicr_init(struct vcpu *vcpu,
		struct vgic_gicr *gicr, unsigned long base)
{
	gicr->vcpu_id = get_vcpu_id(vcpu);

	/*
	 * now for gicv3 TBD, do not support vlpi.
	 */
	base = (128 * 1024) * vcpu->vcpu_id;
	gicr->rd_base = base;
	gicr->sgi_base = base + (64 * 1024);
	gicr->vlpi_base = 0;

	gicr->gicr_ctlr = 0;
	gicr->gicr_ispender = 0;
	spin_lock_init(&gicr->gicr_lock);

	/* TBD */
	gicr->gicr_pidr2 = 0x3 << 4;

	/*
	 * Linux will use the Last bit (bit 4) to detect whether
	 * this gicr is the last GICR.
	 */
	gicr->gicr_typer = 0 | ((unsigned long)vcpu->vcpu_id << 32);
	if (vcpu->vcpu_id == (vcpu->vm->vcpu_nr - 1))
		gicr->gicr_typer |= (1 << 4);
}
#endif

static void vm_release_gic(struct vgicv3_dev *gic)
{
	if (!gic)
		return;
}

static void vgic_deinit(struct vdev *vdev)
{
	struct vgicv3_dev *dev = vdev_to_vgic(vdev);

	vm_release_gic(dev);
}

static void vgic_reset(struct vdev *vdev)
{
	dlog_info("vgic device reset\n");
}

int vgicv3_init(struct vm *vm, struct mpool *ppool)
{
	int ret = 0;
	struct vdev *vdev = NULL;
	struct vgic_set *set = NULL;
	struct vgicv3_dev *vgic_dev = NULL;
	struct gicv3_device *gic_dev = NULL;

	dlog_info("vgicv3: create vdev for vm\n");

	CHECK(set = get_vgic_set());
	CHECK(gic_dev = get_gicv3_device());
	CHECK(vgic_dev = request_vgic_device(set));

	vdev = &vgic_dev->vdev;
	ret = vdev_init(vdev, gic_dev->gic_base, gic_dev->gic_size);
	if (ret) {
		dlog_error("Virtual device initialization failed");
		goto init_failed;
	}

	vdev_set_name(vdev, "vgicv3");
	vdev->read = vgic_mmio_read;
	vdev->write = vgic_mmio_write;
	vdev->deinit = vgic_deinit;
	vdev->reset = vgic_reset;

	ret = register_one_vdev(vm, vdev, ppool);
	if (ret) {
		dlog_error("Registe virtual device '%s' failed", vdev->name);
		goto registe_failed;
	}

	dlog_info("vgicv3: create vdev for vm done\n");
out:
	return ret;

registe_failed:
	vdev_deinit(vdev);
init_failed:
	release_vgic_device(set, vgic_dev);

	goto out;
}

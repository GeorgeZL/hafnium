/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

#define VDEV_NAME_SIZE  25

#include <hf/std.h>
#include <hf/list.h>
#include <hf/errno.h>
#include <hf/mpool.h>
#include <hf/arch/types.h>
#include <hf/device/gic.h>
#include <hf/device/vgic.h>

struct vdev {
    char name[VDEV_NAME_SIZE];
    struct vm *vm;
    void *iomem;
    uint32_t mem_size;
    uint64_t gvm_paddr;
    uint64_t hvm_paddr;
    
    struct list_entry   list;
    int (*read)(struct vdev *, struct vcpu *, MMIOInfo_t *mmio, uint64_t, uint8_t size, uint32_t *);
    int (*write)(struct vdev *, struct vcpu *, MMIOInfo_t *mmio, uint64_t, uint8_t size, uint32_t *);
    void (*deinit)(struct vdev *);
    void (*reset)(struct vdev *);
    int (*suspend)(struct vdev *);
    int (*resume)(struct vdev *);
};

void vdev_set_name(struct vdev *vdev, char *name);
void vdev_deinit(struct vdev *vdev);
void virtual_device_init(struct vm *vm, struct mpool *ppool);
int vdev_init(struct vdev *vdev, uint64_t base, uint32_t size);
bool register_one_vdev(struct vm *vm, struct vdev *vdev, struct mpool *);
int vdev_mmio_emulation(struct vcpu *vcpu, MMIOInfo_t *mmio, int write, uint8_t size, uint64_t addr, uint32_t *value);

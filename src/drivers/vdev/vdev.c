#include <hf/device/vdev.h>
#include <hf/vm.h>
#include <hf/std.h>
#include <hf/dlog.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *des, const char *src);

void vdev_set_name(struct vdev *vdev, char *name)
{
    int len;

    if (!vdev || !name)
        return;

    len = strnlen_s(name, 100);
    if (len > VDEV_NAME_SIZE)
        len = VDEV_NAME_SIZE;

    strcpy(vdev->name, name);
}

void vdev_deinit(struct vdev *vdev)
{
    if (!vdev || !vdev->vm) {
        return;
    }

    list_remove(&vdev->list);
    memset(vdev, 0, sizeof(struct vdev));
}

int vdev_init(struct vdev *vdev, uint64_t base, uint32_t size)
{
    if (!vdev)
        return -EINVAL;

    memset(vdev, 0, sizeof(struct vdev));
    vdev->gvm_paddr = base;
    vdev->mem_size = size;
    vdev->vm = NULL;
    list_init(&vdev->list);

    return 0;
}

bool register_one_vdev(struct vm *vm, struct vdev *vdev, struct mpool *ppool)
{
    bool success = false;

    if (!vm || !vdev)
        return success;

    list_add_tail(&vm->vdev_list, &vdev->list);

    dlog_error("Trying to unmap S2 of vm-%d: 0x%08x - 0x%08x\n", \
	    vm->id, vdev->gvm_paddr, vdev->gvm_paddr + vdev->mem_size);

	mm_vm_dump(&vm->ptable);

#if 1
    success = mm_vm_unmap(&vm->ptable, (paddr_t){vdev->gvm_paddr},
        (paddr_t){vdev->gvm_paddr + vdev->mem_size}, ppool);
    if (!success) {
        dlog_error("vdev - '%s': failed to register to VM\n", vdev->name);
    }
#else
    success = true;
#endif

	mm_vm_dump(&vm->ptable);

    return success;
}

int vdev_mmio_emulation(
    struct vcpu *vcpu, MMIOInfo_t *mmio, int write, uint8_t size, uint64_t addr, uint32_t *value)
{
    struct vm *vm = current_vm();
    struct vdev *vdev;
    int ret = -EINVAL;

    list_for_each_entry(vdev, &vm->vdev_list, list) {
        if ((addr >= vdev->gvm_paddr) &&  \
            (addr < vdev->gvm_paddr + vdev->mem_size)) {
            if (write) {
                return vdev->write(vdev, vcpu, mmio, addr, size, value);
            } else {
                return vdev->read(vdev, vcpu, mmio, addr, size, value);
            }
        }
    }

    //TODO: inject data abort into vm
    return ret;
}

void virtual_device_init(struct vm *vm, struct mpool *ppool)
{
	if (!vm || !ppool) {
		dlog_error("Could not to init virtual devices\n");
		return;
	}

	/* vgic device init */
	vgic_init(vm, ppool);
}

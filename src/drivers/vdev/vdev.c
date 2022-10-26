
void vdev_set_name(struct vdev *vdev, char *name)
{
    int len;

    if (!vdev || !name)
        return;

    len = strlen(name);
    if (len > VDEV_NAME_SIZE)
        len = VDEV_NAME_SIZE;

    strncpy(vdev->name, name, len);
}

void vdev_deinit(struct vdev *vdev)
{
    struct vm *vm = NULL;

    if (!vdev || !vdev->vm) {
        return;
    }

    list_del(&vdev->list);
    memset(vdev, 0, sizeof(struct vdev));
}

int vdev_init(struct vm *vm, struct vdev *vdev, uint64_t base, uint32_t size)
{
    if (!vm || !vdev)
        return -EINVAL;

    memset(vdev, 0, sizeof(struct vdev));
    vdev->gvm_paddr = base;
    vdev->mem_size = size;
    vdev->vm = vm;
    list_init(&vdev->list);
    list_add_tail(&vm->vdev_list, &vdev->list);

    return 0;
}

int vdev_mmio_emulation(
    struct regs *regs, int write, uint64_t addr, uint64_t *value)
{
    struct vm *vm = get_current_vm();
    struct vdev *vdev;

    list_for_each_entry(vdev, &vm->vdev_list, list) {
        if ((addr >= vdev->gvm_paddr) &&  \
            (addr < vdev->gvm_paddr + vdev->mem_size)) {
            if (write) {
                return vdev->write(vdev, regs, addr, value);
            } else {
                return vdev->read(vdev, regs, addr, value);
            }
        }
    }

    //TODO: inject data abort into vm
}

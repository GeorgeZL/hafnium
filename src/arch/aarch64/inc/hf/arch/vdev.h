#define VDEV_NAME_SIZE  25

struct vdev {
    char name[VDEV_NAME_SIZE];
    struct vm *vm;
    void *iomem;
    uint32_t mem_size;
    uint64_t gvm_paddr;
    uint64_t hvm_paddr;
    
    struct list_entry   list;
    int (*read)(struct vdev *, struct regs *, uint64_t, uint64_t);
    void (*write)(struct vdev *, struct regs *, uint64_t, uint64_t *);
    void (*deinit)(struct vdev *);
    void (*reset)(struct vdev *);
    int (*suspend)(struct vdev *);
    int (*resume)(struct vdev *);
};

struct vdev *create_vdev(struct vm *vm, uint64_t base, uint32_t size);
void release_vdev(struct vdev *vdev);
int iomem_vdev_init(struct vm*vm, struct vdev *dev, uint32_t size);
int vdev_mmio_emulation(struct regs *regs, int write, uint64_t address, uint64_t *value);
void vdev_set_name(struct vdev *vdev, char *name);


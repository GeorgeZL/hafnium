#include <hf/arch/arch.h>
#include <hf/io.h>
#include <hf/static_assert.h>
#include <hf/std.h>
#include <hf/errno.h>
#include <hf/dlog.h>
#include <hf/mm.h>
#include <hf/spinlock.h>
#include <hf/bitops.h>
#include <hf/interrupt.h>

#define MAX_SPI_NUM 1024

static ffa_vm_id_t spi_maps[MAX_SPI_NUM] = {0};
struct spinlock spi_lock;

void spi_map_init(void)
{
    sl_init(&spi_lock);
    for (int i = 16; i < MAX_SPI_NUM; i++) {
        //spi_maps[i] = HF_INVALID_VM_ID;
        spi_maps[i] = HF_PRIMARY_VM_ID;
    }
    
    for (int i = 0; i < 16; i++) {
        spi_maps[i] = 0xff;
    }
}

void vm_hwirq_add(uint32_t hwirq, ffa_vm_id_t vmid)
{
    sl_lock(&spi_lock);

    if (hwirq >= 16 && hwirq < 1024) {
        if (spi_maps[hwirq] == HF_INVALID_VM_ID) {
            spi_maps[hwirq] = vmid;
        } else {
            dlog_error("hwirq(0x%x) has been allocated.\n", hwirq);
        }
    } else {
        dlog_error("Invalid hwirq(0x%x), only spi is supported\n", hwirq);
    }

    sl_unlock(&spi_lock);
}

bool hwirq_is_belongs_current(uint32_t hwirq)
{
    struct vm *current = current_vm();
    bool valid = false;

    sl_lock(&spi_lock);

    if ((hwirq >= 16 && hwirq < 1024) && \
        (current && (current->id == spi_maps[hwirq]))) { // ppi & spi
        valid = true;
    } else if (hwirq < 16) {  // ipi
        valid = true;
    }

    sl_unlock(&spi_lock);

    return valid;
}

void vm_hwirq_remove(uint32_t hwirq)
{
    sl_lock(&spi_lock);

    if (hwirq >= 16 && hwirq > 1024) { // ppi & spi
        spi_maps[hwirq] = HF_INVALID_VM_ID;
    }

    sl_unlock(&spi_lock);
}

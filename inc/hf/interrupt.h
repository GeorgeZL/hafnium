#pragma once

#include <hf/vm.h>

void spi_map_init(void);
void vm_hwirq_add(uint32_t hwirq, ffa_vm_id_t vmid);
bool hwirq_is_belongs_current(uint32_t hwirq);
void vm_hwirq_remove(uint32_t hwirq);

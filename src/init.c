/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/init.h"

#include <stdalign.h>
#include <stddef.h>

#include "hf/arch/other_world.h"
#include "hf/arch/plat/ffa.h"

#include "hf/api.h"
#include "hf/boot_flow.h"
#include "hf/boot_params.h"
#include "hf/cpio.h"
#include "hf/cpu.h"
#include "hf/dlog.h"
#include "hf/fdt_handler.h"
#include "hf/load.h"
#include "hf/mm.h"
#include "hf/mpool.h"
#include "hf/panic.h"
#include "hf/plat/boot_flow.h"
#include "hf/plat/console.h"
#include "hf/plat/interrupts.h"
#include "hf/plat/iommu.h"
#include "hf/std.h"
#include "hf/vm.h"

#include "vmapi/hf/call.h"

alignas(MM_PPOOL_ENTRY_SIZE) char ptable_buf[MM_PPOOL_ENTRY_SIZE * HEAP_PAGES];

static struct mpool ppool;

/*
 * Hafnium manifest declaring all VMs. Notice this structure is large hence kept
 * in BSS rather than directly allocated to the stack in one_time_init.
 */
static struct manifest manifest;

/**
 * Performs one-time initialisation of memory management for the hypervisor.
 *
 * This is the only C code entry point called with MMU and caching disabled. The
 * page table returned is used to set up the MMU and caches for all subsequent
 * code.
 */
void one_time_init_mm(void)
{
	/* Make sure the console is initialised before calling dlog. */
	plat_console_init();

	plat_ffa_log_init();

	mpool_init(&ppool, MM_PPOOL_ENTRY_SIZE);
	mpool_add_chunk(&ppool, ptable_buf, sizeof(ptable_buf));

	if (!mm_init(&ppool)) {
		panic("mm_init failed");
	}
}

static void dump_cpu_info(struct boot_params *p)
{
    dlog_info("CPU IDS: \n");
    for (int i = 0; i < MAX_CPUS && i < p->cpu_count; i++) {
        dlog_info("\tcpu-%d: 0x%08x\n", i, p->cpu_ids[i]);
    }
}

static void dump_mem_info(struct boot_params *p)
{
    dlog_info("Memory infos: \n");
    for (int i = 0; i < MAX_MEM_RANGES && i < p->mem_ranges_count; i++) {
        dlog_info("\tmemory block %d: 0x%08x - 0x%08x\n", 
            i, p->mem_ranges[i].begin.pa, p->mem_ranges[i].end.pa);
    }
}

static void dump_device_mem_info(struct boot_params *p)
{
    dlog_info("Device memory infos: \n");
    for (int i = 0; i < MAX_DEVICE_MEM_RANGES && i < p->device_mem_ranges_count; i++) {
        dlog_info("\tmemory block %d: 0x%08x - 0x%08x\n", 
            i, p->device_mem_ranges[i].begin.pa, p->device_mem_ranges[i].end.pa);
    }
}

static void boot_param_dumps(struct boot_params *p)
{
    dlog_info("=============Boot information: ==============\n");
    dump_cpu_info(p);
    dump_mem_info(p);
    dump_device_mem_info(p);
    dlog_info("initrd info: 0x%08x - 0x%08x\n", p->initrd_begin.pa, p->initrd_end.pa);
    dlog_info("dts address: 0x%08x\n", p->kernel_arg);
}


/**
 * Performs one-time initialisation of the hypervisor.
 */
void one_time_init(void)
{
	struct string manifest_fname = STRING_INIT("manifest.dtb");
	struct fdt fdt;
	enum manifest_return_code manifest_ret;
	struct boot_params params;
	struct boot_params_update update;
	struct memiter cpio;
	struct memiter manifest_it;
	void *initrd;
	size_t i;
	struct mm_stage1_locked mm_stage1_locked;

	arch_one_time_init();

	/* Enable locks now that mm is initialised. */
	dlog_enable_lock();
	mpool_enable_locks();

	mm_stage1_locked = mm_lock_stage1();

    mm_vm_dump(mm_stage1_locked.ptable);

	if (!fdt_map(&fdt, mm_stage1_locked, plat_boot_flow_get_fdt_addr(),
		     &ppool)) {
		panic("Unable to map FDT.");
	}

	if (!boot_flow_get_params(&params, &fdt)) {
		panic("Could not parse boot params.");
	}

    boot_param_dumps(&params);
	for (i = 0; i < params.mem_ranges_count; ++i) {
		dlog_info("Memory range:  %#x - %#x\n",
			  pa_addr(params.mem_ranges[i].begin),
			  pa_addr(params.mem_ranges[i].end) - 1);
	}

	/*
	 * Hafnium manifest is either gathered from the ramdisk or passed
	 * directly to Hafnium entry point by the earlier bootloader stage.
	 * If the ramdisk start address is non-zero it hints the manifest
	 * shall be looked up from the ramdisk. If zero, assume the address
	 * passed to Hafnium entry point is the manifest address.
	 */
	if (pa_addr(params.initrd_begin)) {
		dlog_info("Ramdisk range: %#x - %#x\n",
			  pa_addr(params.initrd_begin),
			  pa_addr(params.initrd_end) - 1);

		/* Map initrd in, and initialise cpio parser. */
		initrd = mm_identity_map(mm_stage1_locked, params.initrd_begin,
					 params.initrd_end, MM_MODE_R, &ppool);
		if (!initrd) {
			panic("Unable to map initrd.");
		}

		memiter_init(
			&cpio, initrd,
			pa_difference(params.initrd_begin, params.initrd_end));

		if (!cpio_get_file(&cpio, &manifest_fname, &manifest_it)) {
			panic("Could not find manifest in initrd.");
		}
	} else {
		manifest_it = fdt.buf;
	}

	manifest_ret = manifest_init(mm_stage1_locked, &manifest, &manifest_it,
				     &ppool);

	if (manifest_ret != MANIFEST_SUCCESS) {
		panic("Could not parse manifest: %s.",
		      manifest_strerror(manifest_ret));
	}

	if (!plat_iommu_init(&fdt, mm_stage1_locked, &ppool)) {
		panic("Could not initialize IOMMUs.");
	}

#if 0
	if (!fdt_unmap(&fdt, mm_stage1_locked, &ppool)) {
		panic("Unable to unmap FDT.");
	}
#endif

	cpu_module_init(params.cpu_ids, params.cpu_count);

	if (!plat_interrupts_controller_driver_init(&fdt, mm_stage1_locked,
						    &ppool)) {
		panic("Could not initialize Interrupt Controller driver.");
	}

	/* Load all VMs. */
	update.reserved_ranges_count = 0;
	if (!load_vms(mm_stage1_locked, &manifest, &cpio, &params, &update,
		      &ppool)) {
		panic("Unable to load VMs.");
	}

	if (!boot_flow_update(mm_stage1_locked, &manifest, &update, &cpio,
			      &ppool)) {
		panic("Unable to update boot flow.");
	}

	mm_defrag(mm_stage1_locked, &ppool);

    mm_vm_dump(mm_stage1_locked.ptable);

	mm_unlock_stage1(&mm_stage1_locked);

	/* Initialise the API page pool. ppool will be empty from now on. */
	api_init(&ppool);

	/* Enable TLB invalidation for VM page table updates. */
	mm_vm_enable_invalidation();

	/* Perform platform specfic FF-A initialization. */
	plat_ffa_init(manifest.ffa_tee_enabled);

	dlog_info("Hafnium initialisation completed\n");
}

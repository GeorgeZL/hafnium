/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "hf/arch/plat/ffa.h"

#include "hf/cpu.h"
#include "hf/dlog.h"
#include "hf/vm.h"

#include "vmapi/hf/ffa.h"

/**
 * The entry point of CPUs when they are turned on. It is supposed to initialise
 * all state and return the first vCPU to run.
 */
struct vcpu *cpu_main(struct cpu *c)
{
	//struct vm *first_boot;
    struct vm *vm;
	struct vcpu *vcpu;
    ffa_vm_id_t vmid;
    static bool debug = false;

	/*
	 * This returns the PVM in the normal world and the first
	 * booted Secure Partition in the secure world.
	 */
	//first_boot = vm_get_first_boot();
    vmid = cpu_get_vmid(c);
    vm = vm_find(vmid);

    vcpu = vm_get_vcpu_with_cpu(vm, c);

	//vcpu = vm_get_vcpu(first_boot, cpu_index(c));

	//vcpu->cpu = c;

	/* Reset the registers to give a clean start for vCPU. */
	vcpu_reset(vcpu);

	/* Set the designated GP with the physical core index. */
	vcpu_set_phys_core_idx(vcpu);

	/* Initialize SRI for running core. */
	plat_ffa_sri_init(c);

	vm_set_boot_info_gp_reg(vm, vcpu);

    if (debug == false) {
        debug = true;

        dlog_warning("Primary VM context:\n");
        vcpu_dump_sysarch(vcpu);
    }

	return vcpu;
}

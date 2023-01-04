#!/bin/bash

FVP_ARGS=
smmu_args=
cpu_args=
gic_args=
uart_args=
imgs_args=
net_args=
GIC2=1

if [ $GIC2 -ne 1 ]; then
	DTB=images/dtb/fvp-base-gicv3-psci-1t.dtb
	BL31=/opt/atf/build/fvp/release/bl31.bin
else
	DTB=images/dtb/fvp-base-gicv2-psci.dtb
	BL31=/opt/atf/build/fvp/debug/bl31.bin
fi
INITRD=images/initrd.img
KERNEL=out/reference/aem_v8a_fvp_clang/hafnium.bin

function device_args()
{
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_AIDR=2"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_IDR0=0x0046123B"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_IDR1=0x00600002"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_IDR3=0x1714"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_IDR5=0xFFFF0472"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_S_IDR1=0xA0000002"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_S_IDR2=0"
	smmu_args=$smmu_args" -C pci.pci_smmuv3.mmu.SMMU_S_IDR3=0"

	cpu_args=$cpu_args" -C cluster0.NUM_CORES=4"
	cpu_args=$cpu_args" -C cluster1.NUM_CORES=4"
	cpu_args=$cpu_args" -C cache_state_modelled=0"
	cpu_args=$cpu_args" -C cluster0.cpu0.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster0.cpu1.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster0.cpu2.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster0.cpu3.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster1.cpu0.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster1.cpu1.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster1.cpu2.RVBAR=0x04020000"
	cpu_args=$cpu_args" -C cluster1.cpu3.RVBAR=0x04020000"

	uart_args=$uart_args" -C bp.terminal_0.start_telnet=false"
	uart_args=$uart_args" -C bp.terminal_1.start_telnet=false"
	uart_args=$uart_args" -C bp.terminal_2.start_telnet=false"
	uart_args=$uart_args" -C bp.terminal_3.start_telnet=false"
	uart_args=$uart_args" -C bp.pl011_uart0.untimed_fifos=1"
	uart_args=$uart_args" -C bp.pl011_uart0.unbuffered_output=1"

	net_args=$net_args" -C bp.smsc_91c111.enabled=true"
	net_args=$net_args" -C bp.hostbridge.userNetworking=true"

	gic_args="-C gicv3.gicv2-only=$GIC2"
}

function images_args()
{
	imgs_args=$imgs_args" --data cluster0.cpu0=$DTB@0x82000000"
	imgs_args=$imgs_args" --data cluster0.cpu0=$INITRD@0x84000000"
	imgs_args=$imgs_args" --data cluster0.cpu0=$KERNEL@0x80000000"
	imgs_args=$imgs_args" --data cluster0.cpu0=$BL31@0x04020000"
}

function fvp_args()
{
	device_args
	images_args

	FVP_ARGS="$smmu_args $cpu_args $uart_args $net_args $imgs_args $gic_args"
	FVP_ARGS=$FVP_ARGS" -C pctl.startup=0.0.0.0"
	FVP_ARGS=$FVP_ARGS" -C bp.vis.disable_visualisation=true"
	FVP_ARGS=$FVP_ARGS" -C bp.vis.rate_limit-enable=false"
	FVP_ARGS=$FVP_ARGS" --iris-port 7100 -p "
}

fvp_args

echo "$FVP_ARGS"
FVP_Base_RevC-2xAEMvA  \
	$FVP_ARGS $@

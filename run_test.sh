#/home/workspace2/base/hypervisor/fvp/Base_RevC_AEMvA_pkg/models/Linux64_GCC-9.3/FVP_Base_RevC-2xAEMvA  \
FVP_Base_RevC-2xAEMvA  \
-C pci.pci_smmuv3.mmu.SMMU_AIDR=2  \
-C pci.pci_smmuv3.mmu.SMMU_IDR0=0x0046123B  \
-C pci.pci_smmuv3.mmu.SMMU_IDR1=0x00600002  \
-C pci.pci_smmuv3.mmu.SMMU_IDR3=0x1714  \
-C pci.pci_smmuv3.mmu.SMMU_IDR5=0xFFFF0472  \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR1=0xA0000002  \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR2=0  \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR3=0  \
-C pctl.startup=0.0.0.0  \
-C cluster0.NUM_CORES=4  \
-C cluster1.NUM_CORES=4  \
-C cache_state_modelled=0  \
-C bp.vis.disable_visualisation=true  \
-C bp.vis.rate_limit-enable=false  \
-C bp.terminal_0.start_telnet=true  \
-C bp.terminal_1.start_telnet=true  \
-C bp.terminal_2.start_telnet=false  \
-C bp.terminal_3.start_telnet=false  \
-C bp.pl011_uart0.untimed_fifos=1  \
-C bp.pl011_uart0.unbuffered_output=1  \
-C cluster0.cpu0.RVBAR=0x04020000  \
-C cluster0.cpu1.RVBAR=0x04020000  \
-C cluster0.cpu2.RVBAR=0x04020000 \
-C cluster0.cpu3.RVBAR=0x04020000 \
-C cluster1.cpu0.RVBAR=0x04020000 \
-C cluster1.cpu1.RVBAR=0x04020000 \
-C cluster1.cpu2.RVBAR=0x04020000 \
-C cluster1.cpu3.RVBAR=0x04020000 \
--data cluster0.cpu0=images/dtb/fvp-base-gicv3-psci-1t.dtb@0x82000000 \
--data cluster0.cpu0=images/initrd.img@0x84000000 \
--data cluster0.cpu0=out/reference/aem_v8a_fvp_clang/hafnium.bin@0x80000000 \
--data cluster0.cpu0=./prebuilts/linux-aarch64/trusted-firmware-a-trusty/fvp/bl31.bin@0x04020000 \
--iris-port 7100 -p $@ \


#--data cluster0.cpu0=/home/fvp-base-gicv3-psci-1t.dtb@0x82000000 \
#--data cluster0.cpu0=/home/packages/tools/atf/build/fvp/debug/bl31.bin@0x04020000 -p $@
#--data cluster0.cpu0=out/reference/aem_v8a_fvp_clang/hafnium.bin@0x80000000 \
#--data cluster0.cpu0=images/dtb/fvp-base-gicv3-psci-1t.dtb@0x82000000 \
#--data cluster0.cpu0=images/initrd.img@0x84000000 \

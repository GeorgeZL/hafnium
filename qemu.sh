#!/bin/bash

GICV2=1

if [ $GICV2 -eq 1 ]; then
    GIC_VERSION=2
    KERN_IMG=out/reference/qemu_aarch64_gicv2_clang/hafnium.bin
else
    GIC_VERSION=3
    KERN_IMG=out/reference/qemu_aarch64_clang/hafnium.bin
fi

function qemu_virt()
{
    qemu-system-aarch64 \
        -M virt,gic_version=$GIC_VERSION \
        -m 4G   \
        -smp 8  \
        -cpu cortex-a57 -nographic \
        -machine virtualization=true \
        -kernel $KERN_IMG \
        -monitor telnet:localhost:5005,server,nowait \
        -serial chardev:s0 -chardev stdio,id=s0,mux=on,signal=off \
        -serial telnet:localhost:5001,server,nowait \
        -serial telnet:localhost:5002,server,nowait \
        -serial telnet:localhost:5003,server,nowait \
        -initrd ./images/initrd.img -append "rdinit=init console=ttyAMA0 earlycon=pl011,mmio32,0x9000000" $@
}

qemu_virt $@

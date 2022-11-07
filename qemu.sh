qemu-system-aarch64 \
    -M virt,gic_version=3 \
    -cpu cortex-a57 -nographic \
    -machine virtualization=true \
    -kernel out/reference/qemu_aarch64_clang/hafnium.bin \
    -initrd ./images/initrd.img -append "rdinit=/sbin/init" $@

#!/bin/bash

FVP_MODEL=FVP_Base_RevC-2xAEMvA

ROOT=$PWD
BLOCK_DEVICE_FILE=$ROOT/images/initrd.img
FVP_LOG_FILE=$ROOT/log/fvp_uart0.log
BOOT_INITRD=$ROOT/images/initrd.img
BOOT_DTB=$ROOT/images/hafnium_initrd/manifest.dtb
BOOT_BINRARY=$ROOT/out/reference/aem_v8a_fvp_clang/hafnium.bin

$FVP_MODEL \
    -C cluster0.NUM_CORES=4 \
    -C cluster1.NUM_CORES=4 \
    -C bp.terminal_0.start_telnet=false \
    -C bp.terminal_1.start_telnet=false \
    -C bp.terminal_2.start_telnet=false \
    -C bp.terminal_3.start_telnet=false \
    --data cluster0.cpu0=$BOOT_BINRARY@0x88000000 \
    --data cluster0.cpu0=$BOOT_INITRD@0x84000000 \
    --data cluster0.cpu0=$BOOT_DTB@0x82000000


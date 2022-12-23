#!/bin/bash

IP=10.197.124.87
#DIST=/mnt/boot
DIST=/boot
FLAGS="StrictHostKeyChecking=no"

ssh-keygen -f "/root/.ssh/known_hosts" -R "$IP"

cp out/reference/jetson_xavier_clang/hafnium.elf .
scp -o $FLAGS out/reference/jetson_xavier_clang/hafnium.bin root@$IP:$DIST/hafnium.bin
scp -o $FLAGS images/initrd.img root@$IP:$DIST/initrd.img
scp -o $FLAGS images/hafnium_initrd/initrd.img root@$IP:$DIST/initrd
scp -o $FLAGS images/extlinux.conf* root@$IP:$DIST/extlinux
scp -o $FLAGS images/extlinux.conf.hf root@$IP:$DIST/extlinux/extlinux.conf
scp -o $FLAGS images/dtb/tegra194-p2888-0001-p2822-0000.dtb root@$IP:$DIST/tegra194-p2888-0001-p2822-0000.dtb

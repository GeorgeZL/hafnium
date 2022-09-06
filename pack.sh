#!/bin/bash

BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

ROOT=$PWD
IMGS=$ROOT/images
DTBS=$IMGS/dtb
HAF_INITRD=$IMGS/hafnium_initrd
PACK_FILE=$IMGS/file

mkdir -p $IMGS
mkdir -p $DTBS
mkdir -p $HAF_INITRD

function ErrMsg()
{
    echo -e "${RED}$1${NC}"
}

function InfoMsg()
{
    echo -e "${BLUE}$1${NC}"
}

function package_boot_image()
{
    # dtb
    InfoMsg "build dts for hafnium & guest ..."
    pushd $ROOT/example
        _DTS=$(ls)
        for _f in $_DTS
        do
            _name=${_f%%.dts}
            if [ "X$_name" != "X" ]; then
                dtc -I dts -O dtb -o $DTBS/$_name.dtb $_f 2>/dev/null
            fi
        done
    popd
    cp $DTBS/manifest.dtb $HAF_INITRD

    InfoMsg "create initrd for hafnium ..."
    pushd $HAF_INITRD
        find . | cpio -o > $IMGS/initrd.img
    popd
}

function remove_hafnium_imgs()
{
    rm $IMGS/initrd.img

    pushd $HAF_INITRD
        #rm -fr *
    popd
}

function update_rootfs()
{
    InfoMsg "Update rootfs ..."
    cp /opt/linux/kbuild/arch/arm64/boot/Image  $HAF_INITRD

    pushd $ROOT/driver/linux
        ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL_PATH=/opt/linux/kbuild make
        cp *.ko $IMGS/rootfs/.
    popd

    pushd $IMGS/rootfs
        find . | cpio -o -H newc | gzip > $HAF_INITRD/initrd.img
    popd
}


remove_hafnium_imgs
update_rootfs
package_boot_image

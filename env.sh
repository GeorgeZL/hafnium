#!/bin/bash

export PATH=$PWD/prebuilts/linux-x64/clang/bin:$PWD/prebuilts/linux-x64/dtc:$PATH
export PATH=/home/workspace2/base/linaro-image-tools:$PATH

BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

CC=""
PLAT=""
ROOT=$PWD
IMGS=$ROOT/images
DTSDIR=$ROOT/example
ROOTFSDIR=
ROOTFS=
MANIFESTDTS=
GUESTDTS=

function ErrMsg()
{
    echo -e "${RED}$1${NC}"
}

function InfoMsg()
{
    echo -e "${BLUE}$1${NC}"
}

function init()
{
    if [ "X$PLAT" == "Xqemu" ]; then
        #CC=aarch64-linux-gnu-
        ROOTFSDIR=$IMGS/rootfs_qemu
        MANIFESTDTS=$DTSDIR/manifest_qemu.dts
        GUESTDTS=$DTSDIR/qemu_guest_gicv2.dts

        CROSS_COMPILE=aarch64-linux-gnu-
    elif [ "X$PLAT" == "Xfvp" ]; then
        #CC=aarch64-linux-gnu-
        ROOTFSDIR=$IMGS/rootfs_fvp
        MANIFESTDTS=$DTSDIR/manifest_fvp.dts
        GUESTDTS=$DTSDIR/fvp_guest.dts

        CROSS_COMPILE=aarch64-linux-gnu-
    elif [ "X$PLAT" == "Xxavier" ]; then
        CC=aarch64-oe4t-linux-
        ROOTFSDIR=$IMGS/rootfs_xavier
        MANIFESTDTS=$DTSDIR/manifest_xavier.dts
        GUESTDTS=$DTSDIR/xavier_guest.dts

        source /usr/local/sbin/setup_jetson_env
        CROSS_COMPILE=aarch64-oe4t-linux-
    else
        ErrMsg "Invalid Platform Configuration[qemu | fvp | xavier]"
        return 1
    fi

    if [ -L $IMGS/rootfs ]; then
        rm $IMGS/rootfs
    fi

    if [ -L $DTSDIR/manifest.dts ]; then
        rm $DTSDIR/manifest.dts
    fi

    if [ -L $DTSDIR/guest.dts ]; then
        rm $DTSDIR/guest.dts
    fi

    ln -s $ROOTFSDIR $IMGS/rootfs
    ln -s $MANIFESTDTS $DTSDIR/manifest.dts
    ln -s $GUESTDTS $DTSDIR/guest.dts
    export CC=$CC
}

if [ "X$1" == "X" ]; then
    ErrMsg "Please give the platform configuration[qemu | fvp | xavier]"
    return 1
fi

PLAT=$1

if [ ! -L ./images ]; then
    ln -s ../images/ images
fi

init

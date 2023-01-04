#!/bin/bash

ROOT=$PWD
DRV=$ROOT/driver/linux
LNX=$ROOT/third_party/linux/kbuild/

function BuildModule()
{
    pushd $DRV
    ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE KERNEL_PATH=$LNX make
    popd
}

function BuildHafnium()
{
    make
}

BuildHafnium
#BuildModule

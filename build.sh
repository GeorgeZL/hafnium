#!/bin/bash

ROOT=$PWD
DRV=$ROOT/driver/linux

function BuildModule()
{
    pushd $DRV
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KERNEL_PATH=/opt/linux/kbuild/ make
    popd
}

function BuildHafnium()
{
    make
}

BuildHafnium
#BuildModule

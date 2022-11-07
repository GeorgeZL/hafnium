#!/bin/bash

export PATH=$PWD/prebuilts/linux-x64/clang/bin:$PWD/prebuilts/linux-x64/dtc:$PATH
export PATH=/home/workspace2/base/linaro-image-tools:$PATH

if [ ! -L ./images ]; then
    ln -s ../images/ images
fi

#!/bin/bash

DIR=$PWD
find  $DIR                                                             \
    -path "$DIR/prebuilts*" -prune -o  \
    -path "$DIR/images*" -prune -o  \
    -path "$DIR/docs*" -prune -o  \
    -path "$DIR/test*" -prune -o  \
    -path "$DIR/kokoro*" -prune -o  \
    -path "$DIR/example*" -prune -o  \
    -path "$DIR/build*" -prune -o  \
    -path "$DIR/third_party*" ! -path "$DIR/third_party/dtc*" -prune -o  \
    -path "$DIR/project*" -prune -o  \
    -path "$DIR/out*" -prune -o  \
    -name "*.[chxsS]" -print > cscope.files

cscope -Rbqk

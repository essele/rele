#!/bin/bash

export CC=arm-none-eabi-gcc
export AR=arm-none-eabi-ar
export RANLIB=arm-none-eabi-ranlib
export CFLAGS="-O2 -ffreestanding -mcpu=cortex-a8"
export LDFLAGS="-ffreestanding -nostdlib"

cd pcre2 && ./autogen.sh && \
    ./configure --host=arm-none-eabi \
            --prefix=/usrmak/local \
            --disable-utf \
            --disable-pcre2-16 \
            --disable-pcre2-32 \
            --disable-shared \
            --enable-static \
            --disable-jit \
            --disable-cpp \
    && make
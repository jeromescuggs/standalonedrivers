#!/bin/bash
PWD=`pwd`
VIRT2REAL_ROOT=/home/mkrentovskiy/develop/virt2real/install-sdk
make ARCH=arm CROSS_COMPILE=$VIRT2REAL_ROOT/codesourcery/arm-2013.05/bin/arm-none-linux-gnueabi- KSRC=$VIRT2REAL_ROOT/kernel EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -DCONFIG_PLATFORM_TI_DM365 -I$PWD/include"
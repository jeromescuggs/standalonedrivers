#!/bin/bash
VIRT2REAL_ROOT=/home/mkrentovskiy/develop/virt2real/install-sdk
make ARCH=arm CROSS_COMPILE=$VIRT2REAL_ROOT/fs/output/host/usr/bin/arm-none-linux-gnueabi- KSRC=$VIRT2REAL_ROOT/kernel
#!/bin/sh
install -m 664 ./8192cu.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
mkdir -p $TARGETDIR/etc/modprobe.d
install -m 664 ./8192cu-disable-power-management.conf $TARGETDIR/etc/modprobe.d
install -m 664 ./blacklist-native-rtl8192.conf $TARGETDIR/etc/modprobe.d

#!/bin/sh
install ./8192cu.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
mkdir -p $TARGETDIR/etc/modprobe.d
install ./8192cu-disable-power-management.conf $TARGETDIR/etc/modprobe.d
install ./blacklist-native-rtl8192.conf $TARGETDIR/etc/modprobe.d

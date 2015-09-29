#!/bin/sh
install -m 664 ./os/linux/rt5572sta.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
install -m 664 ./*.dat $TARGETDIR/etc/Wireless/RT2870STA/

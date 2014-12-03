#!/bin/sh
install ./os/linux/rt5572sta.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
install ./*.dat $TARGETDIR/etc/Wireless/RT2870STA/

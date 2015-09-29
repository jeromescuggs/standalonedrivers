#!/bin/sh
install -m 664 ./8188eu.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
install -m 664 ./rtl8188eufw.bin $TARGETDIR/lib/firmware/rtlwifi/

#!/bin/sh
install ./8188eu.ko $TARGETDIR/lib/modules/$KERNEL_NAME/kernel/drivers/net/wireless
install ./rtl8188eufw.bin $TARGETDIR/lib/firmware/rtlwifi/

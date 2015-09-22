# !!! standalone not worked, call from sdk make !!!
KSRC=$DEVDIR/kernel
EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -DCONFIG_PLATFORM_TI_DM365 -I$KSRC/include"

if [ -z "${CROSS_COMPILE}" ];
then echo CROSS_COMPILE is not set - use make driversbuild
exit 1
fi

export ARCH
export CROSS_COMPILE
export DEVDIR
export KSRC
export EXTRA_CFLAGS

SUBDIRS="rt5572 rtl8188eu rtl8192cu"

for path in ${SUBDIRS}
do

if [ "$1" = "BUILD" ] ;
then
	make -C $path
fi

if [ "$1" = "INSTALL" ] ;
then
   	cd $path && ./copy.sh && cd ..
fi

done


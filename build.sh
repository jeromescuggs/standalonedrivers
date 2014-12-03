ARCH=arm
PWD=`pwd`
KSRC=$DEVDIR/kernel
EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -DCONFIG_PLATFORM_TI_DM365 -I$PWD/include"

export DEVDIR
export CROSS_COMPILE
export KSRC
export EXTRA_CFLAGS
export ARCH

for path in `ls`
do

if [ "$path" = "build.sh" ] ;
then
	continue
fi

if [ "$1" = "BUILD" ] ;
then
	make -C $path
fi

if [ "$1" = "INSTALL" ] ;
then
   	cd $path && ./copy.sh && cd ..
fi

done

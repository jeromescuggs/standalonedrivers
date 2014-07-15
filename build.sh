DEVDIR=/opt1/SDK7
PWD=`pwd`
ARCH=arm
CROSS_COMPILE=$DEVDIR/codesourcery/arm-2013.05/bin/arm-none-linux-gnueabi-
KSRC=$DEVDIR/kernel
EXTRA_CFLAGS="-DCONFIG_LITTLE_ENDIAN -DCONFIG_PLATFORM_TI_DM365 -I$PWD/include"

export DEVDIR
export CROSS_COMPILE
export KSRC
export EXTRA_CFLAGS
export ARCH

for path in `ls`
do

make -C $path

if [ "$?" = 0  ] ;
then
    cd $path
    ./copy.sh
    cd ../
fi

done

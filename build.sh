DEVDIR=/opt1/SDK6
export DEVDIR

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

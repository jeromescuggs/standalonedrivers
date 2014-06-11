#!/bin/sh
name=rt5572sta
mkdir -p ../output/$name
install ./os/linux/rt5572sta.ko ../output/$name/
install ./RT2870STA.dat ../output/$name/

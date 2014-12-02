#!/bin/sh
name=rt5572sta_new
mkdir -p ../output/$name
install ./os/linux/rt5572sta.ko ../output/$name/
install ./RT2870STA.dat ../output/$name/
install ./RT2870STACard.dat ../output/$name/
install ./RT2870AP.dat ../output/$name/
install ./RT2870APCard.dat ../output/$name/

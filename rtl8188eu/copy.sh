#!/bin/sh
name=rtl8188eu
mkdir -p ../output/$name
install ./8188eu.ko ../output/$name/
install ./rtl8188eufw.bin ../output/$name/

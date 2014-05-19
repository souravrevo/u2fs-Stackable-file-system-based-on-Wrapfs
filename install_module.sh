#!/bin/sh
lsmod
make
rmmod wrapfs.ko
insmod wrapfs.ko
lsmod

#!/bin/bash

source /home/lqd/imx8/SDK/environment-setup-armv8a-poky-linux

make imx8mp_ddr4_evk_defconfig

make all -j4

#!/bin/bash

source /home/lqd/imx8/SDK/environment-setup-armv8a-poky-linux

make imx8mp_ddr4_evk_defconfig

make all -j4


cp ./u-boot-nodtb.bin /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/iMX8M/
cp ./spl/u-boot-spl.bin /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/iMX8M/
cp ./arch/arm/dts/imx8mm-ddr4-evk.dtb /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/iMX8M/
cp ./tools/mkimage /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/iMX8M/mkimage_uboot

cd /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/
make SOC=iMX8MP clean
make SOC=iMX8MP flash_ddr4_evk

cp /home/lqd/imx8/tools/imx-mkimage/src/imx-mkimage/iMX8M/flash.bin /home/lqd/imx8/src/boot/

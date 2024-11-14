#!/bin/bash

source /home/lqd/imx8/SDK/environment-setup-armv8a-poky-linux

make imx8mp_evk_defconfig

make all -j4


cp ./u-boot-nodtb.bin /home/lqd/imx8/src/flash_bin/imx-mkimage/iMX8M/
cp ./spl/u-boot-spl.bin /home/lqd/imx8/src/flash_bin/imx-mkimage/iMX8M/
cp ./arch/arm/dts/imx8mp-evk.dtb /home/lqd/imx8/src/flash_bin/imx-mkimage/iMX8M/
cp ./tools/mkimage /home/lqd/imx8/src/flash_bin/imx-mkimage/iMX8M/mkimage_uboot

cd /home/lqd/imx8/src/flash_bin/imx-mkimage/
make SOC=iMX8MP clean
make SOC=iMX8MP flash_evk

cp /home/lqd/imx8/src/flash_bin/imx-mkimage/iMX8M/flash.bin /home/lqd/imx8/src/boot/

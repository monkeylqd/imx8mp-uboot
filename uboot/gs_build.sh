#!/bin/bash

source /home/lqd/imx8/SDK/environment-setup-armv8a-poky-linux

make imx8mp_evk_defconfig

make all -j4


cp ./u-boot-nodtb.bin ./flash_bin/imx-mkimage/iMX8M/
cp ./spl/u-boot-spl.bin ./flash_bin/imx-mkimage/iMX8M/
cp ./arch/arm/dts/imx8mp-evk.dtb ./flash_bin/imx-mkimage/iMX8M/
cp ./tools/mkimage ./flash_bin/imx-mkimage/iMX8M/mkimage_uboot

echo "uboot build finish"

cd ./flash_bin/imx-mkimage/
make SOC=iMX8MP clean
make SOC=iMX8MP flash_evk

cd -
cp ./flash_bin/imx-mkimage/iMX8M/flash.bin /home/lqd/imx8/src/boot/

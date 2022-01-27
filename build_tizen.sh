#!/bin/bash
#
JOBS="-j4"
ARM=arm64
BOOT_PATH="arch/${ARM}/boot"
IMAGE="Image"
DZIMAGE="dzImage"
RELEASE_DATE=`date +%Y%m%d%H%M%s`

CHIPSET=exynos7270
RELEASE="eng"
MODEL="tizen_solis"

echo "base_def : ${MODEL}_defconfig , Release : ${RELEASE}"

export CROSS_COMPILE=aarch64-linux-gnu-
make ARCH=${ARM} ${MODEL}_defconfig

if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"${ARCH}
	exit 1
fi

rm ${BOOT_PATH}/dts/*.dtb -f

make ${JOBS} ARCH=${ARM} ${IMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to make "${IMAGE}
	exit 1
fi

DTC_PATH="scripts/dtc/"

./exynos_dtbtool.sh -o ${BOOT_PATH}/merged-dtb -p ${DTC_PATH} -v ${BOOT_PATH}/dts/
if [ "$?" != "0" ]; then
	echo "Failed to make merged-dtb"
	exit 1
fi

./exynos_mkdzimage.sh -o ${BOOT_PATH}/${DZIMAGE} -k ${BOOT_PATH}/${IMAGE} -d ${BOOT_PATH}/merged-dtb
if [ "$?" != "0" ]; then
	echo "Failed to make mkdzImage"
	exit 1
fi

RELEASE_IMAGE=GearS3_Kernel_${RELEASE_DATE}.tar

tar cf $RELEASE_IMAGE -C $BOOT_PATH $DZIMAGE
if [ "$?" != "0" ]; then
	echo "Failed to tar $DZIMAGE"
	exit 1
fi

echo $RELEASE_IMAGE

make kernelversion
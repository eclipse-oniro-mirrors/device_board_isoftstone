#!/bin/bash

# Copyright (c) 2021 iSoftStone Open Source Organization .
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

pushd ${1}
ROOT_DIR=${5}
export PRODUCT_PATH=${4}

KERNEL_PATCH_PATH=${3}/kernel/patches
HDF_PATCH=${3}/kernel/patches/hdf.patch
KERNEL_SOURCE=${ROOT_DIR}/kernel/linux/linux-5.10
KERNEL_SRC_TMP_PATH=${ROOT_DIR}/out/kernel/src_tmp/linux-5.10
KERNEL_CONFIG_FILE=${3}/kernel/patches/rk3399_standard_defconfig


rm -rf ${KERNEL_SRC_TMP_PATH}
mkdir -p ${KERNEL_SRC_TMP_PATH}

cp -arf ${KERNEL_SOURCE}/* ${KERNEL_SRC_TMP_PATH}/

cd ${KERNEL_SRC_TMP_PATH}

#合入HDF patch
bash ${ROOT_DIR}/drivers/adapter/khdf/linux/patch_hdf.sh ${ROOT_DIR} ${KERNEL_SRC_TMP_PATH} ${HDF_PATCH}

#合入kernel patch
bash ${3}/kernel/patches/kernel-patch.sh ${KERNEL_SRC_TMP_PATH} ${KERNEL_PATCH_PATH}

cp -rf ${3}/kernel/logo* ${KERNEL_SRC_TMP_PATH}/

#拷贝config
cp -rf ${KERNEL_CONFIG_FILE} ${KERNEL_SRC_TMP_PATH}/arch/arm64/configs/rockchip_linux_defconfig

#编译内核
if [ "enable_ramdisk" == "${6}" ]; then
    ./make-ohos.sh sapphire-rk3399 enable_ramdisk
else
    ./make-ohos.sh sapphire-rk3399 disable_ramdisk
fi

mkdir -p ${2}

if [ "enable_ramdisk" != "${6}" ]; then
    cp boot_linux.img ${2}/boot_linux.img
    cp resource.img ${2}/resource.img
fi

cp ${3}/loader/parameter.txt ${2}/parameter.txt
cp ${3}/loader/MiniLoaderAll.bin ${2}/MiniLoaderAll.bin
cp ${3}/loader/uboot.img ${2}/uboot.img
cp ${3}/loader/trust.img ${2}/trust.img
popd

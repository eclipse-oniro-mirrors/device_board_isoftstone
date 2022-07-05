#!/bin/bash

# Copyright (C) 2022 iSoftStone Device Co., Ltd.
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


export LICHEE_IC=${1}
export LICHEE_KERN_VER=${2}
export LICHEE_ARCH=${3}

# root path
export OHOS_ROOT_PATH=${4}
export PRODUCT_COMPANY=seed
export SOC_COMPANY=allwinner
export LICHEE_PLATFORM=linux
export LICHEE_LINUX_DEV=bsp
export IC_COMPANY=allwinner
export LICHEE_KERN_SYSTEM=kernel_boot
export LICHEE_FLASH=default
export PRODUCT_NAME=t507_pines
export DEVICE_NAME=${PRODUCT_NAME}
# test_flag
build_flag=${5}


if [ "x$LICHEE_IC" == "xt507" ]; then
	export LICHEE_BOARD=t507_pines
	export LICHEE_CHIP=sun50iw9p1
	export LICHEE_KERN_DEFCONF=t507_linux-5.10_defconfig
fi


if [ "x$LICHEE_ARCH" == "xarm64" ]; then
	export LICHEE_CROSS_COMPILER=aarch64-linux-gnu
	export LICHEE_KERN_DEFCONF_RELATIVE=${LICHEE_KERN_DEFCONF}
	export LICHEE_TOOLCHAIN_PATH=${OHOS_ROOT_PATH}/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu
	export LICHEE_GNU_NAME=aarch64-linux-gnu-
fi

# src kernel path
export OHOS_KERNEL_SRC_PATH=${OHOS_ROOT_PATH}/kernel/linux                    #kernel/linux

# vendor path
export LICHEE_VENDOR_DIR=${OHOS_ROOT_PATH}/vendor
export PRODUCT_PATH=vendor/${PRODUCT_COMPANY}/${PRODUCT_NAME}                 #vendor/seed/t507_pines

# out path
export LICHEE_OUT_DIR=${OHOS_ROOT_PATH}/out
export BUILD_ROOT_PATH=${LICHEE_OUT_DIR}/${PRODUCT_NAME}
export LICHEE_KERN_DIR=${LICHEE_OUT_DIR}/KERNEL_OBJ/kernel/src_tmp/${LICHEE_KERN_VER}
export KERNEL_BUILD_SCRIPT_DIR=${LICHEE_KERN_DIR}
export KERNEL_BUILD_OUT_DIR=${LICHEE_KERN_DIR}
export LICHEE_KERN_DEFCONF_ABSOLUTE=${LICHEE_KERN_DIR}/arch/${LICHEE_ARCH}/configs/${LICHEE_KERN_DEFCONF}
export LICHEE_PACK_OUT_DIR=/${LICHEE_OUT_DIR}/pack_out
export LICHEE_PLAT_OUT=${LICHEE_OUT_DIR}/kernel/bsp

# device soc path
export DEVICE_COMPANY_PATH=${OHOS_ROOT_PATH}/device/soc/${IC_COMPANY}         #device/soc/allwinner
export DEVICE_CHIP_PATH=${DEVICE_COMPANY_PATH}/${LICHEE_IC}                   #device/soc/allwinner/t507

# device board path
export BOARD_COMPANY_PATH=${OHOS_ROOT_PATH}/device/board/${PRODUCT_COMPANY}   #device/board/seed
export BOARD_PATH=${BOARD_COMPANY_PATH}/${PRODUCT_NAME}                       #device/board/seed/t507_pines
export BOARD_KERNEL_PATH=${BOARD_PATH}/kernel                                 #device/board/seed/t507_pines/kernel
export LICHEE_TOP_DIR=${BOARD_KERNEL_PATH}/build                              #device/board/seed/T507_pines/kernel/build
export LICHEE_BUILD_DIR=${LICHEE_TOP_DIR}/scripts                             #device/board/seed/T507_pines/kernel/build/scripts
export LICHEE_TOOLS_DIR=${LICHEE_TOP_DIR}/tools                               #device/board/seed/T507_pines/kernel/build/tools
export LICHEE_BSP_DIR=${BOARD_KERNEL_PATH}/driver                             #device/board/seed/T507_pines/kernel/driver
export LICHEE_SRC_KERN_DEFCONF=${DEVICE_CHIP_PATH}/patches/config/${LICHEE_KERN_DEFCONF}
export LICHEE_LOADER_DIR=${BOARD_PATH}/loader                                 #device/board/seed/t507_pines/loader
export LICHEE_CHIP_CONFIG_DIR=${LICHEE_LOADER_DIR}                   	      #device/board/seed/t507_pines/loader
export LICHEE_BRANDY_OUT_DIR=${LICHEE_LOADER_DIR}/bin                         #device/board/seed/t507_pines/loader/bin
export LICHEE_BOARD_CONFIG_DIR=${LICHEE_CHIP_CONFIG_DIR}/configs/${PRODUCT_NAME}  #device/board/seed/t507_pines/loader/configs/t507_pines
[ -d "${LICHEE_BSP_DIR}" ] && export BSP_TOP=${LICHEE_KERN_DIR}/bsp/



pack_config_file=.packconfig

export BUILD_CONFIG_FILE=${LICHEE_TOP_DIR}/${pack_config_file}


function mk_error()
{
	echo -e "\033[47;31mERROR: $*\033[0m"
}

function mk_warn()
{
	echo -e "\033[47;34mWARN: $*\033[0m"
}

function mk_info()
{
	echo -e "\033[47;30mINFO: $*\033[0m"
}

tag_version="kernel-v0.1"

function check_env()
{
	if [ -f $LICHEE_KERN_DIR/scripts/build.sh ]; then
		KERNEL_BUILD_SCRIPT_DIR=$LICHEE_KERN_DIR
		KERNEL_BUILD_SCRIPT=scripts/build.sh
		KERNEL_BUILD_OUT_DIR=$LICHEE_KERN_DIR
		KERNEL_STAGING_DIR=$LICHEE_KERN_DIR/output
	else
		KERNEL_BUILD_SCRIPT_DIR=$LICHEE_BUILD_DIR
		KERNEL_BUILD_SCRIPT=mkkernel.sh
		KERNEL_BUILD_OUT_DIR=$LICHEE_KERN_DIR
		KERNEL_STAGING_DIR=$LICHEE_OUT_DIR/kernel/staging
	fi
}

function load_kernel()
{
	mk_info "start to load kernel"

	if [ -d "$LICHEE_KERN_DIR" ]; then
		rm -rf $LICHEE_KERN_DIR
	fi

	mkdir -p ${LICHEE_KERN_DIR}

	#cp kernel to tmp kernel dir
	git clone ${OHOS_KERNEL_SRC_PATH}/${LICHEE_KERN_VER} ${LICHEE_KERN_DIR}
	mk_info "Finish copying ${LICHEE_LICHEE_KERN_VER} kernel to out path"

	# enter kernel path to put sunxi patch
	cd ${LICHEE_KERN_DIR}
	git tag -a $tag_version -m "kernel tag"

	mk_info "load kernel success"

}

ln_list=(
	$OHOS_ROOT_PATH/drivers/adapter/khdf/linux    drivers/hdf/khdf
	$OHOS_ROOT_PATH/drivers/framework             drivers/hdf/framework
	$OHOS_ROOT_PATH/drivers/framework/include     include/hdf
)

cp_list=(
	$OHOS_ROOT_PATH/third_party/bounds_checking_function  ./
	$OHOS_ROOT_PATH/device/soc/hisilicon/common/platform/wifi         drivers/hdf/
	$OHOS_ROOT_PATH/third_party/FreeBSD/sys/dev/evdev     drivers/hdf/
)

function copy_external_compents()
{
	for ((i=0; i<${#cp_list[*]}; i+=2))
	do
		dst_dir=${cp_list[$(expr $i + 1)]}/${cp_list[$i]##*/}
		mkdir -p $dst_dir
		cp -arfL ${cp_list[$i]}/* $dst_dir/
	done
}

function ln_hdf_repos()
{
	for ((i=0; i<${#ln_list[*]}; i+=2))
	do
		ln -sf ${ln_list[$i]} ${ln_list[$(expr $i + 1)]}
	done
}

function add_patch_to_kernel()
{
	# get kernel patch path

	local patch_path=${DEVICE_CHIP_PATH}/patches    # Todo
	local kernel_patch=${patch_path}/kernel/${LICHEE_KERN_VER}/*.patch # all patch
	local hdf_patch=${patch_path}/hdf/${LICHEE_KERN_VER}/*.patch                      # Todo
	local kernel_config=${patch_path}/config/${LICHEE_KERN_DEFCONF} # Todo

	echo "apply allwinner patch to kernel"

	cd ${LICHEE_KERN_DIR}   # enter kernel path

	# add aw patch
	if [ -d "$patch_path/kernel/${LICHEE_KERN_VER}" ] ; then
		git reset --hard $tag_version
		git am ${kernel_patch}
	else
		mk_error "not found kernel patch: $patch_path/kernel"
		exit 1
	fi

	# add bsp driver to kernel
	if [ "x$LICHEE_KERN_VER" == "xlinux-5.10" ]; then
		if [ -e "${LICHEE_BSP_DIR}" ]; then
			rm -rf bsp
		fi
		cp -af ${LICHEE_BSP_DIR} bsp
	fi

#---------------------------------------------------------------------------
	# add hdf patch
	if [ -d "$patch_path/hdf/${LICHEE_KERN_VER}" ] ; then
		git apply ${hdf_patch}
	else
		mk_error "not found hdf patch: $patch_path/hdf"
		exit 1
	fi
#---------------------------------------------------------------------------

	# cp config to kernel
	if [ -f "$kernel_config" ] ; then
		cp -af ${kernel_config} ./arch/${LICHEE_ARCH}/configs/
	else
		mk_error "not found kernel config: ${kernel_config}"
		exit 1
	fi

	ln_hdf_repos
	copy_external_compents
	cd -
	echo "put sunxi patches successfully"
}

function substitute_inittab()
{

	declare console
	env_cfg_dir=${LICHEE_CHIP_CONFIG_DIR}/configs/default/env.cfg

	if [ ! -f ${env_cfg_dir} ];then
		mk_info "not find env.cfg in ${env_cfg_dir}"
		return;
	fi
	console=$(grep -m1 -o ${env_cfg_dir} -e 'console=\w\+')
	console=$(sed -e 's/console=\(\w\+\).*/\1/g' <<< $console)

	if [ ${console} ]
	then
		sed -ie "s/ttyS[0-9]*/${console}/g" $1
	fi

}

function prepare_mkkernel()
{
	# mark kernel .config belong to which platform
	local config_mark="${KERNEL_BUILD_OUT_DIR}/.config.mark"
	local board_dts="$LICHEE_BOARD_CONFIG_DIR/${LICHEE_KERN_VER}/board.dts"

	if [ -f ${config_mark} ] ; then
		local tmp=`cat ${config_mark}`
		local tmp1="${LICHEE_CHIP}_${LICHEE_BOARD}_${LICHEE_PLATFORM}"
		if [ ${tmp} != ${tmp1} ] ; then
			mk_info "clean last time build for different platform"
			if [ "x${LICHEE_KERN_DIR}" != "x" -a -d ${LICHEE_KERN_DIR} ]; then
				(cd ${KERNEL_BUILD_SCRIPT_DIR} && [ -x ${KERNEL_BUILD_SCRIPT} ] && ./${KERNEL_BUILD_SCRIPT} "distclean")
				rm -rf ${KERNEL_BUILD_OUT_DIR}/.config
				echo "${LICHEE_CHIP}_${LICHEE_BOARD}_${LICHEE_PLATFORM}" > ${config_mark}
			fi
		fi
	else
		echo "${LICHEE_CHIP}_${LICHEE_BOARD}_${LICHEE_PLATFORM}" > ${config_mark}
	fi
}

function mkkernel()
{
	#source .buildconfig-bk
	mk_info "build kernel ..."

	LICHEE_KERN_SYSTEM="kernel_boot"

	prepare_mkkernel
	
	echo "(cd ${KERNEL_BUILD_SCRIPT_DIR} && [ -x ${KERNEL_BUILD_SCRIPT} ] && ./${KERNEL_BUILD_SCRIPT})"
	(cd ${KERNEL_BUILD_SCRIPT_DIR} && [ -x ${KERNEL_BUILD_SCRIPT} ] && ./${KERNEL_BUILD_SCRIPT} $@)
	[ $? -ne 0 ] && mk_error "build kernel Failed" && return 1
	# copy files related to pack to platform out

	cp ${KERNEL_BUILD_OUT_DIR}/vmlinux ${LICHEE_PLAT_OUT}

	cp ${LICHEE_KERN_DIR}/scripts/dtc/dtc ${LICHEE_PLAT_OUT}
	local dts_path=$LICHEE_KERN_DIR/arch/${LICHEE_ARCH}/boot/dts
	[ "x${LICHEE_ARCH}" == "xarm64" ] && \
	dts_path=$dts_path/sunxi
	rm -rf ${LICHEE_PLAT_OUT}/.sun*.dtb.*.tmp
	cp $dts_path/.${LICHEE_CHIP}-*.dtb.d.dtc.tmp ${LICHEE_PLAT_OUT} 2>/dev/null
	cp $dts_path/.${LICHEE_CHIP}-*.dtb.dts.tmp ${LICHEE_PLAT_OUT} 2>/dev/null

	cp $dts_path/.board.dtb.d.dtc.tmp ${LICHEE_PLAT_OUT} 2>/dev/null
	cp $dts_path/.board.dtb.dts.tmp ${LICHEE_PLAT_OUT} 2>/dev/null

	mk_info "build kernel OK."

	#delete board.dts
	if [ "x${LICHEE_ARCH}" == "xarm64" ]; then
		if [ -f ${LICHEE_KERN_DIR}/arch/${LICHEE_ARCH}/boot/dts/sunxi/board.dts ]; then
			rm ${LICHEE_KERN_DIR}/arch/${LICHEE_ARCH}/boot/dts/sunxi/board.dts
		fi
	else
		if [ -f ${LICHEE_KERN_DIR}/arch/${LICHEE_ARCH}/boot/dts/board.dts ];then
			rm ${LICHEE_KERN_DIR}/arch/${LICHEE_ARCH}/boot/dts/board.dts
		fi
	fi
}

function delete_kernel_config()
{
	if [ -f "${LICHEE_KERN_DIR}/.config" ] ; then
		rm -rf ${LICHEE_KERN_DIR}/.config
	fi
	if [ -f "${LICHEE_KERN_DIR}/.config.mark" ] ; then
		rm -rf ${LICHEE_KERN_DIR}/.config
	fi
		if [ -f "${LICHEE_KERN_DIR}/.config.old" ] ; then
		rm -rf ${LICHEE_KERN_DIR}/.config
	fi
}

function mklichee()
{

	mk_info "----------------------------------------"
	mk_info "build lichee ..."
	mk_info "chip: $LICHEE_CHIP"
	mk_info "platform: $LICHEE_PLATFORM"
	mk_info "kernel: $LICHEE_KERN_VER"
	mk_info "board: $LICHEE_BOARD"
	mk_info "output: $LICHEE_PLAT_OUT"
	mk_info "----------------------------------------"

	check_output_dir

	mkkernel
	if [ $? -ne 0 ]; then
		mk_info "mkkernel failed"
		exit 1
	fi
}

function check_output_dir()
{
	#mkdir out directory:
	if [ "x" != "x${LICHEE_PLAT_OUT}" ]; then
		if [ ! -d ${LICHEE_PLAT_OUT} ]; then
			mkdir -p ${LICHEE_PLAT_OUT}
		fi
	fi
}

allconfig=(
LICHEE_IC
LICHEE_IC_BIG
LICHEE_KERN_VER
LICHEE_PLATFORM
LICHEE_LINUX_DEV
LICHEE_CHIP
LICHEE_ARCH
LICHEE_BOARD
LICHEE_FLASH
LICHEE_CROSS_COMPILER
LICHEE_ROOTFS
LICHEE_KERN_DEFCONF
LICHEE_KERN_DEFCONF_RELATIVE
IC_COMPANY
PRODUCT_PATH
KERNEL_BUILD_SCRIPT
LICHEE_KERN_SYSTEM
OHOS_ROOT_PATH
OHOS_KERNEL_SRC_PATH
LICHEE_OUT_DIR
LICHEE_KERN_DIR
KERNEL_BUILD_SCRIPT_DIR
KERNEL_BUILD_OUT_DIR
KERNEL_STAGING_DIR
LICHEE_KERN_DEFCONF_ABSOLUTE
DEVICE_COMPANY_PATH
DEVICE_CHIP_PATH
LICHEE_TOP_DIR
LICHEE_BUILD_DIR
LICHEE_TOOLS_DIR
LICHEE_DEVICE_DIR
LICHEE_CHIP_CONFIG_DIR
LICHEE_BOARD_CONFIG_DIR
LICHEE_PRODUCT_CONFIG_DIR
LICHEE_BRANDY_OUT_DIR
LICHEE_PACK_OUT_DIR
LICHEE_TOOLCHAIN_PATH
LICHEE_PLAT_OUT
BSP_TOP
PRODUCT_NAME)

function save_config()
{
	local cfgkey=$1
	local cfgval=$2
	local cfgfile=$3
	local dir=$(dirname $cfgfile)
	[ ! -d $dir ] && mkdir -p $dir
	cfgval=$(echo -e "$cfgval" | sed -e 's/^\s\+//g' -e 's/\s\+$//g')
	if [ -f $cfgfile ] && [ -n "$(sed -n "/^\s*export\s\+$cfgkey\s*=/p" $cfgfile)" ]; then
		sed -i "s|^\s*export\s\+$cfgkey\s*=\s*.*$|export $cfgkey=$cfgval|g" $cfgfile
	else
		echo "export $cfgkey=$cfgval" >> $cfgfile
	fi
}

function save_all_config()
{

	local cfgkey=""
	local cfgval=""

	for cfgkey in ${allconfig[@]}; do
		[ "x$cfgkey" == "xCONFIG_SESSION_SEPARATE" ] && continue
		cfgval="$(eval echo '$'${cfgkey})"
		save_config "$cfgkey" "$cfgval" $BUILD_CONFIG_FILE
	done

}

KERNEL_MAKE="make -C ${LICHEE_KERN_DIR} 0=${LICHEE_KERN_DIR} ARCH=${LICHEE_ARCH} "
KERNEL_MAKE+="ARCH_PREFIX=${LICHEE_ARCH} "
KERNEL_MAKE+="CROSS_COMPILE=${LICHEE_TOOLCHAIN_PATH}/bin/${LICHEE_GNU_NAME} "



if [ "x$build_flag" == "xloadconfig" ]; then
	mk_info "make loadconfig"
	(cd ${LICHEE_KERN_DIR} && \
	${KERNEL_MAKE} defconfig KBUILD_DEFCONFIG=$LICHEE_KERN_DEFCONF_RELATIVE
	)
	exit 1
elif [ "x$build_flag" == "xmenuconfig" ]; then
	mk_info "make menuconfig"
	cd ${LICHEE_KERN_DIR} && ${KERNEL_MAKE} menuconfig
	exit 1
elif [ "x$build_flag" == "xsaveconfig" ] ;then
	mk_info "make saveconfig"
	cd ${LICHEE_KERN_DIR} && ${KERNEL_MAKE} savedefconfig
	cp $LICHEE_KERN_DIR/defconfig $LICHEE_KERN_DEFCONF_ABSOLUTE
	cp $LICHEE_KERN_DIR/defconfig $LICHEE_SRC_KERN_DEFCONF
	exit 1
fi

if [ "x$build_flag" == "xload" ] ; then
	mk_info "Only load kernel"
	load_kernel
	add_patch_to_kernel
	exit 0
elif [ "x$build_flag" == "xkernel" ] ;then
	mk_info "Only build kernel"
else
	load_kernel
	add_patch_to_kernel
fi

delete_kernel_config

check_env

mklichee

save_all_config

mk_info "----------------------------------------"
mk_info "build kernel OK."
mk_info "----------------------------------------"



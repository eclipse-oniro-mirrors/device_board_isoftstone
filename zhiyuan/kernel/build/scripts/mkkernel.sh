#!/bin/bash

# Copyright (C) 2022 VYAGOO TECHNOLOGY Co., Ltd.
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

function build_setup()
{
    local action=$1

    # ARCH & CROSS_COMPILE
    export ARCH=arm
    export CROSS_COMPILE=arm-linux-gnueabi-

    if [[ "${LICHEE_CHIP}" =~ ^sun5[0-9]i ]] && [ "x${LICHEE_ARCH}" = "xarm64" ]; then
        export ARCH=arm64
        export CROSS_COMPILE=aarch64-linux-gnu-
    fi

    if [ -n "${LICHEE_TOOLCHAIN_PATH}" -a -d "${LICHEE_TOOLCHAIN_PATH}" ]; then
        local GCC=$(find ${LICHEE_TOOLCHAIN_PATH} -perm /a+x -a -regex '.*-gcc')
        export CROSS_COMPILE="${GCC%-*}-";
    elif [ -n "${LICHEE_CROSS_COMPILER}" ]; then
        export CROSS_COMPILE="${LICHEE_CROSS_COMPILER}-"
    fi

    # cc & make
    if [ -n "$ANDROID_CLANG_PATH" ]; then
        export PATH=$ANDROID_CLANG_PATH:$PATH
        MAKE="make CC=clang HOSTCC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy"
        MAKE+=" LLVM=1 LLVM_IAS=1"
        local ARCH_PREFIX=arm
        [ "x$ARCH" == "xarm64" ] && ARCH_PREFIX=aarch64
        if [ -n "$ANDROID_TOOLCHAIN_PATH" ]; then
            export CROSS_COMPILE=$ANDROID_TOOLCHAIN_PATH/$ARCH_PREFIX-linux-androidkernel-
            export CLANG_TRIPLE=$ARCH_PREFIX-linux-gnu-
        fi
    else
        MAKE="make CROSS_COMPILE="${CCACHE_Y}${CROSS_COMPILE}""
    fi

    # kerneltree, out & staging
    KERNEL_SRC=$LICHEE_KERN_DIR
    BUILD_OUT_DIR=$LICHEE_KERN_DIR
    STAGING_DIR=$LICHEE_OUT_DIR/kernel/staging
    MAKE+=" ARCH=${ARCH} -j${LICHEE_JLEVEL} O=${BUILD_OUT_DIR}"
    MAKE+=" KERNEL_SRC=$KERNEL_SRC INSTALL_MOD_PATH=${STAGING_DIR}"
    cd $KERNEL_SRC

    [[ "$action" =~ ^(dist)?clean|deal_verity ]] && return 0

    rm -rf $STAGING_DIR && mkdir -p $STAGING_DIR
	#rm -rf $BUILD_OUT_DIR && mkdir -p $BUILD_OUT_DIR
	echo ${MAKE}
    [ "$BUILD_OUT_DIR" != "$KERNEL_SRC" ] && ${MAKE} O= mrproper

    if [ ! -f $BUILD_OUT_DIR/.config ]; then
        printf "\n\033[0;31;1mUsing default config ${LICHEE_KERN_DEFCONF_ABSOLUTE} ...\033[0m\n\n"
        ${MAKE} defconfig KBUILD_DEFCONFIG=${LICHEE_KERN_DEFCONF_RELATIVE}
    fi
    [ ! -f $BUILD_OUT_DIR/include/generated/utsrelease.h ] && ${MAKE} archprepare

    # kernel version
    KERNEL_VERSION=$(awk -F\" '/UTS_RELEASE/{print $2}' $BUILD_OUT_DIR/include/generated/utsrelease.h)
}


function build_check()
{
    local SUNXI_SPARSE_CHECK SUNXI_SMATCH_CHECK SUNXI_STACK_CHECK
    local checktools_dir tools_sparse tools_smatch CHECK

    if [ "x$SUNXI_CHECK" = "x1" ];then
        SUNXI_SPARSE_CHECK=1
        SUNXI_SMATCH_CHECK=1
        SUNXI_STACK_CHECK=1
    fi

    checktools_dir=$LICHEE_TOOLS_DIR/codecheck
    tools_sparse=$checktools_dir/sparse/sparse
    tools_smatch=$checktools_dir/smatch/smatch

    if [ "x$SUNXI_SPARSE_CHECK" = "x1" ] && [ -f $tools_sparse ];then
        echo "\n\033[0;31;1mBuilding Round for sparse check...\033[0m\n\n"
        CHECK="$tools_sparse"
        ${MAKE} CHECK=${CHECK} C=2 all modules 2>&1 | tee $STAGING_DIR/build_sparse.txt
        egrep -w '(warn|error|warning)' $STAGING_DIR/build_sparse.txt > $STAGING_DIR/warn_sparse.txt
    fi

    if [ "x$SUNXI_SMATCH_CHECK" = "x1" ]&& [ -f $tools_smatch ];then
        echo "\n\033[0;31;1mBuilding Round for smatch check...\033[0m\n\n"
        CHECK="$tools_smatch --full-path --no-data -p=kkernel"
        ${MAKE} CHECK=${CHECK} C=2 all modules 2>&1 | tee $STAGING_DIR/build_smatch.txt
        egrep -w '(warn|error|warning)' $STAGING_DIR/build_smatch.txt > $STAGING_DIR/warn_smatch.txt
    fi

    if [ "x$SUNXI_STACK_CHECK" = "x1" ];then
        ${MAKE} checkstack 2>&1 | tee $STAGING_DIR/warn_stack.txt
    fi
}

function build_dts()
{
    local kbuild=$1

    echo "---build dts for ${LICHEE_CHIP} ${LICHEE_BOARD}-----"

    local dtsfile=$LICHEE_BOARD_CONFIG_DIR/$LICHEE_KERN_VER/board.dts
    local outpath=$STAGING_DIR
    local DTC=$LICHEE_KERN_DIR/scripts/dtc/dtc
    outname="sunxi.dtb"

    dep=$STAGING_DIR/dts_dep
    mkdir -p $dep

    set -e
    cpp \
        -Wp,-MD,${dep}/.${outname}.d.pre.tmp \
        -nostdinc \
        -I $LICHEE_KERN_DIR/include \
        -I $LICHEE_KERN_DIR/bsp/include \
        -I $LICHEE_CHIP_CONFIG_DIR/configs/default/${LICHEE_KERN_VER} \
        -undef \
        -D__DTS__ \
        -x assembler-with-cpp \
        -o ${dep}/.${outname}.dts.tmp \
        $dtsfile

    $DTC \
        -O dtb \
        -o $outpath/$outname \
        -W no-unit_address_vs_reg \
        -W no-unit_address_format \
        -W no-unique_unit_address\
        -W no-simple_bus_reg \
        -b 0 \
        -@ \
        -i $LICHEE_CHIP_CONFIG_DIR/configs/default/${LICHEE_KERN_VER} \
        -d $dep/.${outname}.d.dtc.tmp $dep/.${outname}.dts.tmp

    cat $dep/.${outname}.d.pre.tmp $dep/.${outname}.d.dtc.tmp > $dep/.${outname}.d
}

function merge_config()
{
    local config_list=(
        '[ "x$PACK_TINY_ANDROID" = "xtrue" ]:$KERNEL_SRC/linaro/configs/sunxi-tinyandroid.conf'
        '[ -n "$PACK_AUTOTEST" ]:$KERNEL_SRC/linaro/configs/sunxi-common.conf'
        '[ -n "$PACK_BSPTEST" -o -n "$BUILD_SATA" -o "x$LICHEE_LINUX_DEV" = "xsata" ]:$KERNEL_SRC/linaro/configs/sunxi-common.conf'
        '[ -n "$PACK_BSPTEST" -o -n "$BUILD_SATA" -o "x$LICHEE_LINUX_DEV" = "xsata" ]:$KERNEL_SRC/linaro/configs/sunxi-sata.conf'
        '[ -n "$PACK_BSPTEST" -o -n "$BUILD_SATA" -o "x$LICHEE_LINUX_DEV" = "xsata" ]:$KERNEL_SRC/linaro/configs/sunxi-sata-${LICHEE_CHIP}.conf'
        '[ -n "$PACK_BSPTEST" -o -n "$BUILD_SATA" -o "x$LICHEE_LINUX_DEV" = "xsata" ]:$KERNEL_SRC/linaro/configs/sunxi-sata-${ARCH}.conf'
    )

    local condition config

    for e in "${config_list[@]}"; do
       condition="${e/:*}"
       config="$(eval echo ${e#*:})"
       if eval $condition; then
           if [ -f $config ]; then
               (cd $KERNEL_SRC && ARCH=${ARCH} $KERNEL_SRC/scripts/kconfig/merge_config.sh -O $BUILD_OUT_DIR $BUILD_OUT_DIR/.config $config)
           fi
       fi
    done
}

function exchange_sdc()
{
    # exchange sdc0 and sdc2 for dragonBoard card boot
    if [ "x${LICHEE_LINUX_DEV}" = "xdragonboard" -o "x${LICHEE_LINUX_DEV}" = "xdragonmat" ]; then
        local SYS_BOARD_FILE=$LICHEE_BOARD_CONFIG_DIR/board.dts

        if [ "x${ARCH}" = "xarm" ];then
            local DTS_PATH=$KERNEL_SRC/arch/${ARCH}/boot/dts
        else
            local DTS_PATH=$KERNEL_SRC/arch/${ARCH}/boot/dts/sunxi
        fi

        if [ -f ${DTS_PATH}/${LICHEE_CHIP}_bak.dtsi ];then
            rm -f ${DTS_PATH}/${LICHEE_CHIP}.dtsi
            mv ${DTS_PATH}/${LICHEE_CHIP}_bak.dtsi ${DTS_PATH}/${LICHEE_CHIP}.dtsi
        fi
        # if find dragonboard_test=1 in board.dts ,then will exchange sdc0 and sdc2
        if [ -n "`grep "dragonboard_test" $SYS_BOARD_FILE | grep "<1>;" `" ]; then
            echo "exchange sdc0 and sdc2 for dragonboard card boot"
            $LICHEE_BUILD_DIR/swapsdc.sh  ${DTS_PATH}/${LICHEE_CHIP}.dtsi
        fi
    fi
}

function build_kernel()
{
    echo "Building kernel"

    local MAKE_ARGS="modules"

    if [ "${ARCH}" = "arm" ]; then
        # uImage is arm architecture specific target
        MAKE_ARGS+=" uImage dtbs LOADADDR=0x40008000"
    else
        MAKE_ARGS+=" all"
    fi

    exchange_sdc
    merge_config
    ${MAKE} $MAKE_ARGS

    # update kernel version
    KERNEL_VERSION=$(awk -F\" '/UTS_RELEASE/{print $2}' $BUILD_OUT_DIR/include/generated/utsrelease.h)

    build_check
}

function clean_kernel()
{
    local clarg="clean"
    [ "x$1" == "xdistclean" ] && clarg="distclean"

    echo "Cleaning kernel, arg: $clarg ..."
    ${MAKE} "$clarg"

    rm -rf $STAGING_DIR $LICHEE_PLAT_OUT/lib $LICHEE_PLAT_OUT/dist
    [ "$clarg" == "distclean" ] && rm -rf ${LICHEE_OUT_DIR}/kernel
}

function build_modules()
{
    local module_list=(
        "NAND:${KERNEL_SRC}/modules/nand"
        "NAND:${KERNEL_SRC}/bsp/modules/nand"
        "GPU:$KERNEL_SRC/modules/gpu"
        "GPU:$KERNEL_SRC/bsp/modules/gpu"
    )

    local module_name module_path

    for e in "${module_list[@]}"; do
        module_name="${e/:*}"
        module_path="${e#*:}"
        if [ ! -e $module_path ]; then
            printf "$module_path does not exist!\n"
            continue
        fi
        printf "\033[34;1m[%4s]: %s\033[0m\n" "$module_name" "Build module driver"
        ${MAKE} -C $module_path M=$module_path -j1
        ${MAKE} -C $module_path M=$module_path modules_install
        printf "\033[34;1m[%4s]: %s\033[0m\n" "$module_name" "Build done"
    done

    return 0
}

function clean_modules()
{
    local module_list=(
        "NAND:${KERNEL_SRC}/modules/nand"
        "GPU:$KERNEL_SRC/modules/gpu"
    )

    local module_name module_path

    for e in "${module_list[@]}"; do
        module_name="${e/:*}"
        module_path="${e#*:}"
        [ ! -e $module_path ] && continue

        printf "\033[34;1m[%4s]: %s\033[0m\n" "$module_name" "Clean module driver"
        ${MAKE} -C $module_path M=$module_path clean
        printf "\033[34;1m[%4s]: %s\033[0m\n" "$module_name" "Clean done"
    done

    return 0
}

function longan_modify_init()
{
    local cmd="/\(\[ -x \/mnt\/sbin\/init\)/c"

    cmd+="if [ -x /mnt/init ]; then\n"
    cmd+="    mount -n --move /proc /mnt/proc\n"
    cmd+="    mount -n --move /sys /mnt/sys\n"
    cmd+="    mount -n --move /dev /mnt/dev\n"
    cmd+="    exec switch_root /mnt /init\n"
    cmd+="fi\n"

    sed -i "$cmd" $STAGING_DIR/skel/init
}

function build_rootfs()
{
    local action=$1
    local src=$2
    local dst=$3
    case $action in
        e)
            # src is file & dst is path
            rm -rf $dst
            mkdir $dst
            gzip -dc $src | (cd $dst; fakeroot cpio -i)
            ;;
        c)
            # src is path & dst is file
            rm -rf $dst
            [ ! -d $src ] && echo "src not exist" && return 1
            (cd $src && find . | fakeroot cpio -o -Hnewc | gzip > $dst)
            ;;
    esac
}

function build_ramfs()
{
    local kernel_size=0
    local bss_start=0
    local bss_stop=0
    local temp=0
    local bss_section_size=0
    local CHIP="${LICHEE_CHIP/iw*}i"

    local DTB="$STAGING_DIR/sunxi.dtb"

    if [ "${LICHEE_COMPRESS}" = "gzip" ]; then
        if [ "${ARCH}" = "arm" ]; then
            local BIMAGE="$STAGING_DIR/zImage";
        elif [ "${ARCH}" = "arm64" -o "${ARCH}" = "riscv" ]; then
            local BIMAGE="$STAGING_DIR/Image.gz";
        fi
    else
        local BIMAGE="$STAGING_DIR/bImage";
    fi

    local RAMDISK="$STAGING_DIR/rootfs.cpio.gz"
    local TINYOS_RAMDISK="${LICHEE_BR_OUT}/images/rootfs.cpio.gz"
    local BASE="0x40000000"
    local RAMDISK_OFFSET="0x0"
    local KERNEL_OFFSET="0x0"
    local DTB_OFFSET="0x0"

    if [ "x${LICHEE_LINUX_DEV}" = "xtinyos" -a -f ${TINYOS_RAMDISK} ]; then
        RAMDISK=${TINYOS_RAMDISK}
    else
        # update rootfs.cpio.gz with new module files
        build_rootfs e $STAGING_DIR/rootfs.cpio.gz $STAGING_DIR/skel >/dev/null

        mkdir -p $STAGING_DIR/skel/lib/modules/${KERNEL_VERSION}

        for e in $(find $STAGING_DIR/lib/modules/$KERNEL_VERSION -name "*.ko"); do
            cp $e $STAGING_DIR/skel/lib/modules/$KERNEL_VERSION
        done

        [ "x${LICHEE_LINUX_DEV}" = "xlongan" ] && longan_modify_init

        build_rootfs c $STAGING_DIR/skel $STAGING_DIR/rootfs.cpio.gz >/dev/null
        rm -rf $STAGING_DIR/skel
    fi

    if [ "${ARCH}" = "arm" ]; then
        KERNEL_OFFSET="0x8000"
    elif [ "${ARCH}" = "arm64" ]; then
        KERNEL_OFFSET="0x80000"
    fi

    if [ -f $STAGING_DIR/bImage ]; then
        kernel_size="`stat $STAGING_DIR/bImage --format="%s"`"
    fi

    if [ -f $BUILD_OUT_DIR/System.map ]; then
        temp=`grep "__bss_start" $BUILD_OUT_DIR/System.map | awk '{print $1}'`
        bss_start=16#${temp: 0-7: 8}
        temp=`grep "__bss_stop" $BUILD_OUT_DIR/System.map | awk '{print $1}'`
        bss_stop=16#${temp: 0-7: 8}
        bss_section_size=$[$bss_stop - $bss_start]
    fi

    DTB_OFFSET=`printf "(%d+%d+%d+%d)/%d*%d\n" $KERNEL_OFFSET $kernel_size $bss_section_size 0x1fffff 0x100000 0x100000 | bc`
    RAMDISK_OFFSET=`printf "%d+%d\n" $DTB_OFFSET 0x100000 | bc`

    local MKBOOTIMG=${LICHEE_TOOLS_DIR}/pack/android/mkbootimg


    if [ "${LICHEE_KERN_SYSTEM}" = "kernel_recovery" ]; then
        IMAGE_NAME="recovery.img"
    else
        IMAGE_NAME="boot.img"
    fi
    ${MKBOOTIMG} --kernel ${BIMAGE} \
        --ramdisk ${RAMDISK} \
        --board ${CHIP}_${ARCH} \
        --base ${BASE} \
        --kernel_offset ${KERNEL_OFFSET} \
        --ramdisk_offset ${RAMDISK_OFFSET} \
        --dtb ${DTB} \
        --dtb_offset ${DTB_OFFSET} \
        --header_version 2 \
        -o $STAGING_DIR/${IMAGE_NAME}

    # If uboot use *bootm* to boot kernel, we should use uImage.
    echo build_ramfs
    echo "Copy ${IMAGE_NAME} to output directory ..."
    cp $STAGING_DIR/${IMAGE_NAME} ${LICHEE_PLAT_OUT}
}

function build_output()
{
    local ramdisk=rootfs.cpio.gz
    local longan_ramdisk="${LICHEE_PLAT_OUT}/ramfs/images/rootfs.cpio.gz"

	if [ "${ARCH}" = "arm" ]; then
		ramdisk=rootfs_arm32.cpio.gz
	elif [ "${ARCH}" = "arm64" ]; then
		ramdisk=rootfs_arm64.cpio.gz
	fi


    # Copy all needed files to staging
    mkdir -p $STAGING_DIR/lib/modules/$KERNEL_VERSION
    (cd $BUILD_OUT_DIR && tar -jcf $STAGING_DIR/vmlinux.tar.bz2 vmlinux)

    [ ! -f $STAGING_DIR/arisc ] && echo "arisc" > $STAGING_DIR/arisc

    build_dts "false"

    local copy_list=(
        $BUILD_OUT_DIR/Module.symvers:$STAGING_DIR/lib/modules/$KERNEL_VERSION/Module.symvers
        $BUILD_OUT_DIR/modules.order:$STAGING_DIR/lib/modules/$KERNEL_VERSION/modules.order
        $BUILD_OUT_DIR/modules.builtin:$STAGING_DIR/lib/modules/$KERNEL_VERSION/modules.builtin
        $BUILD_OUT_DIR/arch/${ARCH}/boot/Image:$STAGING_DIR/bImage
        $BUILD_OUT_DIR/arch/${ARCH}/boot/zImage:$STAGING_DIR/zImage
        $BUILD_OUT_DIR/arch/${ARCH}/boot/uImage:$STAGING_DIR/uImage
        $BUILD_OUT_DIR/arch/${ARCH}/boot/Image.gz:$STAGING_DIR/Image.gz
        $BUILD_OUT_DIR/scripts/dtc/dtc:$LICHEE_PLAT_OUT/dtc
        $BUILD_OUT_DIR/.config:$STAGING_DIR/.config
        $BUILD_OUT_DIR/System.map:$STAGING_DIR/System.map
        $KERNEL_SRC/${ramdisk}:$STAGING_DIR/rootfs.cpio.gz
        $KERNEL_SRC/bsp/ramfs/${ramdisk}:$STAGING_DIR/rootfs.cpio.gz
    )
    # NOTE:
    # '$KERNEL_SRC/${ramdisk}' and '$KERNEL_SRC/bsp/ramfs/${ramdisk}' must not both exist. Therefore,
    # the existence of symlink '$KERNEL_SRC/bsp' is used to avoid an 'if independent_bsp_supported' statement.

    for e in $(find $BUILD_OUT_DIR -name "*.ko"); do
        copy_list+=($e:$STAGING_DIR/lib/modules/$KERNEL_VERSION)
    done

    if [ "x${LICHEE_LINUX_DEV}" = "xlongan" ]; then
        copy_list+=(${longan_ramdisk}:$STAGING_DIR/rootfs.cpio.gz)
    fi

    for e in ${copy_list[@]}; do
        [ -f ${e/:*} ] && cp -f ${e/:*} ${e#*:}
    done

    if [ -n "$(find $STAGING_DIR/lib/modules/$KERNEL_VERSION -name "*.ko")" ]; then
        ${CROSS_COMPILE}strip -d $STAGING_DIR/lib/modules/$KERNEL_VERSION/*.ko
        depmod -b $STAGING_DIR ${KERNEL_VERSION}
    fi

    # Copy file to plat out
    rm -rf ${LICHEE_PLAT_OUT}/lib ${LICHEE_PLAT_OUT}/dist
    cp -rax $STAGING_DIR/. ${LICHEE_PLAT_OUT}
    (cd ${LICHEE_PLAT_OUT} && ln -sf lib/modules/$KERNEL_VERSION dist)

    # Maybe need update modules file to buildroot output
    if [ -d ${LICHEE_BR_OUT}/target ]; then
        echo "Copy modules to target ..."
        local module_dir="${LICHEE_BR_OUT}/target/lib/modules"
        rm -rf ${module_dir}
        mkdir -p ${module_dir}
        cp -rf ${STAGING_DIR}/lib/modules/${KERNEL_VERSION} ${module_dir}
    fi
}

function deal_verity_utils()
{
    local action=$1

    # current not implement, just return
    echo "verity not supported yet"
    [ "$action" == "clean" ] && return 0 || return 0

    # check in the feature when supported
    local blk_size=$1
    local verity_dev=$2
    local ROOTFSTYPE=$3
    local key_file=$4
    local verity_tools_dir="$KERNEL_SRC/verity_tools/32_bit"

    [ "${ARCH}" = "arm64" ] && \
    verity_tools_dir="$KERNEL_SRC/verity_tools/64_bit"

    if [ ! -d ${verity_tools_dir} ]; then
        #no tools, obviously dont need to clean, do nothing
        return 0
    fi

    build_rootfs e $STAGING_DIR/rootfs.cpio.gz $STAGING_DIR/skel >/dev/null

    if [ "$action" == "clean" ]; then
        rm -rf ${STAGING_DIR}/skel/verity_key ${STAGING_DIR}/skel/verityInfo
    else
        echo "blk_size="${blk_size}      > ${STAGING_DIR}/skel/verityInfo
        echo "verity_dev="${verity_dev} >> ${STAGING_DIR}/skel/verityInfo
        echo "ROOTFSTYPE="${ROOTFSTYPE} >> ${STAGING_DIR}/skel/verityInfo

        if [ ! -f ${STAGING_DIR}/skel/usr/bin/openssl ]; then
            if [ -f ${verity_tools_dir}/openssl ]; then
                echo "ramfs do not have openssl bin, use a prebuild one"
                cp ${verity_tools_dir}/openssl ${STAGING_DIR}/skel/usr/bin
            else
                echo "no avaliable openssl found, can not enable dm-verity"
                return 2
            fi
        fi

        if [ ! -f ${STAGING_DIR}/skel/usr/sbin/veritysetup ]; then
            if [ -f ${verity_tools_dir}/veritysetup ]; then
                echo "ramfs do not have veritysetup bin, use a prebuild one"
                cp ${verity_tools_dir}/veritysetup ${STAGING_DIR}/skel/usr/sbin
            else
                echo "no avaliable veritysetup found, can not enable dm-verity"
                return 2
            fi
        fi
        cp ${key_file} ${STAGING_DIR}/skel/verity_key
    fi

    build_rootfs c $STAGING_DIR/skel $STAGING_DIR/rootfs.cpio.gz >/dev/null
    rm -rf ${STAGING_DIR}/skel1 ${STAGING_DIR}/skel

    build_ramfs
    return 0
}

function build_main()
{
    case "$1" in
        kernel)
            build_kernel
            build_output
            build_ramfs
            ;;
        modules)
            build_modules
            build_output
            build_ramfs
            ;;
        clean|distclean)
            clean_modules
            clean_kernel "$1"
            ;;
        deal_verity)
            shift
            deal_verity_utils $@
            ;;
        dts)
            build_dts
            ;;
        *)
            build_kernel
            build_modules
            build_output
            build_ramfs
            echo -e "\n\033[0;31;1m${LICHEE_CHIP} compile Kernel successful\033[0m\n\n"
            ;;
    esac
}

build_setup $@

build_main $@

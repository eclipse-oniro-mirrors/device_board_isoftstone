#!/bin/bash
# Copyright (c) 2021 Huawei Device Co., Ltd.
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

kernel_src_path=${1}
kernel_patch_path=${2}

patch_list=(
    arch.patch
    others.patch
    kernel.patch
    scripts.patch
    Documentation.patch
    sound.patch
    drivers.patch
    net.patch
    include.patch
)

delete_list=(
    drivers/devfreq/rk3399_dmc.c
    drivers/dma-buf/heaps/heap-helpers.c
    drivers/dma-buf/heaps/heap-helpers.h
)

copy_list=(
    arch/dts/*                              arch/arm64/boot/dts/rockchip/
    driv/char/*                             drivers/char/
    driv/clk/*                              drivers/clk/rockchip/
    driv/cpufreq/*                          drivers/cpufreq/
    driv/devfreq/*                          drivers/devfreq/
    driv/devfreq/event/*                    drivers/devfreq/event/
    driv/dma-buf/*                          drivers/dma-buf/heaps/
    driv/gpu/bridge/rk*                     drivers/gpu/drm/bridge/
    driv/gpu/bridge/dw*                     drivers/gpu/drm/bridge/synopsys/
    driv/gpu/rockchip/*                     drivers/gpu/drm/rockchip/
    driv/input/remotectl                    drivers/input/
    driv/input/touchscreen/*                drivers/input/touchscreen/
    driv/media/i2c/*                        drivers/media/i2c/
    driv/media/cif                          drivers/media/platform/rockchip/
    driv/media/isp                          drivers/media/platform/rockchip/
    driv/media/ispp                         drivers/media/platform/rockchip/
    driv/net/rockchip_wlan                  drivers/net/wireless/
    driv/net/stmmac/*                       drivers/net/ethernet/stmicro/stmmac/
    driv/phy/*                              drivers/phy/rockchip/
    driv/soc/*                              drivers/soc/rockchip/
    driv/video/rockchip                     drivers/video/
    incl/drm/*                              include/drm/
    incl/dt-bindings/clock/*                include/dt-bindings/clock/
    incl/dt-bindings/current/*              include/dt-bindings/
    incl/dt-bindings/display/*              include/dt-bindings/display/
    incl/dt-bindings/input/*                include/dt-bindings/input/
    incl/dt-bindings/memory/*               include/dt-bindings/memory/
    incl/dt-bindings/phy/*                  include/dt-bindings/phy/
    incl/dt-bindings/soc/*                  include/dt-bindings/soc/
    incl/linux/clk/*                        include/linux/clk/
    incl/linux/current/*                    include/linux/
    incl/linux/phy/*                        include/linux/phy/
    incl/linux/power/*                      include/linux/power/
    incl/linux/rockchip                     include/linux/soc/
    incl/linux/usb/*                        include/linux/usb/
    incl/media/*                            include/media/
    incl/soc/*                              include/soc/rockchip/
    incl/treace/events/*                    include/trace/events/
    # incl/treace/hooks                       include/trace/
    incl/uapi/drm/*                         include/uapi/drm/
    incl/uapi/linux/*                       include/uapi/linux/
    incl/uapi/misc/*                        include/uapi/misc/
    _net/*                                  net/rfkill/
    scri/*                                  scripts/
    soun/codecs/*                           sound/soc/codecs/
    soun/rockchip/*                         sound/soc/rockchip/
)

function copy_kernel_files()
{
    for ((i=0; i<${#copy_list[*]}; i+=2))
    do
        cp -arf ${kernel_src_path}/${copy_list[$i]} ${copy_list[$(expr $i+1)]}
        echo "copy  ${copy_list[$i]}  to ${copy_list[$(expr $i+1)]} success"
    done
}

function delete_kernel_files()
{
    for ((i=0; i<${#delete_list[*]}; i++))
    do
        rm -f ${delete_list[$i]}
        echo "delete ${delete_list[$i]} success"
    done
}

function apply_rk3399_kernel_patch()
{
    for ((i = 0; i < ${#patch_list[*]}; i++))
    do
        patch -p1 < ${kernel_patch_path}/${patch_list[$i]}
        echo "patch for ${patch_list[$i]} success."
    done
}

copy_kernel_files
delete_kernel_files
apply_rk3399_kernel_patch

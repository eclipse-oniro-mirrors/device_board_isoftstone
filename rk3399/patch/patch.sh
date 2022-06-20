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
path_patch=$(cd "$(dirname "$0")";pwd)
echo "++++++++++++++++++++++++++++++++++++++++"
date +%F' '%H:%M:%S
echo $@

copy_list=(
    #device/rockchip/product/device_rk3399.json          productdefine/common/device/rk3399.json
	device/board/isoftstone/rk3399/patch/ohos_config.json  .
)
function cooy_rk3399_files()
{
    for ((i=0; i<${#copy_list[*]}; i+=2))
    do
        cp -arf ${copy_list[$i]} ${copy_list[$(expr $i+1)]}
        echo "copy  ${copy_list[$i]}  to ${copy_list[$(expr $i+1)]} success"
    done
}

patch_list=(
    foundation/communication/wifi                    foundation_communication_wifi.patch
    base/startup/init_lite                           base_startup_initlite.patch
)

function apply_rk3399_patch()
{
    for ((i=0; i<${#patch_list[*]}; i+=2))
    do
        repo forall -r ^${patch_list[$i]}$ -c git apply $path_patch/${patch_list[$(expr $i+1)]}
        echo "patch for ${patch_list[$i]} success"
    done
}

cooy_rk3399_files
apply_rk3399_patch

#ln -s device/rockchip/rk3399/kernel/make_kernel.sh make_kernel+hdf.sh
echo "++++++++++++++++++++++++++++++++++++++++"
#echo "using make_kernel+hdf.sh to build kernel+hdf"
#echo "++++++++++++++++++++++++++++++++++++++++"


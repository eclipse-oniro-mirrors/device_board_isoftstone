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

pushd ${1}

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

function apply_rk3399_kernel_patch()
{
    for ((i = 0; i < ${#patch_list[*]}; i++))
    do
        git apply ${kernel_patch_path}/${patch_list[$i]}
        echo "patch for ${patch_list[$i]} success."
    done
}

apply_rk3399_kernel_patch

popd

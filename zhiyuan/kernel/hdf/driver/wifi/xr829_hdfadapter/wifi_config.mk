#
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
#

WIFI_CONFIG_PATH := $(shell pwd)
$(warning WIFI_CONFIG_PATH=$(WIFI_CONFIG_PATH))

HDF_FRAMEWORKS_INC := \
    -I./ \
    -Idrivers/hdf/framework/ability/sbuf/include \
    -Idrivers/hdf/framework/core/common/include/host \
    -Idrivers/hdf/framework/core/host/include \
    -Idrivers/hdf/framework/core/manager/include \
    -Idrivers/hdf/framework/core/shared/include \
    -Idrivers/hdf/framework/include \
    -Idrivers/hdf/framework/include/config \
    -Idrivers/hdf/framework/include/core \
    -Idrivers/hdf/framework/include/platform \
    -Idrivers/hdf/framework/include/utils \
    -Idrivers/hdf/framework/support/platform/include \
    -Idrivers/hdf/framework/support/platform/include/platform \
    -Idrivers/hdf/framework/utils/include \
    -Idrivers/hdf/khdf/osal/include \
    -Idrivers/hdf/khdf/config/include \
    -Iinclude/hdf \
    -Iinclude/hdf/osal \
    -Iinclude/hdf/utils \
    -Idrivers/hdf/framework/include/ethernet\
    -Idrivers/hdf/framework/include/net\
    -Idrivers/hdf/framework/model/network/common/netdevice\
    -Idrivers/hdf/framework/include/wifi\
    -Idrivers/hdf/framework/model/network/wifi/platform/include \
    -Idrivers/hdf/framework/model/network/wifi/core/components/eapol



HDF_WIFI_FRAMEWORKS_INC := \
    -Idrivers/hdf/framework/model/network/ethernet/include \
    -Idrivers/hdf/framework/model/network/wifi/include

SECURE_LIB_INC := \
    -I./../../../../../third_party/bounds_checking_function/include

HDF_WIFI_ADAPTER_INC := \
    -Idrivers/hdf/khdf/network/include

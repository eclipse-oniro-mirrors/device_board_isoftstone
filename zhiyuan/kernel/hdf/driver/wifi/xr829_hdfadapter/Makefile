#
# Copyright (c) 2021 VYAGOO TECHNOLOGY Co., Ltd.
#
# This software is licensed under the terms of the GNU General Public
# License version 2, as published by the Free Software Foundation, and
# may be copied, distributed, and modified under those terms.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
#

MODULE_NAME := xr829_hdfadapter

VENDOR_WIFI_ROOT = .

WIFI_PATH := $(shell pwd)
$(warning WIFI_PATH=$(WIFI_PATH))

include $(WIFI_PATH)/../../../../../device/board/seed/t507_pines/kernel/hdf/driver/wifi/xr829_hdfadapter/wifi_config.mk

$(warning HDF_FRAMEWORKS_INC=$(HDF_FRAMEWORKS_INC))
$(warning HDF_WIFI_FRAMEWORKS_INC=$(HDF_WIFI_FRAMEWORKS_INC))
$(warning SECURE_LIB_INC=$(SECURE_LIB_INC))
$(warning HDF_WIFI_ADAPTER_INC=$(HDF_WIFI_ADAPTER_INC))

obj-y += $(MODULE_NAME).o
$(MODULE_NAME)-objs := $(VENDOR_WIFI_ROOT)/hdf_driver_register.o \
		 $(VENDOR_WIFI_ROOT)/hdfinit_xr829.o \
         $(VENDOR_WIFI_ROOT)/net_adpater.o \
         $(VENDOR_WIFI_ROOT)/hdf_mac80211.o

ccflags-y += \
    $(HDF_FRAMEWORKS_INC) \
    $(HDF_WIFI_FRAMEWORKS_INC) \
    $(SECURE_LIB_INC) \
	$(HDF_WIFI_ADAPTER_INC)

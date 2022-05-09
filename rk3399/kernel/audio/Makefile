#
# Copyright (c) 2021 Huawei Device Co., Ltd.
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
KHDF_AUDIO_BASE_ROOT_DIR = ../../../../../../../..
KHDF_AUDIO_RK3399_DIR = $(KHDF_AUDIO_BASE_ROOT_DIR)/device/rockchip/rk3399/audio

KHDF_AUDIO_KHDF_ROOT_DIR = drivers/hdf/khdf
KHDF_FRAMEWORK_ROOT_DIR = drivers/hdf/framework
KHDF_AUDIO_RK3399_INC_DIR = $(KHDF_FRAMEWORK_ROOT_DIR)/../../device/rockchip/rk3399/audio

obj-$(CONFIG_DRIVERS_HDF_AUDIO_RK3399) += \
          $(KHDF_AUDIO_RK3399_DIR)/soc/src/rk3399_dma_ops.o \
          $(KHDF_AUDIO_RK3399_DIR)/soc/src/rk3399_dma_adapter.o \
          $(KHDF_AUDIO_RK3399_DIR)/dai/src/rk3399_dai_adapter.o \
          $(KHDF_AUDIO_RK3399_DIR)/dai/src/rk3399_dai_ops.o \
          $(KHDF_AUDIO_RK3399_DIR)/accessory/es8316/src/es8316_accessory_adapter.o \
          $(KHDF_AUDIO_RK3399_DIR)/accessory/es8316/src/es8316_accessory_impl.o

ccflags-$(CONFIG_DRIVERS_HDF_AUDIO_RK3399) +=  -lm -lc -lgcc -std=gnu99 -Werror \
             -I$(srctree)/$(KHDF_AUDIO_KHDF_ROOT_DIR)/osal/include \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/include/core \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/include/utils \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/include/osal \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/include/platform \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/include/audio \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/ability/sbuf/include \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/core/common/include/host \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/model/audio/core/include \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/model/audio/sapm/include \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/model/audio/dispatch/include \
             -I$(srctree)/$(KHDF_FRAMEWORK_ROOT_DIR)/model/audio/common/include \
             -I$(srctree)/bounds_checking_function/include \
             -I$(srctree)/$(KHDF_AUDIO_RK3399_INC_DIR)/soc/include \
             -I$(srctree)/$(KHDF_AUDIO_RK3399_INC_DIR)/dai/include \
             -I$(srctree)/$(KHDF_AUDIO_RK3399_INC_DIR)/accessory/es8316/include
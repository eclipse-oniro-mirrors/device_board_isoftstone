/*
 * Copyright (C) 2022 VYAGOO TECHNOLOGY Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ac107_accessory_impl_linux.h"
// #include "audio_accessory_base.h"
// #include "audio_accessory_if.h"
#include "audio_driver_log.h"
#include "audio_platform_base.h"
#include "audio_dai_if.h"

#define HDF_LOG_TAG "accessory"

struct AccessoryData g_Ac107Data = {
    .Init   = Ac107DeviceInit,
    .Read   = Ac107DeviceReadReg,
    .Write  = Ac107DeviceWriteReg,
};

struct AudioDaiOps g_Ac107DaiDeviceOps = {
    .Startup    = Ac107DaiStartup,
    .HwParams   = Ac107DaiHwParams,
    .Trigger    = Ac107DaiTrigger,
};

struct DaiData g_Ac107DaiData = {
    .drvDaiName = "accessory_dai",
    .DaiInit    = Ac107DaiDeviceInit,
    .ops        = &g_Ac107DaiDeviceOps,
};

/* HdfDriverEntry */
static int32_t GetServiceName(const struct HdfDeviceObject *device)
{
    const struct DeviceResourceNode *node = NULL;
    struct DeviceResourceIface *drsOps = NULL;
    int32_t ret;

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("input HdfDeviceObject object is nullptr.");
        return HDF_FAILURE;
    }
    node = device->property;
    if (node == NULL) {
        AUDIO_DRIVER_LOG_ERR("get drs node is nullptr.");
        return HDF_FAILURE;
    }
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetString == NULL) {
        AUDIO_DRIVER_LOG_ERR("drsOps or drsOps getString is null!");
        return HDF_FAILURE;
    }
    ret = drsOps->GetString(node, "serviceName", &g_Ac107Data.drvAccessoryName, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("read serviceName failed.");
        return ret;
    }

    return HDF_SUCCESS;
}

/* HdfDriverEntry implementations */
static int32_t Ac107DriverBind(struct HdfDeviceObject *device)
{
    (void)device;
    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static int32_t Ac107DriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = AccessoryGetConfigInfo(device, &g_Ac107Data);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("get config info failed.");
        return ret;
    }

    ret = GetServiceName(device);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("GetServiceName failed.");
        return ret;
    }

    ret = AudioRegisterAccessory(device, &g_Ac107Data, &g_Ac107DaiData);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioRegisterAccessory failed.");
        return ret;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_Ac107DriverEntry = {
    .moduleVersion  = 1,
    .moduleName     = "CODEC_AC107",
    .Bind           = Ac107DriverBind,
    .Init           = Ac107DriverInit,
    .Release        = NULL,
};
HDF_INIT(g_Ac107DriverEntry);

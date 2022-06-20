/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "es8316_accessory_impl.h"
#include "audio_codec_if.h"
#include "audio_codec_base.h"
#include "audio_driver_log.h"

#define HDF_LOG_TAG "es8316_codec_adapter"

struct CodecData g_es8316Data = {
    .Init = Es8316DeviceInit,
    .Read = Es8316DeviceRegRead,
    .Write = Es8316DeviceRegWrite,
};

struct AudioDaiOps g_es8316DaiDeviceOps = {
    .Startup = Es8316DaiStartup,
    .HwParams = Es8316DaiHwParams,
};

struct DaiData g_es8316DaiData = {
    .drvDaiName = "codec_dai",
    .DaiInit = Es8316DaiDeviceInit,
    .ops = &g_es8316DaiDeviceOps,
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
    ret = drsOps->GetString(node, "serviceName", &g_es8316Data.drvCodecName, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("read serviceName failed.");
        return ret;
    }
    return HDF_SUCCESS;
}

/* HdfDriverEntry implementations */
static int32_t Es8316DriverBind(struct HdfDeviceObject *device)
{
    (void)device;
    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static int32_t Es8316DriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = Es8316GetConfigInfo(device, &g_es8316Data);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8316GetConfigInfo failed.");
        return ret;
    }

    ret = GetServiceName(device);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("GetServiceName failed.");
        return ret;
    }

    ret = AudioRegisterCodec(device, &g_es8316Data, &g_es8316DaiData);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioRegisterCodec failed.");
        return ret;
    }
    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_es8316DriverEntry = {
    .moduleVersion = 1,
    .moduleName = "CODEC_ES8316",
    .Bind = Es8316DriverBind,
    .Init = Es8316DriverInit,
    .Release = NULL,
};
HDF_INIT(g_es8316DriverEntry);

/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "t507_codec_ops.h"
#include "audio_codec_base.h"
#include "audio_core.h"
#include "audio_driver_log.h"

#define HDF_LOG_TAG t507_codec_adapter

struct CodecData g_codecData = {
    .Init   = T507CodecDeviceInit,
    .Read   = T507CodecDeviceReadReg,
    .Write  = T507CodecDeviceWriteReg,
};

struct AudioDaiOps g_codecDaiDeviceOps = {
    .Startup    = T507CodecDaiStartup,
    .HwParams   = T507CodecDaiHwParams,
    .Trigger    = T507CodecDaiTrigger,
};

struct DaiData g_codecDaiData = {
    .DaiInit    = T507CodecDaiDeviceInit,
    .ops        = &g_codecDaiDeviceOps,
};

/* HdfDriverEntry implementations */
static int32_t CodecDriverBind(struct HdfDeviceObject *device)
{
    struct CodecHost *codecHost;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }

    codecHost = (struct CodecHost *)OsalMemCalloc(sizeof(*codecHost));
    if (codecHost == NULL) {
        AUDIO_DRIVER_LOG_ERR("malloc codecHost fail!");
        return HDF_FAILURE;
    }
    codecHost->device = device;
    device->service = &codecHost->service;

    AUDIO_DRIVER_LOG_DEBUG("success!");

    return HDF_SUCCESS;
}

static int32_t CodecDriverInit(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (CodecGetConfigInfo(device, &g_codecData) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (CodecSetConfigInfoOfControls(&g_codecData, &g_codecDaiData) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (CodecGetServiceName(device, &g_codecData.drvCodecName) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (CodecGetDaiName(device, &g_codecDaiData.drvDaiName) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    if (AudioRegisterCodec(device, &g_codecData, &g_codecDaiData) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void CodecDriverRelease(struct HdfDeviceObject *device)
{
    struct CodecHost *codecHost;

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL");
        return;
    }

    if (device->priv != NULL) {
        OsalMemFree(device->priv);
    }
    codecHost = (struct CodecHost *)device->service;
    if (codecHost == NULL) {
        HDF_LOGE("CodecDriverRelease: codecHost is NULL");
        return;
    }
    OsalMemFree(codecHost);
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_codecDriverEntry = {
    .moduleVersion  = 1,
    .moduleName     = "CODEC_T507",
    .Bind           = CodecDriverBind,
    .Init           = CodecDriverInit,
    .Release        = CodecDriverRelease,
};
HDF_INIT(g_codecDriverEntry);

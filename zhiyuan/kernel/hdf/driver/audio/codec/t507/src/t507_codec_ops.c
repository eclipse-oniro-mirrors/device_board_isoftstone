/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "audio_sapm.h"
#include "audio_platform_base.h"
#include "t507_codec_impl_linux.h"
#include "audio_driver_log.h"
#include "audio_codec_base.h"
#include "audio_stream_dispatch.h"

#include "t507_codec_ops.h"

#define HDF_LOG_TAG t507_codec_ops

static const struct AudioSapmRoute g_audioRoutes[] = {
    { "SPKL", "Dacl enable", "DACL"},
    { "SPKR", "Dacr enable", "DACR"},

    { "ADCL", NULL, "LPGA"},
    { "LPGA", "LPGA MIC Switch", "MIC"},

    { "ADCR", NULL, "RPGA"},
    { "RPGA", "RPGA MIC Switch", "MIC"},
};

int32_t T507CodecDeviceInit(struct AudioCard *audioCard, const struct CodecDevice *codec)
{
    if (audioCard == NULL || codec == NULL || codec->devData == NULL ||
        codec->devData->sapmComponents == NULL || codec->devData->controls == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (CodecSetCtlFunc(codec->devData, T507CodecImplGetCtrlOps, T507CodecImplSetCtrlOps) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioCodecSetCtlFunc failed.");
        return HDF_FAILURE;
    }

    /* audiocodec IoRemap at t507_codec_impl_linux.c */

    if (T507CodecImplRegDefaultInit(codec->devData->regCfgGroup) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("CodecRegDefaultInit failed.");
        return HDF_FAILURE;
    }

    if (AudioAddControls(audioCard, codec->devData->controls, codec->devData->numControls) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add controls failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmNewComponents(audioCard, codec->devData->sapmComponents,
        codec->devData->numSapmComponent) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("new components failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmAddRoutes(audioCard, g_audioRoutes, HDF_ARRAY_SIZE(g_audioRoutes)) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add route failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmNewControls(audioCard) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add sapm controls failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmSleep(audioCard) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add sapm sleep modular failed.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t T507CodecDeviceReadReg(const struct CodecDevice *codec, uint32_t reg, uint32_t *value)
{
    (void)codec;

    if (value == NULL) {
        AUDIO_DRIVER_LOG_ERR("param value is null.");
        return HDF_FAILURE;
    }

    T507CodecImplRegmapRead(reg, value);
    return HDF_SUCCESS;
}

int32_t T507CodecDeviceWriteReg(const struct CodecDevice *codec, uint32_t reg, uint32_t value)
{
    (void)codec;

    T507CodecImplRegmapWrite(reg, value);
    return HDF_SUCCESS;
}

int32_t T507CodecDaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (device == NULL || device->devDaiName == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("codec dai device name: %s\n", device->devDaiName);
    (void)card;
    return HDF_SUCCESS;
}

int32_t T507CodecDaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param)
{
    int ret;
    AUDIO_DRIVER_LOG_DEBUG("");

    if (param == NULL || param->cardServiceName == NULL || card == NULL ||
        card->rtd == NULL || card->rtd->codecDai == NULL || card->rtd->codecDai->devData == NULL ||
        card->rtd->codecDai->devData->regCfgGroup == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }

    ret = T507CodecImplHwParams(param->streamType, param->format, param->channels, param->rate);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t T507CodecDaiStartup(const struct AudioCard *card, const struct DaiDevice *device)
{
    int32_t ret;

    AUDIO_DRIVER_LOG_DEBUG("");

    (void)card;
    (void)device;

    ret = T507CodecImplStartup(AUDIO_RENDER_STREAM);    /* unuse */
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t T507CodecDaiTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *device)
{
    int ret;

    AUDIO_DRIVER_LOG_DEBUG(" cmd -> %d", cmd);

    (void)card;
    (void)device;

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
            ret = T507CodecImplTrigger(AUDIO_RENDER_STREAM, true);
            if (ret != HDF_SUCCESS) {
                return HDF_FAILURE;
            }
            break;
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
            ret = T507CodecImplTrigger(AUDIO_RENDER_STREAM, false);
            if (ret != HDF_SUCCESS) {
                return HDF_FAILURE;
            }
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
            ret = T507CodecImplTrigger(AUDIO_CAPTURE_STREAM, true);
            if (ret != HDF_SUCCESS) {
                return HDF_FAILURE;
            }
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            ret = T507CodecImplTrigger(AUDIO_CAPTURE_STREAM, false);
            if (ret != HDF_SUCCESS) {
                return HDF_FAILURE;
            }
            break;
        default:
            break;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

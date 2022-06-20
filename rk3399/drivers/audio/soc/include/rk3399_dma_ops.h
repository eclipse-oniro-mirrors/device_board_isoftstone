/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef RK3399_PLATFORM_OPS_H
#define RK3399_PLATFORM_OPS_H

#include "audio_core.h"
#include <linux/dmaengine.h>
#include <linux/dma/sprd-dma.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

struct SprdDmaCfg {
    struct dma_slave_config config;
    unsigned long  dma_config_flag;
    u32 transcation_len;
    u32 datawidth;
    u32 src_step;
    u32 des_step;
    u32 fragmens_len;
    u32 sg_num;
    struct scatterlist *sg;
};

int32_t AudioDmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform);

int32_t RK3399DmaBufAlloc(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t RK3399DmaBufFree(struct PlatformData *data, const enum AudioStreamType streamType);

int32_t RK3399DmaRequestChannel(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t RK3399DmaConfigChannel(const struct PlatformData *data, const enum AudioStreamType streamType);

int32_t RK3399PcmPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer);

int32_t RK3399DmaPrep(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t RK3399DmaSubmit(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t RK3399DmaPending(struct PlatformData *data, const enum AudioStreamType streamType);

int32_t RK3399DmaPause(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t RK3399DmaResume(const struct PlatformData *data, const enum AudioStreamType streamType);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* DAYU_CODEC_OPS_H */

/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef T507_DMA_OPS_H
#define T507_DMA_OPS_H

#include <linux/dmaengine.h>
#include "audio_core.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define SUNXI_CODEC_ADDR_BASE       0x05096000
#define SUNXI_DAC_TXDATA            0X20

#define SUNXI_AHUB_ADDR_BASE        0x05097000
#define SUNXI_AHUB_APBIF_RXFIFO(n)  (0x120 + ((n) * 0x30))

#define AHUB_APBIF_0    0
#define AHUB_APBIF_1    1
#define AHUB_APBIF_2    2
#define AHUB_I2S_0      0
#define AHUB_I2S_1      1
#define AHUB_I2S_2      2
#define AHUB_I2S_3      3
#define AHUB_APBIF_USE  AHUB_APBIF_0
#define AHUB_I2S_USE    AHUB_I2S_0

int32_t T507AudioDmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform);
int32_t T507AudioDmaBufAlloc(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaBufFree(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaRequestChannel(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaConfigChannel(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaPrep(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaSubmit(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaPending(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaPause(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaResume(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t T507AudioDmaPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* T507_DMA_OPS_H */

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

/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef T507_DAI_OPS_H
#define T507_DAI_OPS_H

#include "audio_core.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

int32_t T507AhubDeviceInit(struct AudioCard *audioCard, const struct DaiDevice *dai);
int32_t T507AhubDeviceReadReg(const struct DaiDevice *dai, uint32_t reg, uint32_t *value);
int32_t T507AhubDeviceWriteReg(const struct DaiDevice *dai, uint32_t reg, uint32_t value);

int32_t T507AhubDaiStartup(const struct AudioCard *card, const struct DaiDevice *device);
int32_t T507AhubDaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param);
int32_t T507AhubDaiTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *device);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* T507_CODEC_OPS_H */

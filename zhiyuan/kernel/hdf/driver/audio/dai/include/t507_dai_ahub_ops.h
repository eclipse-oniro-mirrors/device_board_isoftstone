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

#ifndef T507_DAI_OPS_H
#define T507_DAI_OPS_H

#include "audio_core.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

int32_t T507AhubDeviceInit(struct AudioCard *audioCard, const struct DaiDevice *dai);
int32_t T507AhubDeviceReadReg(unsigned long virtualAddress, uint32_t reg, uint32_t *value);
int32_t T507AhubDeviceWriteReg(unsigned long virtualAddress, uint32_t reg, uint32_t value);

int32_t T507AhubDaiStartup(const struct AudioCard *card, const struct DaiDevice *device);
int32_t T507AhubDaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param);
int32_t T507AhubDaiTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *device);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* T507_CODEC_OPS_H */

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

#ifndef DSP_OPS_H
#define DSP_OPS_H

#include "audio_host.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

int32_t DspDaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device);
int32_t DspDeviceInit(const struct DspDevice *device);
int32_t DspDeviceReadReg(const struct DspDevice *device, const void *msgs, const uint32_t len);
int32_t DspDeviceWriteReg(const struct DspDevice *device, const void *msgs, const uint32_t len);
int32_t DspDaiStartup(const struct AudioCard *card, const struct DaiDevice *device);
int32_t DspDaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param);
int32_t DspDecodeAudioStream(const struct AudioCard *card, const uint8_t *buf, const struct DspDevice *device);
int32_t DspEncodeAudioStream(const struct AudioCard *card, const uint8_t *buf, const struct DspDevice *device);
int32_t DspEqualizerActive(const struct AudioCard *card, const uint8_t *buf, const struct DspDevice *device);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* DSP_OPS_H */

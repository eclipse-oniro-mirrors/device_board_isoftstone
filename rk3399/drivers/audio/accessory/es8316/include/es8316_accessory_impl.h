/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef ES8316_CODEC_IMPL_H
#define ES8316_CODEC_IMPL_H

#include "audio_codec_if.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "osal_io.h"
#include "securec.h"
#include <linux/types.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

enum Es8316I2sFormatRegVal {
    ES8316_I2S_SAMPLE_FORMAT_REG_VAL_24 = 0x0,    /*  24-bit serial audio data word length(default) */
    ES8316_I2S_SAMPLE_FORMAT_REG_VAL_20 = 0x1,    /*  20-bit serial audio data word length */
    ES8316_I2S_SAMPLE_FORMAT_REG_VAL_16 = 0x2,    /*  18-bit serial audio data word length */
    ES8316_I2S_SAMPLE_FORMAT_REG_VAL_18 = 0x3,    /*  16-bit serial audio data word length */
    ES8316_I2S_SAMPLE_FORMAT_REG_VAL_32 = 0x4,    /*  32-bit serial audio data word length */
};

/**
 The following enum values correspond to the location of the configuration parameters in the HCS file.
 If you modify the configuration parameters, you need to modify this value.
*/
enum Es8316DaiHwParamsIndex {
    ES8316_DHP_RENDER_FREQUENCY_INX = 0,
    ES8316_DHP_RENDER_FORMAT_INX = 1,
    ES8316_DHP_RENDER_CHANNEL_INX = 2,
    ES8316_DHP_CAPTURE_FREQUENCY_INX = 3,
    ES8316_DHP_CAPTURE_FORMAT_INX = 4,
    ES8316_DHP_CAPTURE_CHANNEL_INX = 5,
};

struct Es8316DaiHwParamsTransferData {
    uint8_t inputParamsBeginIndex;
    uint8_t inputParamsEndIndex;
    uint8_t otherParamsBeginIndex;
    uint8_t otherParamsEndIndex;
    uint8_t daiHwParamsRegCfgItemCount;
};

/* Original accessory base declare */
enum Es8316I2sFrequency {
    ES8316_I2S_SAMPLE_FREQUENCY_8000  = 8000,    /* 8kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_11025 = 11025,   /* 11.025kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_12000 = 12000,   /* 12kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_16000 = 16000,   /* 16kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_22050 = 22050,   /* 22.050kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_24000 = 24000,   /* 24kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_32000 = 32000,   /* 32kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_44100 = 44100,   /* 44.1kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_48000 = 48000,   /* 48kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_64000 = 64000,   /* 64kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_88200 = 88200,   /* 88.2kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_96000 = 96000    /* 96kHz sample_rate */
};

enum Es8316I2sFrequencyRegVal {
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_8000  = 0x0,   /* 8kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_11025 = 0x1,   /* 11.025kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_12000 = 0x2,   /* 12kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_16000 = 0x3,   /* 16kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_22050 = 0x4,   /* 22.050kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_24000 = 0x5,   /* 24kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_32000 = 0x6,   /* 32kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_44100 = 0x7,   /* 44.1kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_48000 = 0x8,   /* 48kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_64000 = 0x9,   /* 64kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_88200 = 0xA,   /* 88.2kHz sample_rate */
    ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_96000 = 0xB    /* 96kHz sample_rate */
};

struct Es8316TransferData {
    uint32_t codecCfgCtrlCount;
    struct AudioRegCfgGroupNode **codecRegCfgGroupNode;
    struct AudioKcontrol *codecControls;
};

struct Es8316DaiParamsVal {
    uint32_t frequencyVal;
    uint32_t formatVal;
    uint32_t channelVal;
};

int32_t Es8316DeviceRegRead(unsigned long virtualAddress, uint32_t reg, uint32_t *value);
int32_t Es8316DeviceRegWrite(unsigned long virtualAddress, uint32_t reg, uint32_t value);
int32_t Es8316GetConfigInfo(const struct HdfDeviceObject *device, struct CodecData *codecData);

/* Original es8316 declare */
int32_t Es8316DeviceInit(struct AudioCard *audioCard, const struct CodecDevice *device);
int32_t Es8316DaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device);
int32_t Es8316DaiStartup(const struct AudioCard *card, const struct DaiDevice *device);
int32_t Es8316DaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

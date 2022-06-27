/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "es8316_accessory_impl.h"
#include "audio_control.h"
#include "audio_core.h"
#include "audio_host.h"
#include "audio_parse.h"
#include "audio_platform_base.h"
#include "audio_sapm.h"
#include "audio_driver_log.h"
#include "i2c_if.h"
#include "gpio_if.h"
#include "linux/of_gpio.h"

#define HDF_LOG_TAG "es8316_codec"

#define RK3399_I2C_BUS_NUMBER      (1)    // i2c1(master)
/* ES8316 I2C Device address 0 0 1 0 0 0 AD0 R/W */
#define ES8316_I2C_DEV_ADDR        (0x11) // 0010000 (slave)
#define ES8316_I2C_REG_DATA_LEN    (1)    // reg value length is 8 bits

#define ES8316_I2C_WAIT_TIMES      (10)   // ms

#define RK3399_GPIO_NUMBER         (11)   // gpio0_b3;

/* Original accessory_base implements */
#define ES8316_COMM_SHIFT_8BIT     (8)
#define ES8316_COMM_MASK_FF        (0xFF)

#define ES8316_I2C_REG_SIZE        (1)
#define ES8316_I2C_MSG_NUM         (2)
#define ES8316_I2C_MSG_BUF_SIZE    (2)

#define NUM_NEG_SIXTEEN            (-16)

struct AudioRegCfgGroupNode **g_es8316RegCfgGroupNode = NULL;
struct AudioKcontrol *g_es8316Controls = NULL;
struct Es8316TransferData g_es8316TransferData;

static const char *g_audioControlsList[AUDIO_CTRL_LIST_MAX] = {
    "Main Playback Volume", "Main Capture Volume",
    "Playback Mute", "Capture Mute", "Mic Left Gain",
    "Mic Right Gain", "External Codec Enable",
    "Internally Codec Enable", "Render Channel Mode", "Captrue Channel Mode"
};


/*
 * release I2C object public function
 */
static void Es8316I2cRelease(struct I2cMsg *msgs, int16_t msgSize, DevHandle i2cHandle)
{
    if (msgs != NULL) {
        if (msgSize == 0 && msgs->buf != NULL) {
            OsalMemFree(msgs->buf);
            msgs->buf = NULL;
        } else if (msgSize == 1 && msgs[0].buf != NULL) {
            OsalMemFree(msgs[0].buf);
            msgs[0].buf = NULL;
        } else if (msgSize >= ES8316_I2C_MSG_NUM) {
            if (msgs[0].buf != NULL) {
                msgs[0].buf = NULL;
            }
            if (msgs[1].buf != NULL) {
                OsalMemFree(msgs[1].buf);
                msgs[1].buf = NULL;
            }
        }
        AUDIO_DRIVER_LOG_DEBUG("OsalMemFree msgBuf success.\n");
    }
    // close i2c device
    if (i2cHandle != NULL) {
        I2cClose(i2cHandle);
        i2cHandle = NULL;
        AUDIO_DRIVER_LOG_DEBUG("I2cClose success.\n");
    }
}

static int32_t Es8316I2cMsgFill(const struct AudioAddrConfig *regAttr, uint16_t rwFlag,
    uint8_t *regs, struct I2cMsg *msgs)
{
    uint8_t *msgBuf = NULL;
    if (rwFlag != 0 && rwFlag != I2C_FLAG_READ) {
        AUDIO_DRIVER_LOG_ERR("invalid rwFlag value: %d.", rwFlag);
        return HDF_ERR_INVALID_PARAM;
    }
    regs[0] = regAttr->addr;
    msgs[0].addr = ES8316_I2C_DEV_ADDR;
    msgs[0].flags = 0;
    msgs[0].len = ES8316_I2C_REG_DATA_LEN + 1;
    AUDIO_DRIVER_LOG_DEBUG("msgs[0].addr=0x%02x, regs[0]=0x%02x.", msgs[0].addr, regs[0]);
    if (rwFlag == 0) { // write
        // S 11011A2A1 0 A ADDR A MS1 A LS1 A <....> P
        msgBuf = OsalMemCalloc(ES8316_I2C_REG_DATA_LEN + 1);
        if (msgBuf == NULL) {
            AUDIO_DRIVER_LOG_ERR("[write]: malloc buf failed!");
            return HDF_ERR_MALLOC_FAIL;
        }
        msgBuf[0] = regs[0];
        msgBuf[1] = (uint8_t)regAttr->value;
        if (ES8316_I2C_REG_DATA_LEN == ES8316_I2C_MSG_BUF_SIZE) { // when 2 bytes
            msgBuf[1] = (regAttr->value >> ES8316_COMM_SHIFT_8BIT); // High 8 bits
            msgBuf[ES8316_I2C_MSG_BUF_SIZE] = (uint8_t)(regAttr->value & ES8316_COMM_MASK_FF);    // Low 8 bits
        }
        msgs[0].buf = msgBuf;
        AUDIO_DRIVER_LOG_DEBUG("msgs[0].buf=0x%x.", regAttr->value);
    } else {
        // S 11011A2A1 0 A ADDR A Sr 11011A2A1 1 A MS1 A LS1 A <....> NA P
        msgBuf = OsalMemCalloc(ES8316_I2C_REG_DATA_LEN);
        if (msgBuf == NULL) {
            AUDIO_DRIVER_LOG_ERR("[read]: malloc buf failed!");
            return HDF_ERR_MALLOC_FAIL;
        }
        msgs[0].len = 1;
        msgs[0].buf = regs;
        msgs[1].addr = ES8316_I2C_DEV_ADDR;
        msgs[1].flags = I2C_FLAG_READ;
        msgs[1].len = ES8316_I2C_REG_DATA_LEN;
        msgs[1].buf = msgBuf;
    }
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int32_t Es8316I2cReadWrite(struct AudioAddrConfig *regAttr, uint16_t rwFlag)
{
    int32_t ret;
    DevHandle i2cHandle;
    int16_t transferMsgCount = 1;
    uint8_t regs[ES8316_I2C_REG_SIZE];
    struct I2cMsg msgs[ES8316_I2C_MSG_NUM];
    (void)memset_s(msgs, sizeof(struct I2cMsg) * ES8316_I2C_MSG_NUM, 0, sizeof(struct I2cMsg) * ES8316_I2C_MSG_NUM);

    AUDIO_DRIVER_LOG_DEBUG("entry.\n");
    if (regAttr == NULL || rwFlag < 0 || rwFlag > 1) {
        AUDIO_DRIVER_LOG_ERR("invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    i2cHandle = I2cOpen(RK3399_I2C_BUS_NUMBER);
    if (i2cHandle == NULL) {
        AUDIO_DRIVER_LOG_ERR("open i2cBus:%u failed! i2cHandle:%p", RK3399_I2C_BUS_NUMBER, i2cHandle);
        return HDF_FAILURE;
    }
    if (rwFlag == I2C_FLAG_READ) {
        transferMsgCount = ES8316_I2C_MSG_NUM;
    }
    ret = Es8316I2cMsgFill(regAttr, rwFlag, regs, msgs);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8316I2cMsgFill failed!");
        I2cClose(i2cHandle);
        return HDF_FAILURE;
    }
    ret = I2cTransfer(i2cHandle, msgs, transferMsgCount);
    if (ret != transferMsgCount) {
        AUDIO_DRIVER_LOG_ERR("I2cTransfer err:%d", ret);
        Es8316I2cRelease(msgs, transferMsgCount, i2cHandle);
        return HDF_FAILURE;
    }
    if (rwFlag == I2C_FLAG_READ) {
        regAttr->value = msgs[1].buf[0];
        if (ES8316_I2C_REG_DATA_LEN == ES8316_I2C_MSG_BUF_SIZE) { // when 2 bytes
            regAttr->value = (msgs[1].buf[0] << ES8316_COMM_SHIFT_8BIT) | msgs[1].buf[1]; // result value 16 bits
        }
        AUDIO_DRIVER_LOG_DEBUG("[read]: regAttr->regValue=0x%x.\n", regAttr->value);
    }
    Es8316I2cRelease(msgs, transferMsgCount, i2cHandle);
    return HDF_SUCCESS;
}

// Read contrl reg bits value
static int32_t Es8316RegBitsRead(struct AudioMixerControl *regAttr, uint32_t *regValue)
{
    int32_t ret;
    struct AudioAddrConfig regVal;
    if (regAttr == NULL || regAttr->reg < 0 || regValue == NULL) {
        AUDIO_DRIVER_LOG_ERR("input invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    regVal.addr  = regAttr->reg;
    ret = Es8316I2cReadWrite(&regVal, I2C_FLAG_READ);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8316RegBitsRead failed.");
        return HDF_FAILURE;
    }
    *regValue = regVal.value;
    regAttr->value = (regVal.value >> regAttr->shift) & regAttr->mask;
    if (regAttr->value > regAttr->max || regAttr->value < regAttr->min) {
        AUDIO_DRIVER_LOG_ERR("invalid bitsValue=0x%x", regAttr->value);
        return HDF_FAILURE;
    }
    if (regAttr->invert) {
        regAttr->value = regAttr->max - regAttr->value;
    }
    AUDIO_DRIVER_LOG_DEBUG("regAddr=0x%x, regValue=0x%x, currBitsValue=0x%x",
        regAttr->reg, regVal.value, regAttr->value);
    AUDIO_DRIVER_LOG_DEBUG("mask=0x%x, shift=%d, max=0x%x,min=0x%x, invert=%d",
        regAttr->mask, regAttr->shift, regAttr->max, regAttr->min, regAttr->invert);
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

// Update contrl reg bits value
static int32_t Es8316RegBitsUpdate(struct AudioMixerControl regAttr)
{
    int32_t ret;
    struct AudioAddrConfig regVal;
    uint32_t newValue, newMask, value;
    if (regAttr.reg < 0) {
        AUDIO_DRIVER_LOG_ERR("input invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    if (regAttr.invert) {
        regAttr.value = regAttr.max - regAttr.value;
    }
    newValue = regAttr.value << regAttr.shift;
    newMask = regAttr.mask << regAttr.shift;
    ret = Es8316RegBitsRead(&regAttr, &value);
    if (ret != HDF_SUCCESS) {
        ADM_LOG_ERR("Es8316RegBitsRead faileded, ret=%d.", ret);
        return HDF_FAILURE;
    }
    regVal.value = (value & ~newMask) | (newValue & newMask);
    regVal.addr  = regAttr.reg;
    ret = Es8316I2cReadWrite(&regVal, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8316I2cReadWrite faileded.");
        return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG("regAddr=0x%x, regValue=0x%x, oldValue=0x%x, newValue=0x%x,",
        regAttr.reg, regVal.value, regAttr.value, newValue);
    AUDIO_DRIVER_LOG_DEBUG(" mask=0x%x, shift=%d, max=0x%x, min=0x%x, invert=%d",
        newMask, regAttr.shift, regAttr.max, regAttr.min, regAttr.invert);
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int32_t Es8316DaiParamsUpdate4Scene(struct AudioMixerControl *daiHwParamsRegCfgItem,
    struct Es8316DaiHwParamsTransferData transferData, struct Es8316DaiParamsVal daiParamsVal)
{
    int32_t ret;
    uint8_t i;
    ret = (daiHwParamsRegCfgItem == NULL || transferData.daiHwParamsRegCfgItemCount == 0);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("Invaild params.");
        return HDF_FAILURE;
    }
    for (i = transferData.inputParamsBeginIndex; i <= transferData.inputParamsEndIndex; i++) {
        switch (i) {
            case ES8316_DHP_RENDER_FREQUENCY_INX:
            case ES8316_DHP_CAPTURE_FREQUENCY_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.frequencyVal;
                break;
            case ES8316_DHP_RENDER_FORMAT_INX:
            case ES8316_DHP_CAPTURE_FORMAT_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.formatVal;
                break;
            case ES8316_DHP_RENDER_CHANNEL_INX:
            case ES8316_DHP_CAPTURE_CHANNEL_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.channelVal;
                break;
            default:
                continue;
        }
        ret = Es8316RegBitsUpdate(daiHwParamsRegCfgItem[i]);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("Es8316RegBitsUpdate failed.");
            return HDF_FAILURE;
        }
    }

    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

// update external codec I2S frequency
static int32_t Es8316DaiParamsUpdate(struct Es8316DaiParamsVal daiParamsVal, bool playback)
{
    int32_t ret;
    struct AudioMixerControl *daiHwParamsRegCfgItem = NULL;
    struct Es8316DaiHwParamsTransferData transferData;
    ret = (g_es8316TransferData.codecRegCfgGroupNode == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_PATAM_GROUP] == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_PATAM_GROUP]->regCfgItem == NULL);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("g_es8316RegCfgGroupNode[AUDIO_DAI_PATAM_GROUP] is NULL.");
        return HDF_FAILURE;
    }
    daiHwParamsRegCfgItem =
        g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_PATAM_GROUP]->regCfgItem;
    transferData.daiHwParamsRegCfgItemCount =
        g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_PATAM_GROUP]->itemNum;
    if (!playback) { // capture
        transferData.inputParamsBeginIndex = ES8316_DHP_CAPTURE_FREQUENCY_INX;
        transferData.inputParamsEndIndex = ES8316_DHP_CAPTURE_CHANNEL_INX;
    } else { // playback
        transferData.inputParamsBeginIndex = ES8316_DHP_RENDER_FREQUENCY_INX;
        transferData.inputParamsEndIndex = ES8316_DHP_RENDER_CHANNEL_INX;
    }
    ret = Es8316DaiParamsUpdate4Scene(daiHwParamsRegCfgItem, transferData, daiParamsVal);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316DaiParamsUpdate4Scene failed.");
        return HDF_FAILURE;
    }

    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}


static int32_t Es8316DeviceFrequencyParse(uint32_t rate, uint16_t *freq)
{
    if (freq == NULL) {
        AUDIO_DRIVER_LOG_ERR("input param is NULL");
        return HDF_FAILURE;
    }
    switch (rate) {
        case ES8316_I2S_SAMPLE_FREQUENCY_8000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_8000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_11025:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_11025;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_12000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_12000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_16000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_16000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_22050:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_22050;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_24000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_24000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_32000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_32000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_44100:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_44100;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_48000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_48000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_64000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_64000;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_88200:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_88200;
            break;
        case ES8316_I2S_SAMPLE_FREQUENCY_96000:
            *freq = ES8316_I2S_SAMPLE_FREQUENCY_REG_VAL_96000;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("rate: %d is not support.", rate);
            return HDF_ERR_NOT_SUPPORT;
    }
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int32_t Es8316DeviceCfgGet(struct CodecData *codecData,
    struct Es8316TransferData *es8316TransferData)
{
    int32_t ret;
    int32_t index;
    int32_t audioCfgCtrlCount;

    ret = (codecData == NULL || codecData->regConfig == NULL || es8316TransferData == NULL);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }
    g_es8316RegCfgGroupNode = codecData->regConfig->audioRegParams;
    ret = (g_es8316RegCfgGroupNode[AUDIO_CTRL_CFG_GROUP] == NULL ||
            g_es8316RegCfgGroupNode[AUDIO_CTRL_CFG_GROUP]->ctrlCfgItem == NULL ||
            g_es8316RegCfgGroupNode[AUDIO_CTRL_PATAM_GROUP] == NULL ||
            g_es8316RegCfgGroupNode[AUDIO_CTRL_PATAM_GROUP]->regCfgItem == NULL);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("parsing params is NULL.");
        return HDF_FAILURE;
    }
    struct AudioControlConfig *ctlcfgItem = g_es8316RegCfgGroupNode[AUDIO_CTRL_CFG_GROUP]->ctrlCfgItem;
    audioCfgCtrlCount = g_es8316RegCfgGroupNode[AUDIO_CTRL_CFG_GROUP]->itemNum;
    g_es8316Controls = (struct AudioKcontrol *)OsalMemCalloc(audioCfgCtrlCount * sizeof(struct AudioKcontrol));
    es8316TransferData->codecRegCfgGroupNode = g_es8316RegCfgGroupNode;
    es8316TransferData->codecCfgCtrlCount = audioCfgCtrlCount;
    es8316TransferData->codecControls = g_es8316Controls;
    for (index = 0; index < audioCfgCtrlCount; index++) {
        g_es8316Controls[index].iface = ctlcfgItem[index].iface;
        g_es8316Controls[index].name  = g_audioControlsList[ctlcfgItem[index].arrayIndex];
        g_es8316Controls[index].Info  = AudioInfoCtrlOps;
        g_es8316Controls[index].privateValue =
            (unsigned long)&g_es8316RegCfgGroupNode[AUDIO_CTRL_PATAM_GROUP]->regCfgItem[index];
        g_es8316Controls[index].Get = AudioCodecGetCtrlOps;
        g_es8316Controls[index].Set = AudioCodecSetCtrlOps;
    }
    return HDF_SUCCESS;
}

/*
 * init control reg to default value
 */
static int32_t Es8316DeviceCtrlRegInit(void)
{
    int32_t ret, i;
    // Set codec control register(00h-14h) default value
    ret = (g_es8316RegCfgGroupNode == NULL || g_es8316RegCfgGroupNode[AUDIO_INIT_GROUP] == NULL
            || g_es8316RegCfgGroupNode[AUDIO_INIT_GROUP]->addrCfgItem == NULL);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("g_es8316RegCfgGroupNode[AUDIO_INIT_GROUP] is NULL.");
        return HDF_FAILURE;
    }
    struct AudioAddrConfig *initCfg = g_es8316RegCfgGroupNode[AUDIO_INIT_GROUP]->addrCfgItem;
    for (i = 0; i < g_es8316RegCfgGroupNode[AUDIO_INIT_GROUP]->itemNum; i++) {
        AUDIO_DRIVER_LOG_DEBUG("i=%d, Addr = [0x%2x]", i, initCfg[i].addr);
        ret = Es8316I2cReadWrite(&initCfg[i], 0);
        if (ret != HDF_SUCCESS) {
            AUDIO_DRIVER_LOG_ERR("Es8316I2cReadWrite(write) err, regAttr.regAddr: 0x%x.\n",
                initCfg[i].addr);
            return HDF_FAILURE;
        }
        OsalMSleep(ES8316_I2C_WAIT_TIMES);
    }
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8316DeviceRegRead(unsigned long virtualAddress, uint32_t reg, uint32_t *val)
{
    int32_t ret;
    struct AudioAddrConfig regAttr;
    if (val == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    regAttr.addr = (uint8_t)reg;
    regAttr.value = 0;
    ret = Es8316I2cReadWrite(&regAttr, I2C_FLAG_READ);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("failed.");
        return HDF_FAILURE;
    }
    *val = regAttr.value;
    AUDIO_DRIVER_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t Es8316DeviceRegWrite(unsigned long virtualAddress, uint32_t reg, uint32_t value)
{
    int32_t ret;
    struct AudioAddrConfig regAttr;
    regAttr.addr = (uint8_t)reg;
    regAttr.value = (uint16_t)value;
    ret = Es8316I2cReadWrite(&regAttr, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("failed.");
        return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t Es8316GetConfigInfo(const struct HdfDeviceObject *device, struct CodecData *codecData)
{
    if (device == NULL || codecData == NULL) {
        AUDIO_DRIVER_LOG_ERR("param is null!");
        return HDF_FAILURE;
    }

    if (codecData->regConfig != NULL) {
        ADM_LOG_ERR("g_codecData regConfig  fail!");
        return HDF_FAILURE;
    }

    codecData->regConfig = (struct AudioRegCfgData *)OsalMemCalloc(sizeof(*(codecData->regConfig)));
    if (codecData->regConfig == NULL) {
        ADM_LOG_ERR("malloc AudioRegCfgData fail!");
        return HDF_FAILURE;
    }

    if (CodecGetRegConfig(device, codecData->regConfig) != HDF_SUCCESS) {
        ADM_LOG_ERR("CodecGetRegConfig fail!");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}


/* Original es8316 functions */
static int32_t Es8316FormatParse(enum AudioFormat format, uint16_t *bitWidth)
{
    // current set default format(standard) for 24 bit
    if (bitWidth == NULL) {
        AUDIO_DEVICE_LOG_ERR("input param is NULL");
        return HDF_FAILURE;
    }
    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            *bitWidth = ES8316_I2S_SAMPLE_FORMAT_REG_VAL_16;
            break;
        case AUDIO_FORMAT_PCM_24_BIT:
            *bitWidth = ES8316_I2S_SAMPLE_FORMAT_REG_VAL_24;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            *bitWidth = ES8316_I2S_SAMPLE_FORMAT_REG_VAL_32;
            break;
        default:
            AUDIO_DEVICE_LOG_ERR("format: %d is not support.", format);
            return HDF_ERR_NOT_SUPPORT;
    }
    AUDIO_DEVICE_LOG_DEBUG(" success.");
    return HDF_SUCCESS;
}

static int32_t Es8316WorkStatusEnable()
{
    int32_t ret;
    uint8_t i;
    struct AudioMixerControl *daiStartupParamsRegCfgItem = NULL;
    uint8_t daiStartupParamsRegCfgItemCount;
    ret = (g_es8316TransferData.codecRegCfgGroupNode == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_STARTUP_PATAM_GROUP] == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_STARTUP_PATAM_GROUP]->regCfgItem == NULL);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("g_es8316RegCfgGroupNode[AUDIO_DAI_STARTUP_PATAM_GROUP] is NULL.");
        return HDF_FAILURE;
    }
    daiStartupParamsRegCfgItem =
        g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_STARTUP_PATAM_GROUP]->regCfgItem;
    daiStartupParamsRegCfgItemCount =
        g_es8316TransferData.codecRegCfgGroupNode[AUDIO_DAI_STARTUP_PATAM_GROUP]->itemNum;
    for (i = 0; i < daiStartupParamsRegCfgItemCount; i++) {
        ret = Es8316RegBitsUpdate(daiStartupParamsRegCfgItem[i]);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("Es8316RegBitsUpdate failed.");
            return HDF_FAILURE;
        }
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

/*
 * ES8316 Hardware Rest
*/
static int32_t Es8316HardwareRest(void)
{
    int32_t ret;
    const char *spk_con = "spk-con";
    ret = gpio_request(RK3399_GPIO_NUMBER, spk_con);
    if (ret != 0 && ret != NUM_NEG_SIXTEEN) {
        AUDIO_DEVICE_LOG_ERR("gpio0_b3 request failed. ret:%d", ret);
        gpio_free(RK3399_GPIO_NUMBER);
        return HDF_FAILURE;
    }
    ret = GpioSetDir(RK3399_GPIO_NUMBER, GPIO_DIR_OUT);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("set gpio dir failed. ret:%d", ret);
        return ret;
    }
    ret = GpioWrite(RK3399_GPIO_NUMBER, GPIO_VAL_LOW);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("write gpio val failed. ret:%d", ret);
        return ret;
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

/*
 * Es8316 Software Rest
 */
static int32_t Es8316SoftwareRest(void)
{
    int32_t ret;
    int i;
    struct AudioAddrConfig *es8316RegRestAttr;
    uint32_t restRegCfgItemCount;
    ret = (g_es8316TransferData.codecRegCfgGroupNode == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_RSET_GROUP] == NULL
            || g_es8316TransferData.codecRegCfgGroupNode[AUDIO_RSET_GROUP]->addrCfgItem == NULL);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("g_es8316RegCfgGroupNode[AUDIO_RSET_GROUP] is NULL.");
        return HDF_FAILURE;
    }
    es8316RegRestAttr = g_es8316TransferData.codecRegCfgGroupNode[AUDIO_RSET_GROUP]->addrCfgItem;
    restRegCfgItemCount = g_es8316TransferData.codecRegCfgGroupNode[AUDIO_RSET_GROUP]->itemNum;
    for (i = 0; i < restRegCfgItemCount; i++) {
        ret = Es8316I2cReadWrite(&es8316RegRestAttr[i], 0);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("Es8316I2cReadWrite(write) failed, regAddr: 0x%02x.", es8316RegRestAttr[i].addr);
            return HDF_FAILURE;
        }
        OsalMSleep(ES8316_I2C_WAIT_TIMES);
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8316DeviceInit(struct AudioCard *audioCard, const struct CodecDevice *device)
{
    int32_t ret;
    if ((audioCard == NULL) || (device == NULL)) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    ret = Es8316DeviceCfgGet(device->devData, &g_es8316TransferData);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316DeviceCfgGet failed.");
        return HDF_FAILURE;
    }
    ret = Es8316HardwareRest();
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316HardwareRest failed.");
        return HDF_FAILURE;
    }
    ret = Es8316SoftwareRest();
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316SoftwareRest failed.");
        return HDF_FAILURE;
    }
    // Initial es8316 register
    ret = Es8316DeviceCtrlRegInit();
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316DeviceRegInit failed.");
        return HDF_FAILURE;
    }
    ret = AudioAddControls(audioCard, g_es8316TransferData.codecControls,
        g_es8316TransferData.codecCfgCtrlCount);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("AudioAddControls failed.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8316DaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device)
{
    if (device == NULL || device->devDaiName == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }
    (void)card;
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8316DaiStartup(const struct AudioCard *card, const struct DaiDevice *device)
{
    int ret;
    (void)card;
    (void)device;
    ret = Es8316WorkStatusEnable();
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316WorkStatusEnable failed.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8316DaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param)
{
    int32_t ret;
    uint16_t frequency, bitWidth;
    struct Es8316DaiParamsVal daiParamsVal;
    bool playback = true;
    AUDIO_DEVICE_LOG_DEBUG("entry.");
    (void)card;
    if (param == NULL || param->cardServiceName == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_PARAM;
    }
    ret = Es8316DeviceFrequencyParse(param->rate, &frequency);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316DeviceFrequencyParse failed.");
        return HDF_ERR_NOT_SUPPORT;
    }
    ret = Es8316FormatParse(param->format, &bitWidth);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316FormatParse failed.");
        return HDF_ERR_NOT_SUPPORT;
    }
    daiParamsVal.frequencyVal = frequency;
    daiParamsVal.formatVal = bitWidth;
    daiParamsVal.channelVal = param->channels;
    playback = (param->streamType == AUDIO_RENDER_STREAM) ? true : false;
    ret = Es8316DaiParamsUpdate(daiParamsVal, playback);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8316DaiParamsUpdate failed.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("channels = %d, rate = %d, periodSize = %d, \
        periodCount = %d, format = %d, cardServiceName = %s \n",
        param->channels, param->rate, param->periodSize,
        param->periodCount, (uint32_t)param->format, param->cardServiceName);
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}


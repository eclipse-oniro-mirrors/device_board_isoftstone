/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>

#include <linux/dmaengine.h>
#include <sound/core.h>
#include <sound/memalloc.h>

#include "audio_platform_base.h"
#include "audio_sapm.h"
#include "osal_io.h"
#include "osal_time.h"

#include "osal_uaccess.h"
#include "audio_platform_base.h"
#include "audio_driver_log.h"
#include "rk3399_dma_ops.h"

#define HDF_LOG_TAG rk3399_dma_ops
#define IIS_PHY_BASE     0xff880000  //  i2s@ff880000
#define I2S_TXDR         0x24
#define I2S_RXDR         0x28

#define DMAC0_REG_BASE_ADDR   (0xff6d0000)

#define DMAC_DSR        (0x0000)
#define DMAC_DPC        (0x0004)
#define DMAC_INTEN      (0x0020)
#define DMAC_EVENT_RIS  (0x0024)
#define DMAC_INTMIS     (0x0028)
#define DMAC_INTCLR     (0x002c)
#define DMAC_FSRD       (0x0030)
#define DMAC_FSRC       (0x0034)
#define DMAC_FTRD       (0x0038)
#define DMAC_FTR0       (0x0040)
#define DMAC_CSR0       (0x0100)
#define DMAC_CPC0       (0x0104)
#define DMAC_SAR0       (0x0400)
#define DMAC_DAR0       (0x0404)
#define DMAC_CCR0       (0x0408)
#define DMAC_CR0        (0x0e00)

#define FRAME_SIZE_BIT_8  (8)

const int MAX_CHAN_COUNT = 2;
const int MAX_BURST_NUM = 8;

static const int MIN_BUFF_SIZE = 16 * 1024;
const int MAX_PERIODS_COUNT = 100;

struct DmaRuntimeData {
    struct dma_chan *dmaChn[2];
    struct dma_slave_config config;
    void *dmaCfgArray;
    dma_addr_t dmaCfgPhy[2];
    void *dmaCfgVirt[2];
    struct dma_async_tx_descriptor *dmaTxDes[2];
    dma_cookie_t cookie[2];
    struct device *dev;
};

struct DmaRuntimeData g_dmaRtd;

int32_t AudioDmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform)
{
    AUDIO_DRIVER_LOG_DEBUG("entry.");

    struct PlatformData *data = NULL;

    data = PlatformDataFromCard(card);
    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("PlatformDataFromCard failed.");
        return HDF_FAILURE;
    }

    if (data->platformInitFlag == true) {
        AUDIO_DRIVER_LOG_DEBUG("platform init complete!");
        return HDF_SUCCESS;
    }

    data->platformInitFlag = true;
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int32_t DmaRtdMemAlloc(struct PlatformData *data)
{
    AUDIO_DRIVER_LOG_DEBUG("entry.");

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null.");
        return HDF_FAILURE;
    }

    data->dmaPrv = (void *)&g_dmaRtd;

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t RK3399DmaBufAlloc(struct PlatformData *data, enum AudioStreamType streamType)
{
    int ret;
    uint32_t size;
    struct device dmaDev;
    struct DmaRuntimeData *dmaRtd = NULL;
    char *chanName[MAX_CHAN_COUNT] = {"tx", "rx"};

    AUDIO_DRIVER_LOG_DEBUG("entry.");

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    ret = DmaRtdMemAlloc(data);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("DmaRtdMemAlloc fail.");
        return HDF_FAILURE;
    }

    dmaDev.of_node = of_find_node_by_path("/i2s@ff880000");
    if (dmaDev.of_node == NULL) {
        AUDIO_DRIVER_LOG_ERR("of_node is NULL.");
        return HDF_FAILURE;
    }

    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaRtd is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        // AUDIO_CAPTURE_STREAM
        dmaRtd->dmaChn[1] = dma_request_slave_channel_reason(&dmaDev, chanName[1]);
        if (dmaRtd->dmaChn[1] == NULL || dmaRtd->dmaChn[1]->device == NULL || dmaRtd->dmaChn[1]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("capture dmaChn is null.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("chan id = %d .", dmaRtd->dmaChn[1]->chan_id);

        size = data->captureBufInfo.cirBufMax;

        AUDIO_DRIVER_LOG_DEBUG("mem alloc buff size: %d .", size);

        if (data->captureBufInfo.virtAddr == NULL) {
            dmaRtd->dmaChn[1]->device->dev->coherent_dma_mask = 0xffffffffUL;
            data->captureBufInfo.virtAddr = dma_alloc_wc(dmaRtd->dmaChn[1]->device->dev,
                data->captureBufInfo.cirBufMax, 
                (dma_addr_t *)&data->captureBufInfo.phyAddr, GFP_DMA | GFP_KERNEL);
            if (data->captureBufInfo.virtAddr == NULL) {
                AUDIO_DRIVER_LOG_ERR("dma_alloc_wc fail.");
                return HDF_FAILURE;
            }
            AUDIO_DRIVER_LOG_DEBUG("captureBufInfo virtAddr = 0x%px phyAddr = 0x%x.",
                data->captureBufInfo.virtAddr, data->captureBufInfo.phyAddr);
        }
        dmaRtd->dev = dmaRtd->dmaChn[1]->device->dev;

        data->captureBufInfo.wbufOffSet = 0;
        data->captureBufInfo.periodsMax = MAX_PERIODS_COUNT;

        AUDIO_DRIVER_LOG_DEBUG("captureBufInfo success.");
    } else if (streamType == AUDIO_RENDER_STREAM) {
        dmaRtd->dmaChn[0] = dma_request_slave_channel_reason(&dmaDev, chanName[0]);
        if (dmaRtd->dmaChn[0] == NULL || dmaRtd->dmaChn[0]->device == NULL
            || dmaRtd->dmaChn[0]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("render dmaChn is null.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("chan id = %d .", dmaRtd->dmaChn[0]->chan_id);

        size = data->renderBufInfo.cirBufMax;
        AUDIO_DRIVER_LOG_DEBUG("mem alloc buff size: %d .", size);

        if (data->renderBufInfo.virtAddr == NULL) {
            dmaRtd->dmaChn[0]->device->dev->coherent_dma_mask = 0xffffffffUL;
            data->renderBufInfo.virtAddr = dma_alloc_wc(dmaRtd->dmaChn[0]->device->dev,
                data->renderBufInfo.cirBufMax,
                (dma_addr_t *)&data->renderBufInfo.phyAddr, GFP_DMA | GFP_KERNEL);

            if (data->renderBufInfo.virtAddr == NULL) {
                AUDIO_DRIVER_LOG_ERR("dma_alloc_wc fail.");
                return HDF_FAILURE;
            }

            AUDIO_DRIVER_LOG_DEBUG("renderBufInfo virtAddr = 0x%px phyAddr = 0x%x.",
                data->renderBufInfo.virtAddr, data->renderBufInfo.phyAddr);
        }
        dmaRtd->dev = dmaRtd->dmaChn[0]->device->dev;

        data->renderBufInfo.wbufOffSet = 0;
        data->renderBufInfo.periodsMax = MAX_PERIODS_COUNT;

        AUDIO_DRIVER_LOG_DEBUG("renderBufInfo success.");
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");

    return HDF_SUCCESS;
}

static int32_t DmaRtdMemFree(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct DmaRuntimeData *dmaRtd = NULL;
    AUDIO_DRIVER_LOG_DEBUG("entry.");

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null.");
        return HDF_FAILURE;
    }
    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_DEBUG("dmaPrv is null.");
        return HDF_SUCCESS;
    }
    if (streamType == AUDIO_RENDER_STREAM && dmaRtd->dmaChn[0] != NULL
        && dmaRtd->dmaChn[0]->device != NULL && dmaRtd->dmaChn[0]->device->dev != NULL) {
        dma_release_channel(dmaRtd->dmaChn[0]);
    }
    if (streamType == AUDIO_CAPTURE_STREAM && dmaRtd->dmaChn[1] != NULL
        && dmaRtd->dmaChn[1]->device != NULL && dmaRtd->dmaChn[1]->device->dev != NULL) {
        dma_release_channel(dmaRtd->dmaChn[1]);
    }

    AUDIO_DRIVER_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t RK3399DmaBufFree(struct PlatformData *data, const enum AudioStreamType streamType)
{
    int ret;
    struct DmaRuntimeData *dmaRtd = NULL;
    AUDIO_DRIVER_LOG_DEBUG("success");

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    dmaRtd = data->dmaPrv;
    if (dmaRtd == NULL || dmaRtd->dev == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaRtd is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        if (data->captureBufInfo.virtAddr != NULL && dmaRtd->dmaChn[0] != NULL
            && dmaRtd->dmaChn[0]->device != NULL && dmaRtd->dmaChn[0]->device->dev != NULL) {
            dma_free_wc(dmaRtd->dev, data->captureBufInfo.cirBufMax, data->captureBufInfo.virtAddr,
                data->captureBufInfo.phyAddr);
            data->captureBufInfo.phyAddr = (dma_addr_t)NULL;
        }
    } else if (streamType == AUDIO_RENDER_STREAM) {
        if (data->renderBufInfo.virtAddr != NULL && dmaRtd->dmaChn[1] != NULL
            && dmaRtd->dmaChn[1]->device != NULL && dmaRtd->dmaChn[1]->device->dev != NULL) {
            dma_free_wc(dmaRtd->dev, data->renderBufInfo.cirBufMax, data->renderBufInfo.virtAddr,
                data->renderBufInfo.phyAddr);
            data->renderBufInfo.phyAddr = (dma_addr_t)NULL;
        }
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    ret = DmaRtdMemFree(data, streamType);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("DmaRtdMemFree fail.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

int32_t  RK3399DmaRequestChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    return HDF_SUCCESS;
}

static int32_t SetBusWidthToSlaveConfig(struct PcmInfo pcmInfo, struct dma_slave_config *slaveConfig)
{
    enum dma_slave_buswidth buswidth;
    int bits;

    if (slaveConfig == NULL) {
        AUDIO_DRIVER_LOG_ERR("slaveConfig is null.");
        return HDF_FAILURE;
    }

    bits = pcmInfo.bitWidth * pcmInfo.channels;
    if (bits < 8 || bits > 64)
        return HDF_FAILURE;
    else if (bits == 8)
        buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
    else if (bits == 16)
        buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
    else if (bits == 24)
        buswidth = DMA_SLAVE_BUSWIDTH_3_BYTES;
    else if (bits <= 32)
        buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
    else
        buswidth = DMA_SLAVE_BUSWIDTH_8_BYTES;

    if (pcmInfo.streamType == AUDIO_CAPTURE_STREAM) {
        slaveConfig->src_addr_width = buswidth;
    } else { // AUDIO_RENDER_STREAM
        slaveConfig->dst_addr_width = buswidth;
    }

    return HDF_SUCCESS;
}

static int32_t SetDmaSlaveConfig(struct PcmInfo pcmInfo, const enum AudioStreamType streamType,
    struct dma_slave_config *slaveConfig)
{
    int32_t ret;
    if (slaveConfig == NULL) {
        AUDIO_DRIVER_LOG_ERR("slaveConfig is null.");
        return HDF_FAILURE;
    }

    pcmInfo.streamType = streamType;
    ret = SetBusWidthToSlaveConfig(pcmInfo, slaveConfig);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("SetBusWidthToSlaveConfig failed.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        slaveConfig->direction = DMA_DEV_TO_MEM;
        slaveConfig->src_addr = IIS_PHY_BASE + I2S_RXDR;
        slaveConfig->src_maxburst = MAX_BURST_NUM;
    } else { // AUDIO_RENDER_STREAM
        slaveConfig->direction = DMA_MEM_TO_DEV;
        slaveConfig->dst_addr = IIS_PHY_BASE + I2S_TXDR;
        slaveConfig->dst_maxburst = MAX_BURST_NUM;
    }
    slaveConfig->device_fc = false;
    slaveConfig->slave_id = 0;
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t RK3399DmaConfigChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct DmaRuntimeData *dmaRtd = NULL;
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    int32_t ret = HDF_SUCCESS;
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("PlatformDataFromCard failed.");
        return HDF_FAILURE;
    }

    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaPrv is null.");
        return HDF_FAILURE;
    }

    (void)memset_s(&dmaRtd->config, sizeof(struct dma_slave_config), 0, sizeof(struct dma_slave_config));

    if (streamType == AUDIO_CAPTURE_STREAM) {
        SetDmaSlaveConfig(data->capturePcmInfo, streamType, &dmaRtd->config);
        if (ret != HDF_SUCCESS) {
            AUDIO_DRIVER_LOG_ERR("SetDmaSlaveConfig failed.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("bitWidth = %d channels = %d.", data->capturePcmInfo.bitWidth,
            data->capturePcmInfo.channels);
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaChan[1] == NULL || dmaChan[1]->device == NULL) {
            AUDIO_DRIVER_LOG_ERR("capture dmaChan is null.");
            return HDF_FAILURE;
        }
        ret = dmaengine_slave_config(dmaChan[1], &dmaRtd->config);
        if (ret != HDF_SUCCESS) {
            AUDIO_DRIVER_LOG_ERR("dmaengine_slave_config fail.");
            return HDF_FAILURE;
        }
    } else { // AUDIO_RENDER_STREAM
        ret = SetDmaSlaveConfig(data->renderPcmInfo, streamType, &dmaRtd->config);
        if (ret != HDF_SUCCESS) {
            AUDIO_DRIVER_LOG_ERR("SetDmaSlaveConfig failed.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("bitWidth = %d channels = %d.", data->renderPcmInfo.bitWidth,
            data->renderPcmInfo.channels);
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaChan[0] == NULL || dmaChan[0]->device == NULL) {
            AUDIO_DRIVER_LOG_ERR("render dmaChan is null.");
            return HDF_FAILURE;
        }
        ret = dmaengine_slave_config(dmaChan[0], &dmaRtd->config);
        if (ret != HDF_SUCCESS) {
            AUDIO_DRIVER_LOG_ERR("dmaengine_slave_config fail.");
            return HDF_FAILURE;
        }
    }
    AUDIO_DRIVER_LOG_DEBUG("success.");

    return HDF_SUCCESS;
}

static inline u32 PcmDmaGetAddr(struct dma_chan *dma_chn, dma_cookie_t cookie)
{
    struct dma_tx_state dma_state;
    dmaengine_tx_status(dma_chn, cookie, &dma_state);
    return dma_state.residue;
}

static inline signed long BytesToFrames(uint32_t frameBits, uint32_t size)
{
    return size * FRAME_SIZE_BIT_8 / frameBits;
}

int32_t RK3399PcmPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer)
{
    int nowPointer;
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    struct DmaRuntimeData *dmaRtd = NULL;
    int shift = 1;
    uint32_t resiude1;
    uint32_t frameSize;

    if (pointer == NULL) {
        AUDIO_DRIVER_LOG_ERR("pointer is null.");
        return HDF_FAILURE;
    }

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("PlatformData is null.");
        return HDF_FAILURE;
    }

    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaRtd is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        if (data->capturePcmInfo.interleaved) {
            shift = 0;
        }
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaChan[1]) {
            resiude1 = PcmDmaGetAddr(dmaChan[1], dmaRtd->cookie[1]);
            nowPointer = data->captureBufInfo.cirBufSize - resiude1;
        }
        frameSize = data->capturePcmInfo.bitWidth * data->capturePcmInfo.channels;
    } else { // AUDIO_RENDER_STREAM
        if (data->renderPcmInfo.interleaved) {
            shift = 0;
        }
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaChan[0]) {
            resiude1 = PcmDmaGetAddr(dmaChan[0], dmaRtd->cookie[0]);
            nowPointer = data->renderBufInfo.cirBufSize - resiude1;
        }
        frameSize = data->renderPcmInfo.bitWidth * data->renderPcmInfo.channels;
    }
    *pointer = BytesToFrames(frameSize, nowPointer); // 32 is frame byte
    if ((streamType == AUDIO_RENDER_STREAM && *pointer == data->renderBufInfo.cirBufSize)
        || (streamType == AUDIO_CAPTURE_STREAM && *pointer == data->captureBufInfo.cirBufSize)) {
        *pointer = 0;
    }

    return HDF_SUCCESS;
}

int32_t RK3399DmaPrep(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;

    return HDF_SUCCESS;
}

int32_t RK3399DmaSubmit(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    enum dma_transfer_direction direction;
    unsigned long flags = 3;
    struct DmaRuntimeData *dmaRtd = NULL;
    struct dma_async_tx_descriptor *desc;
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    if (data == NULL || (struct DmaRuntimeData *)data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null.");
        return HDF_FAILURE;
    }

    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaPrv is null.");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_CAPTURE_STREAM) {
        direction = DMA_DEV_TO_MEM;
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaRtd->dmaChn[1] == NULL || dmaRtd->dmaChn[1]->device == NULL
            || dmaRtd->dmaChn[1]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("captureBufInfo.phyAddr = 0x%x.", data->captureBufInfo.phyAddr);
        desc = dmaengine_prep_dma_cyclic(dmaChan[1],
            data->captureBufInfo.phyAddr,
            data->captureBufInfo.cirBufSize,
            data->captureBufInfo.periodSize, direction, flags);
        if (desc == NULL) {
            AUDIO_DRIVER_LOG_ERR("desc is null.");
            return HDF_FAILURE;
        }
        dmaRtd->cookie[1] = dmaengine_submit(desc);
    } else { // AUDIO_RENDER_STREAM
        direction = DMA_MEM_TO_DEV;
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaRtd->dmaChn[0] == NULL || dmaRtd->dmaChn[0]->device == NULL
            || dmaRtd->dmaChn[0]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        AUDIO_DRIVER_LOG_DEBUG("renderBufInfo.phyAddr = 0x%x.", data->renderBufInfo.phyAddr);
        desc = dmaengine_prep_dma_cyclic(dmaChan[0],
            data->renderBufInfo.phyAddr,
            data->renderBufInfo.cirBufSize,
            data->renderBufInfo.periodSize, direction, flags);
        if (desc == NULL) {
            AUDIO_DRIVER_LOG_ERR("desc is null.");
            return HDF_FAILURE;
        }
        dmaRtd->cookie[0] = dmaengine_submit(desc);
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t RK3399DmaPending(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct DmaRuntimeData *dmaRtd = NULL;
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }
    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaPrv is null.");
        return HDF_FAILURE;
    }
    if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaRtd->dmaChn[1] == NULL || dmaRtd->dmaChn[1]->device == NULL
            || dmaRtd->dmaChn[1]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        dma_async_issue_pending(dmaChan[1]);
    } else { // AUDIO_RENDER_STREAM
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaRtd->dmaChn[0] == NULL || dmaRtd->dmaChn[0]->device == NULL
            || dmaRtd->dmaChn[0]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        dma_async_issue_pending(dmaChan[0]);
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t RK3399DmaPause(struct PlatformData *data, const enum AudioStreamType streamType)
{
    int ret = HDF_SUCCESS;
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    struct DmaRuntimeData *dmaRtd = NULL;
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    if (data == NULL || (struct DmaRuntimeData *)data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null.");
        return HDF_FAILURE;
    }
    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL || (struct dma_chan *)dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaPrv is null.");
        return HDF_FAILURE;
    }
    if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaRtd->dmaChn[1] == NULL || dmaRtd->dmaChn[1]->device == NULL
            || dmaRtd->dmaChn[1]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        ret = dmaengine_terminate_async(dmaChan[1]);
    } else { // AUDIO_RENDER_STREAM
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaRtd->dmaChn[0] == NULL || dmaRtd->dmaChn[0]->device == NULL
            || dmaRtd->dmaChn[0]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        ret = dmaengine_terminate_async(dmaChan[0]);
    }
    if (ret != 0) {
        AUDIO_DRIVER_LOG_ERR("dmaengine_pause failed.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t RK3399DmaResume(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan[MAX_CHAN_COUNT];
    struct DmaRuntimeData *dmaRtd = NULL;
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    if (data == NULL || (struct DmaRuntimeData *)data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    dmaRtd = (struct DmaRuntimeData *)data->dmaPrv;
    if (dmaRtd == NULL || (struct dma_chan *)dmaRtd == NULL) {
        AUDIO_DRIVER_LOG_ERR("dmaPrv is null.");
        return HDF_FAILURE;
    }
    RK3399DmaSubmit(data, streamType);
    if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan[1] = (struct dma_chan *)dmaRtd->dmaChn[1];
        if (dmaRtd->dmaChn[1] == NULL || dmaRtd->dmaChn[1]->device == NULL
            || dmaRtd->dmaChn[1]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        dma_async_issue_pending(dmaChan[1]);
    } else { // AUDIO_RENDER_STREAM
        dmaChan[0] = (struct dma_chan *)dmaRtd->dmaChn[0];
        if (dmaRtd->dmaChn[0] == NULL || dmaRtd->dmaChn[0]->device == NULL
            || dmaRtd->dmaChn[0]->device->dev == NULL) {
            AUDIO_DRIVER_LOG_ERR("dmaChan is null.");
            return HDF_FAILURE;
        }
        dma_async_issue_pending(dmaChan[0]);
    }
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

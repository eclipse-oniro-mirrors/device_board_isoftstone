/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include "audio_platform_if.h"
#include "audio_sapm.h"
#include "audio_stream_dispatch.h"
#include "audio_driver_log.h"

#include "t507_dma_ops.h"

#define HDF_LOG_TAG dma_ops

enum {
    DMA_STREAM_TX = 0,
    DMA_STREAM_RX,
    DMA_STREAM_CNT,
};

struct DmaRuntimeData {
    struct device *dma_dev[DMA_STREAM_CNT];
    struct dma_chan *dma_chan[DMA_STREAM_CNT];
    dma_cookie_t cookie[DMA_STREAM_CNT];

    uint32_t streamType;
};

static struct DmaRuntimeData g_prtd;

/* note:
 * render -> internal codec
 * capture -> ahub & ac107
 */
static const char *g_codec_dtstreepath	= "/soc@3000000/codec@5096000";
static const char *g_ahub_dtstreepath	= "/soc@3000000/ahub@5097000";

static struct device *get_dma_device(const char *dtstreepath)
{
    struct device_node *dma_of_node;
    struct platform_device *pdev;

    AUDIO_DRIVER_LOG_DEBUG("dtstreepath %s", dtstreepath);
    dma_of_node = of_find_node_by_path(dtstreepath);
    if (dma_of_node == NULL) {
        AUDIO_DRIVER_LOG_ERR("of_find_node_by_path failed.");
        return NULL;
    }
    pdev = of_find_device_by_node(dma_of_node);
    if (pdev == NULL) {
        AUDIO_DRIVER_LOG_ERR("of_find_device_by_node failed.");
        return NULL;
    }

    return (&pdev->dev);
}

struct dma_chan *snd_dmaengine_pcm_request_channel(dma_filter_fn filter_fn, void *filter_data)
{
    dma_cap_mask_t mask;

    dma_cap_zero(mask);
    dma_cap_set(DMA_SLAVE, mask);
    dma_cap_set(DMA_CYCLIC, mask);

    return dma_request_channel(mask, filter_fn, filter_data);
}

static int audio_dma_request(void)
{
    /* note: for internal codec */
    g_prtd.dma_dev[DMA_STREAM_TX] = get_dma_device(g_codec_dtstreepath);
    if (!(g_prtd.dma_dev[DMA_STREAM_TX])) {
        AUDIO_DRIVER_LOG_ERR("get_dma_device failed.");
        return HDF_FAILURE;
    }

    /* g_prtd.dma_chan[DMA_STREAM_TX] = snd_dmaengine_pcm_request_channel(NULL, NULL); */
    g_prtd.dma_chan[DMA_STREAM_TX] = dma_request_chan(g_prtd.dma_dev[DMA_STREAM_TX], "tx");
    if (IS_ERR(g_prtd.dma_chan[DMA_STREAM_TX])) {
        AUDIO_DRIVER_LOG_ERR("DMA channels request tx failed, err -> %d.\n", IS_ERR(g_prtd.dma_chan[DMA_STREAM_TX]));
        return HDF_FAILURE;
    }

    /* note: for ahub (i2s with hub function) */
    g_prtd.dma_dev[DMA_STREAM_RX] = get_dma_device(g_ahub_dtstreepath);
    if (!(g_prtd.dma_dev[DMA_STREAM_RX])) {
        AUDIO_DRIVER_LOG_ERR("get_dma_device failed.");
        return HDF_FAILURE;
    }

    /* g_prtd.dma_chan[DMA_STREAM_RX] = snd_dmaengine_pcm_request_channel(NULL, NULL); */
    g_prtd.dma_chan[DMA_STREAM_RX] = dma_request_chan(g_prtd.dma_dev[DMA_STREAM_RX], "rx");
    if (IS_ERR(g_prtd.dma_chan[DMA_STREAM_RX])) {
        AUDIO_DRIVER_LOG_ERR("DMA channels request rx failed, err -> %d.\n", IS_ERR(g_prtd.dma_chan[DMA_STREAM_RX]));
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t T507AudioDmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platformDevice)
{
    int ret;

    AUDIO_DRIVER_LOG_DEBUG("entry");

    if (card == NULL || platformDevice == NULL || platformDevice->devData == NULL) {
        AUDIO_DRIVER_LOG_ERR("platformDevice is NULL.");
        return HDF_FAILURE;
    }
    if (platformDevice->devData->platformInitFlag == true) {
        AUDIO_DRIVER_LOG_ERR("platform init complete!");
        return HDF_SUCCESS;
    }

    /* note: include internal codec and ahub */
    ret = audio_dma_request();
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("audio_dma_request failed");
        return HDF_FAILURE;
    }

    platformDevice->devData->platformInitFlag = true;
    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t T507AudioDmaBufAlloc(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct device *dma_dev;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        if (data->renderBufInfo.virtAddr == NULL) {
            dma_dev = g_prtd.dma_dev[DMA_STREAM_TX];
            dma_dev->coherent_dma_mask = 0xffffffffUL;
            data->renderBufInfo.virtAddr = dma_alloc_wc(dma_dev, data->renderBufInfo.cirBufMax,
                                                        (dma_addr_t *)&data->renderBufInfo.phyAddr,
                                                        GFP_DMA | GFP_KERNEL);
            if (data->renderBufInfo.virtAddr == NULL) {
                AUDIO_DRIVER_LOG_ERR("dma_alloc_wc faild");
                return HDF_FAILURE;
            }
        }
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        if (data->captureBufInfo.virtAddr == NULL) {
            dma_dev = g_prtd.dma_dev[DMA_STREAM_RX];
            dma_dev->coherent_dma_mask = 0xffffffffUL;
            data->captureBufInfo.virtAddr = dma_alloc_wc(dma_dev, data->captureBufInfo.cirBufMax,
                                                         (dma_addr_t *)&data->captureBufInfo.phyAddr,
                                                         GFP_DMA | GFP_KERNEL);
            if (data->captureBufInfo.virtAddr == NULL) {
                AUDIO_DRIVER_LOG_ERR("dma_alloc_wc faild");
                return HDF_FAILURE;
            }
        }
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    data->dmaPrv = &g_prtd;
    g_prtd.streamType = streamType;

    AUDIO_DRIVER_LOG_DEBUG("success.");

    return HDF_SUCCESS;
}

int32_t T507AudioDmaBufFree(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct device *dma_dev;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        if (data->renderBufInfo.virtAddr != NULL) {
            dma_dev = g_prtd.dma_dev[DMA_STREAM_TX];
            dma_free_wc(dma_dev, data->renderBufInfo.cirBufMax, data->renderBufInfo.virtAddr, data->renderBufInfo.phyAddr);
        }
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        if (data->captureBufInfo.virtAddr != NULL) {
            dma_dev = g_prtd.dma_dev[DMA_STREAM_RX];
            dma_free_wc(dma_dev, data->captureBufInfo.cirBufMax, data->captureBufInfo.virtAddr, data->captureBufInfo.phyAddr);
        }
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t T507AudioDmaRequestChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    AUDIO_DRIVER_LOG_DEBUG("entry");
    return HDF_SUCCESS;
}

int32_t T507AudioDmaConfigChannel(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    int32_t ret = 0;
    struct dma_chan *dmaChan;
    struct dma_slave_config slaveConfig;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    (void)memset_s(&slaveConfig, sizeof(slaveConfig), 0, sizeof(slaveConfig));

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_TX];
        slaveConfig.direction = DMA_MEM_TO_DEV;
        switch (data->renderPcmInfo.bitWidth) {
            case 8:
                slaveConfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
                break;
            case 16:
                slaveConfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
                break;
            case 32:
                slaveConfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
                break;
            default:
                AUDIO_DRIVER_LOG_ERR("unsupport bitWidth -> %u", data->renderPcmInfo.bitWidth);
                return HDF_FAILURE;
        }
        slaveConfig.src_addr_width = slaveConfig.dst_addr_width;
        slaveConfig.dst_maxburst = 4;
        slaveConfig.dst_addr = SUNXI_CODEC_ADDR_BASE + SUNXI_DAC_TXDATA;
        /* slaveConfig.slave_id = sunxi_slave_id(DRQDST_AUDIO_CODEC, DRQSRC_SDRAM); */
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_RX];
        slaveConfig.direction = DMA_DEV_TO_MEM;
        switch (data->capturePcmInfo.bitWidth) {
            case 8:
                slaveConfig.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
                break;
            case 16:
                slaveConfig.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
                break;
            case 32:
                slaveConfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
                break;
            default:
                AUDIO_DRIVER_LOG_ERR("unsupport bitWidth -> %u", data->capturePcmInfo.bitWidth);
                return HDF_FAILURE;
        }
        slaveConfig.dst_addr_width = slaveConfig.src_addr_width;
        slaveConfig.src_maxburst = 4;
        slaveConfig.src_addr = SUNXI_AHUB_ADDR_BASE + SUNXI_AHUB_APBIF_RXFIFO(AHUB_APBIF_USE);
        /* slaveConfig.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_AHUB0_RX); */
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }
    slaveConfig.device_fc = false;

    ret = dmaengine_slave_config(dmaChan, &slaveConfig);
    if (ret != 0) {
        AUDIO_DRIVER_LOG_ERR("dmaengine_slave_config failed");
        return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG("success!");

    return HDF_SUCCESS;
}

int32_t T507AudioDmaPrep(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    AUDIO_DRIVER_LOG_DEBUG("entry");
    return HDF_SUCCESS;
}

int32_t T507AudioDmaSubmit(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_async_tx_descriptor *desc;
    enum dma_transfer_direction direction;
    unsigned long flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (streamType == AUDIO_RENDER_STREAM) {
        direction = DMA_MEM_TO_DEV;
        desc = dmaengine_prep_dma_cyclic(g_prtd.dma_chan[DMA_STREAM_TX],
                                         data->renderBufInfo.phyAddr,
                                         data->renderBufInfo.cirBufSize,
                                         data->renderBufInfo.periodSize,
                                         direction, flags);
        if (!desc) {
            AUDIO_DRIVER_LOG_ERR("DMA_STREAM_TX desc create failed");
            return -ENOMEM;
        }
        g_prtd.cookie[DMA_STREAM_TX] = dmaengine_submit(desc);
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        direction = DMA_DEV_TO_MEM;
        desc = dmaengine_prep_dma_cyclic(g_prtd.dma_chan[DMA_STREAM_RX],
                                         data->captureBufInfo.phyAddr,
                                         data->captureBufInfo.cirBufSize,
                                         data->captureBufInfo.periodSize,
                                         direction, flags);
        if (!desc) {
            AUDIO_DRIVER_LOG_ERR("DMA_RX_CHANNEL desc create failed");
            return -ENOMEM;
        }
        g_prtd.cookie[DMA_STREAM_RX] = dmaengine_submit(desc);
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

int32_t T507AudioDmaPending(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_TX];
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_RX];
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }
    dma_async_issue_pending(dmaChan);
    AUDIO_DRIVER_LOG_DEBUG("dmaChan chan_id = %d.", dmaChan->chan_id);

    return HDF_SUCCESS;
}

int32_t T507AudioDmaPause(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct dma_chan *dmaChan;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_TX];
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_RX];
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }
    dmaengine_terminate_async(dmaChan);

    return HDF_SUCCESS;
}

int32_t T507AudioDmaResume(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    int ret;
    struct dma_chan *dmaChan;

    AUDIO_DRIVER_LOG_DEBUG("streamType = %d", streamType);

    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_TX];
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_RX];
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    ret = T507AudioDmaSubmit(data, streamType);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("call DmaSubmit failed");
        return HDF_FAILURE;
    }
    dma_async_issue_pending(dmaChan);

    return HDF_SUCCESS;
}

static inline signed long BytesToFrames(uint32_t frameSize, uint32_t size)
{
    if (frameSize == 0 || size == 0) {
        return 0;
    }

    return size / frameSize;
}

int32_t T507AudioDmaPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer)
{
    struct dma_chan *dmaChan;
    struct dma_tx_state state;
    enum dma_status status;
    uint32_t buf_size;
    uint32_t pos = 0;

    if (data == NULL || pointer == NULL) {
        AUDIO_DRIVER_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_TX];
        buf_size = data->renderBufInfo.cirBufSize;
        status = dmaengine_tx_status(dmaChan, g_prtd.cookie[DMA_STREAM_TX], &state);
        if (status == DMA_IN_PROGRESS || status == DMA_PAUSED) {
            if (state.residue > 0 && state.residue <= buf_size) {
                pos = buf_size - state.residue;
            }
        }
        *pointer = BytesToFrames(data->renderPcmInfo.frameSize, pos);
    } else if (streamType == AUDIO_CAPTURE_STREAM) {
        dmaChan = g_prtd.dma_chan[DMA_STREAM_RX];
        buf_size = data->captureBufInfo.cirBufSize;
        status = dmaengine_tx_status(dmaChan, g_prtd.cookie[DMA_STREAM_RX], &state);
        if (status == DMA_IN_PROGRESS || status == DMA_PAUSED) {
            if (state.residue > 0 && state.residue <= buf_size) {
                pos = buf_size - state.residue;
            }
        }
        *pointer = BytesToFrames(data->capturePcmInfo.frameSize, pos);
    } else {
        AUDIO_DRIVER_LOG_ERR("stream Type is fail.");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

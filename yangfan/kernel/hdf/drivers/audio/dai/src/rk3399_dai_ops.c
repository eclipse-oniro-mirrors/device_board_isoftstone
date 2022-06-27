/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "rk3399_dai_ops.h"
#include "audio_host.h"
#include "audio_control.h"
#include "audio_dai_if.h"
#include "audio_dai_base.h"
#include "audio_platform_base.h"
#include "audio_driver_log.h"
#include "osal_io.h"
#include "osal_time.h"
#include "audio_stream_dispatch.h"

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/clk/rockchip.h>
#include <sound/dmaengine_pcm.h>
#include <linux/pm_runtime.h>


#define HDF_LOG_TAG rk3399_dai_ops
#define HALF_MILLION     (500000)
#define ONE_MILLION      (1000000)
#define NUM_TWO          (2)
#define NUM_SIXTEEN      (16)
#define NUM_TWENTY_FOUR  (24)

void *g_regDaiBase = NULL;

struct regmap *regmap = NULL;
/* I2S REGS */
#define I2S_TXCR    (0x0000)
#define I2S_RXCR    (0x0004)
#define I2S_CKR     (0x0008)
#define I2S_FIFOLR  (0x000c)
#define I2S_DMACR   (0x0010)
#define I2S_INTCR   (0x0014)
#define I2S_INTSR   (0x0018)
#define I2S_XFER    (0x001c)
#define I2S_CLR     (0x0020)
#define I2S_TXDR    (0x0024)
#define I2S_RXDR    (0x0028)

/*
 * DMACR
 * DMA control register
*/
#define I2S_DMACR_RDE_SHIFT 24
#define I2S_DMACR_RDE_DISABLE   (0 << I2S_DMACR_RDE_SHIFT)
#define I2S_DMACR_RDE_ENABLE    (1 << I2S_DMACR_RDE_SHIFT)
#define I2S_DMACR_RDL_SHIFT 16
#define I2S_DMACR_RDL_MASK  (0x1f << I2S_DMACR_RDL_SHIFT)
#define I2S_DMACR_TDE_SHIFT 8
#define I2S_DMACR_TDE_DISABLE   (0 << I2S_DMACR_TDE_SHIFT)
#define I2S_DMACR_TDE_ENABLE    (1 << I2S_DMACR_TDE_SHIFT)
#define I2S_DMACR_TDL_SHIFT 0
#define I2S_DMACR_TDL_MASK  (0x1f << I2S_DMACR_TDL_SHIFT)

/*
 * XFER
 * Transfer start register
*/
#define I2S_XFER_RXS_SHIFT  1
#define I2S_XFER_RXS_STOP   (0 << I2S_XFER_RXS_SHIFT)
#define I2S_XFER_RXS_START  (1 << I2S_XFER_RXS_SHIFT)
#define I2S_XFER_TXS_SHIFT  0
#define I2S_XFER_TXS_STOP   (0 << I2S_XFER_TXS_SHIFT)
#define I2S_XFER_TXS_START  (1 << I2S_XFER_TXS_SHIFT)

/*
 * CLR
 * clear SCLK domain logic register
*/
#define I2S_CLR_RXC BIT(1)
#define I2S_CLR_TXC BIT(0)

#define I2S_CKR_MDIV_SHIFT  16
#define I2S_CKR_TSD_SHIFT   0
#define I2S_CKR_RSD_SHIFT   8

#define I2S_CKR_MDIV_MASK   (0xff << I2S_CKR_MDIV_SHIFT)

#define I2S_CKR_TSD_MASK    (0xff << I2S_CKR_TSD_SHIFT)
#define I2S_CKR_RSD_MASK    (0xff << I2S_CKR_RSD_SHIFT)

#define I2S_CKR_TRCM_SHIFT  28
#define I2S_CKR_TRCM(x) ((x) << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_TXRX   (0 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_TXONLY (1 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_RXONLY (2 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_MASK   (3 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_MSS_SHIFT   27
#define I2S_CKR_MSS_MASTER  (0 << I2S_CKR_MSS_SHIFT)
#define I2S_CKR_MSS_SLAVE   (1 << I2S_CKR_MSS_SHIFT)
#define I2S_CKR_MSS_MASK    (1 << I2S_CKR_MSS_SHIFT)
#define I2S_CKR_CKP_SHIFT   26
#define I2S_CKR_CKP_NORMAL  (0 << I2S_CKR_CKP_SHIFT)
#define I2S_CKR_CKP_INVERTED    (1 << I2S_CKR_CKP_SHIFT)
#define I2S_CKR_CKP_MASK    (1 << I2S_CKR_CKP_SHIFT)
#define I2S_CKR_RLP_SHIFT   25
#define I2S_CKR_RLP_NORMAL  (0 << I2S_CKR_RLP_SHIFT)
#define I2S_CKR_RLP_INVERTED    (1 << I2S_CKR_RLP_SHIFT)
#define I2S_CKR_RLP_MASK    (1 << I2S_CKR_RLP_SHIFT)
#define I2S_CKR_TLP_SHIFT   24
#define I2S_CKR_TLP_NORMAL  (0 << I2S_CKR_TLP_SHIFT)
#define I2S_CKR_TLP_INVERTED    (1 << I2S_CKR_TLP_SHIFT)
#define I2S_CKR_TLP_MASK    (1 << I2S_CKR_TLP_SHIFT)
#define I2S_CKR_MDIV_SHIFT  16
#define I2S_CKR_MDIV_MASK   (0xff << I2S_CKR_MDIV_SHIFT)
#define I2S_CKR_RSD_SHIFT   8
#define I2S_CKR_RSD_MASK    (0xff << I2S_CKR_RSD_SHIFT)
#define I2S_CKR_TSD_SHIFT   0
#define I2S_CKR_TSD_MASK    (0xff << I2S_CKR_TSD_SHIFT)

/* io direction cfg register */
#define I2S_IO_DIRECTION_MASK   (7)
#define I2S_IO_8CH_OUT_2CH_IN   (0)
#define I2S_IO_6CH_OUT_4CH_IN   (4)
#define I2S_IO_4CH_OUT_6CH_IN   (6)
#define I2S_IO_2CH_OUT_8CH_IN   (7)
/* channel select */
#define I2S_CSR_SHIFT   15
#define I2S_CHN_2   (0 << I2S_CSR_SHIFT)
#define I2S_CHN_4   (1 << I2S_CSR_SHIFT)
#define I2S_CHN_6   (2 << I2S_CSR_SHIFT)
#define I2S_CHN_8   (3 << I2S_CSR_SHIFT)
/*
 * TXDR
 * Transimt FIFO data register, write only.
*/
#define I2S_TXDR_MASK   (0xff)

/*
 * RXDR
 * Receive FIFO data register, write only.
*/
#define I2S_RXDR_MASK   (0xff)
/*
 * TXCR
 * transmit operation control register
*/
#define I2S_TXCR_RCNT_SHIFT 17
#define I2S_TXCR_RCNT_MASK  (0x3f << I2S_TXCR_RCNT_SHIFT)
#define I2S_TXCR_CSR_SHIFT  15
#define I2S_TXCR_CSR_MASK   (3 << I2S_TXCR_CSR_SHIFT)
#define I2S_TXCR_HWT        BIT(14)
#define I2S_TXCR_SJM_SHIFT  12
#define I2S_TXCR_SJM_R      (0 << I2S_TXCR_SJM_SHIFT)
#define I2S_TXCR_SJM_L      (1 << I2S_TXCR_SJM_SHIFT)
#define I2S_TXCR_FBM_SHIFT  11
#define I2S_TXCR_FBM_MSB    (0 << I2S_TXCR_FBM_SHIFT)
#define I2S_TXCR_FBM_LSB    (1 << I2S_TXCR_FBM_SHIFT)
#define I2S_TXCR_IBM_SHIFT  9
#define I2S_TXCR_IBM_NORMAL (0 << I2S_TXCR_IBM_SHIFT)
#define I2S_TXCR_IBM_LSJM   (1 << I2S_TXCR_IBM_SHIFT)
#define I2S_TXCR_IBM_RSJM   (2 << I2S_TXCR_IBM_SHIFT)
#define I2S_TXCR_IBM_MASK   (3 << I2S_TXCR_IBM_SHIFT)
#define I2S_TXCR_PBM_SHIFT  7
#define I2S_TXCR_PBM_MASK   (3 << I2S_TXCR_PBM_SHIFT)
#define I2S_TXCR_TFS_SHIFT  5
#define I2S_TXCR_TFS_I2S    (0 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_PCM    (1 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_MASK   (1 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_VDW_SHIFT  0
#define I2S_TXCR_VDW_MASK   (0x1f << I2S_TXCR_VDW_SHIFT)


#define I2S_RXCR_CSR_SHIFT  15
#define I2S_RXCR_CSR_MASK   (3 << I2S_RXCR_CSR_SHIFT)
#define I2S_RXCR_HWT        BIT(14)
#define I2S_RXCR_SJM_SHIFT  12
#define I2S_RXCR_SJM_R      (0 << I2S_RXCR_SJM_SHIFT)
#define I2S_RXCR_SJM_L      (1 << I2S_RXCR_SJM_SHIFT)
#define I2S_RXCR_FBM_SHIFT  11
#define I2S_RXCR_FBM_MSB    (0 << I2S_RXCR_FBM_SHIFT)
#define I2S_RXCR_FBM_LSB    (1 << I2S_RXCR_FBM_SHIFT)
#define I2S_RXCR_IBM_SHIFT  9
#define I2S_RXCR_IBM_NORMAL (0 << I2S_RXCR_IBM_SHIFT)
#define I2S_RXCR_IBM_LSJM   (1 << I2S_RXCR_IBM_SHIFT)
#define I2S_RXCR_IBM_RSJM   (2 << I2S_RXCR_IBM_SHIFT)
#define I2S_RXCR_IBM_MASK   (3 << I2S_RXCR_IBM_SHIFT)
#define I2S_RXCR_PBM_SHIFT  7
#define I2S_RXCR_PBM_MASK   (3 << I2S_RXCR_PBM_SHIFT)
#define I2S_RXCR_TFS_SHIFT  5
#define I2S_RXCR_TFS_I2S    (0 << I2S_RXCR_TFS_SHIFT)
#define I2S_RXCR_TFS_PCM    (1 << I2S_RXCR_TFS_SHIFT)
#define I2S_RXCR_TFS_MASK   (1 << I2S_RXCR_TFS_SHIFT)
#define I2S_RXCR_VDW_SHIFT  0
#define I2S_RXCR_VDW_MASK   (0x1f << I2S_RXCR_VDW_SHIFT)

inline uint32_t I2sRxcrPbmMode(uint32_t x) {
    return ((x) << I2S_RXCR_PBM_SHIFT);
}

inline uint32_t I2sDmacrRdl(uint32_t x) {
    return (((x) - 1) << I2S_DMACR_RDL_SHIFT);
}

inline uint32_t I2sDmacrTdl(uint32_t x) {
    return ((x) << I2S_DMACR_TDL_SHIFT);
}

inline uint32_t I2sCkrMdiv(uint32_t x) {
    return (((x) - 1) << I2S_CKR_MDIV_SHIFT);
}

inline uint32_t I2sCkrTsd(uint32_t x) {
    return (((x) - 1) << I2S_CKR_TSD_SHIFT);
}

inline uint32_t I2sCkrRsd(uint32_t x) {
    return (((x) - 1) << I2S_CKR_RSD_SHIFT);
}

inline uint32_t I2sRxcrCsr(uint32_t x) {
    return ((x) << I2S_RXCR_CSR_SHIFT);
}

inline uint32_t I2sTxcrCsr(uint32_t x) {
    return ((x) << I2S_TXCR_CSR_SHIFT);
}

inline uint32_t I2sTxcrPbmMode(uint32_t x) {
    return ((x) << I2S_TXCR_PBM_SHIFT);
}

inline uint32_t I2sTxcrVdw(uint32_t x) {
    return (((x) - 1) << I2S_TXCR_VDW_SHIFT);
}

inline uint32_t I2sRxcrVdw(uint32_t x) {
    return (((x) - 1) << I2S_RXCR_VDW_SHIFT);
}

struct rk_i2s_pins {
    u32 reg_offset;
    u32 shift;
};

struct rk_i2s_dev {
    struct device *dev;

    struct clk *hclk;
    struct clk *mclk;
    struct clk *mclk_root;

    struct snd_dmaengine_dai_dma_data capture_dma_data;
    struct snd_dmaengine_dai_dma_data playback_dma_data;

    struct regmap *regmap;
    struct regmap *grf;
    struct reset_control *reset_m;
    struct reset_control *reset_h;

    /*
     * Used to indicate the tx/rx status.
     * I2S controller hopes to start the tx and rx together,
     * also to stop them when they are both try to stop.
    */
    bool tx_start;
    bool rx_start;
    bool is_master_mode;
    const struct rk_i2s_pins *pins;
    unsigned int bclk_fs;
    unsigned int clk_trcm;

    unsigned int mclk_root_rate;
    unsigned int mclk_root_initial_rate;
    int clk_ppm;
    bool mclk_calibrate;
};

struct rk_i2s_dev *g_rkI2SDev = NULL;

struct rk_i2s_dev *GetRockChipI2SDevice(void);

int32_t DaiDeviceReadReg(unsigned long virtualAddress, uint32_t reg, uint32_t *val)
{
    if (val == NULL) {
        AUDIO_DRIVER_LOG_ERR("param val is null.");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("reg virtualAddress = 0x%x  val = 0x%x ", virtualAddress, reg);

    if (reg == 0xffffff) {
        AUDIO_DRIVER_LOG_DEBUG("reg val is invalue.");
        return HDF_SUCCESS;
    }
    *val = 0;

    AUDIO_DRIVER_LOG_DEBUG("RockChipRead reg = 0x%x val = 0x%x.", reg, *val);

    return HDF_SUCCESS;
}

int32_t DaiDeviceWriteReg(unsigned long virtualAddress, uint32_t reg, uint32_t value)
{
    if (reg == 0xffffff) {
        AUDIO_DRIVER_LOG_DEBUG("reg val is invalue.");
        return HDF_SUCCESS;
    }

    AUDIO_DRIVER_LOG_DEBUG("reg virtualAddress = 0x%x val = 0x%x  val = %d", virtualAddress, reg, value);

    return HDF_SUCCESS;
}

int32_t DaiDeviceRegUpdateBits(unsigned int reg, unsigned int mask, unsigned int val)
{
    AUDIO_DRIVER_LOG_DEBUG("reg reg = 0x%x mask = 0x%x  val = %d", reg, mask, val);
    return HDF_SUCCESS;
}

int32_t DaiDeviceInit(struct AudioCard *audioCard, const struct DaiDevice *dai)
{
    int regAddr;
    int ret;

    AUDIO_DRIVER_LOG_DEBUG("entry.");

    if (dai == NULL || dai->devData == NULL) {
        AUDIO_DRIVER_LOG_ERR("dai is nullptr.");
        return HDF_FAILURE;
    }
    struct DaiData *data = dai->devData;
    struct AudioRegCfgData *regConfig = dai->devData->regConfig;
    if (regConfig == NULL) {
        AUDIO_DRIVER_LOG_ERR("regConfig is nullptr.");
        return HDF_FAILURE;
    }

    data->regVirtualAddr = regAddr;

    if (DaiSetConfigInfo(data) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("set config info fail.");
        return HDF_FAILURE;
    }

    ret = AudioAddControls(audioCard, data->controls, data->numControls);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add controls failed.");
        return HDF_FAILURE;
    }

    if (data->daiInitFlag == true) {
        AUDIO_DRIVER_LOG_DEBUG("dai init complete!");
        return HDF_SUCCESS;
    }

    data->daiInitFlag = true;

    AUDIO_DRIVER_LOG_DEBUG("success.");

    return HDF_SUCCESS;
}

int32_t DaiTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *device)
{
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    bool isStart = false;

    (void)card;
    (void)device;

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
            AUDIO_DRIVER_LOG_DEBUG("---------RENDER_START--------------.");
            regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_TDE_ENABLE, I2S_DMACR_TDE_ENABLE);
            isStart = true;
            break;
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
            AUDIO_DRIVER_LOG_DEBUG("---------RENDER_PAUSE_OR_STOP--------------.");
            regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_TDE_ENABLE, I2S_DMACR_TDE_DISABLE);
            break;

        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
            AUDIO_DRIVER_LOG_DEBUG("---------CAPTURE_START--------------.");
            regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_RDE_ENABLE, I2S_DMACR_RDE_ENABLE);
            isStart = true;
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            AUDIO_DRIVER_LOG_DEBUG("---------CAPTURE_PAUSE_OR_STOP--------------.");
            g_rkI2SDev->rx_start = false;
            regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_RDE_ENABLE, I2S_DMACR_RDE_DISABLE);
            break;

        default:
            AUDIO_DRIVER_LOG_ERR("invalude cmd id: %d.", cmd);
            return HDF_FAILURE;
    }

    if (isStart) {
        AUDIO_DRIVER_LOG_DEBUG("---------isStart--------------.");
        regmap_update_bits(g_rkI2SDev->regmap, I2S_XFER, I2S_XFER_TXS_START | I2S_XFER_RXS_START,
            I2S_XFER_TXS_START | I2S_XFER_RXS_START);
    } else { // if (!g_rkI2SDev->rx_start) {
        AUDIO_DRIVER_LOG_DEBUG("---------g_rkI2SDev->rx_start=false--------------.");
        regmap_update_bits(g_rkI2SDev->regmap, I2S_XFER, I2S_XFER_TXS_START | I2S_XFER_RXS_START,
            I2S_XFER_TXS_STOP | I2S_XFER_RXS_STOP);

        udelay(0d150);
        regmap_update_bits(g_rkI2SDev->regmap, I2S_CLR, I2S_CLR_TXC | I2S_CLR_RXC,
            I2S_CLR_TXC | I2S_CLR_RXC);
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int rockchip_i2s_clk_set_rate(struct rk_i2s_dev *i2s,
    struct clk *clk, unsigned long rate,
    int ppm)
{
    unsigned long rate_target;
    int delta, ret;

    if (ppm == i2s->clk_ppm) {
        return 0;
    }  

    ret = rockchip_pll_clk_compensation(clk, ppm);
    if (ret != -ENOSYS)
        goto out;

    delta = (ppm < 0) ? -1 : 1;
    delta *= (int)div64_u64((uint64_t)rate * (uint64_t)abs(ppm) + HALF_MILLION, ONE_MILLION);

    rate_target = rate + delta;

    if (!rate_target) {
        return HDF_FAILURE;
    }

    ret = clk_set_rate(clk, rate_target);
    if (ret) {
        return ret;
    }

out:
    if (!ret)
        i2s->clk_ppm = ppm;

    return ret;
}

static int rockchip_i2s_set_fmt(void)
{
    unsigned int mask = 0, val = 0;
    int ret = 0;
    AUDIO_DRIVER_LOG_DEBUG("%s -------------entry ----------- \n", __func__);
    unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
    pm_runtime_get_sync(g_rkI2SDev->dev);

    mask = I2S_CKR_MSS_MASK;
    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
            /* Set source clock in Master mode */
            val = I2S_CKR_MSS_MASTER;
            g_rkI2SDev->is_master_mode = true;
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
            val = I2S_CKR_MSS_SLAVE;
            g_rkI2SDev->is_master_mode = false;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("------------default ----------- ");
            ret = HDF_FAILURE;
    }

    regmap_update_bits(g_rkI2SDev->regmap, I2S_CKR, mask, val);

    mask = I2S_CKR_CKP_MASK | I2S_CKR_TLP_MASK | I2S_CKR_RLP_MASK;
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
            val = I2S_CKR_CKP_NORMAL |
                I2S_CKR_TLP_NORMAL |
                I2S_CKR_RLP_NORMAL;
            break;
        case SND_SOC_DAIFMT_NB_IF:
            val = I2S_CKR_CKP_NORMAL |
                I2S_CKR_TLP_INVERTED |
                I2S_CKR_RLP_INVERTED;
            break;
        case SND_SOC_DAIFMT_IB_NF:
            val = I2S_CKR_CKP_INVERTED |
                I2S_CKR_TLP_NORMAL |
                I2S_CKR_RLP_NORMAL;
            break;
        case SND_SOC_DAIFMT_IB_IF:
            val = I2S_CKR_CKP_INVERTED |
                I2S_CKR_TLP_INVERTED |
                I2S_CKR_RLP_INVERTED;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("------------default ----------- ");
            ret = HDF_FAILURE;
    }

    regmap_update_bits(g_rkI2SDev->regmap, I2S_CKR, mask, val);

    mask = I2S_TXCR_IBM_MASK | I2S_TXCR_TFS_MASK | I2S_TXCR_PBM_MASK;
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_RIGHT_J:
            val = I2S_TXCR_IBM_RSJM;
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            val = I2S_TXCR_IBM_LSJM;
            break;
        case SND_SOC_DAIFMT_I2S:
            val = I2S_TXCR_IBM_NORMAL;
            break;
        case SND_SOC_DAIFMT_DSP_A: /* PCM delay 1 bit mode */
            val = I2S_TXCR_TFS_PCM | I2sTxcrPbmMode(1);
            break;
        case SND_SOC_DAIFMT_DSP_B: /* PCM no delay mode */
            val = I2S_TXCR_TFS_PCM;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("------------default ----------- ");
            ret = HDF_FAILURE;
    }

    regmap_update_bits(g_rkI2SDev->regmap, I2S_TXCR, mask, val);

    mask = I2S_RXCR_IBM_MASK | I2S_RXCR_TFS_MASK | I2S_RXCR_PBM_MASK;
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_RIGHT_J:
            val = I2S_RXCR_IBM_RSJM;
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            val = I2S_RXCR_IBM_LSJM;
            break;
        case SND_SOC_DAIFMT_I2S:
            val = I2S_RXCR_IBM_NORMAL;
            break;
        case SND_SOC_DAIFMT_DSP_A: /* PCM delay 1 bit mode */
            val = I2S_RXCR_TFS_PCM | I2sRxcrPbmMode(1);
            break;
        case SND_SOC_DAIFMT_DSP_B: /* PCM no delay mode */
            val = I2S_RXCR_TFS_PCM;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("------------default ----------- ");
            ret = HDF_FAILURE;
    }

    regmap_update_bits(g_rkI2SDev->regmap, I2S_RXCR, mask, val);
    AUDIO_DRIVER_LOG_ERR("------------success ----------- ");
    ret = HDF_SUCCESS;
    return ret;
}

int32_t DaiStartup(const struct AudioCard *card, const struct DaiDevice *device)
{
    unsigned int root_rate, div, delta;
    uint64_t ppm;
    unsigned int rate = 0xac4400;
    AUDIO_DRIVER_LOG_DEBUG("entry.");
    int ret;
    g_rkI2SDev = GetRockChipI2SDevice();

    if (g_rkI2SDev == NULL) {
        AUDIO_DRIVER_LOG_ERR("g_rkI2SDev is nullptr.");
        return HDF_FAILURE;
    }

    rockchip_i2s_set_fmt();

    if (g_rkI2SDev->mclk_calibrate) {
        AUDIO_DRIVER_LOG_DEBUG("------------mclk_calibrate entry----------.");
        ret = rockchip_i2s_clk_set_rate(g_rkI2SDev, g_rkI2SDev->mclk_root,
            g_rkI2SDev->mclk_root_rate, 0);
        if (ret) {
            AUDIO_DRIVER_LOG_DEBUG("------------mclk_calibrate ret=%d----------.", ret);
            return ret;
        }

        root_rate = g_rkI2SDev->mclk_root_rate;
        delta = abs(root_rate % rate - rate);
        AUDIO_DRIVER_LOG_DEBUG("------------mclk_calibrate root_rate=%d----delta=%d------.", root_rate, delta);
        ppm = div64_u64((uint64_t)delta * ONE_MILLION, (uint64_t)root_rate);
        if (ppm) {
            div = DIV_ROUND_CLOSEST(g_rkI2SDev->mclk_root_initial_rate, rate);
            if (!div) {
                AUDIO_DRIVER_LOG_DEBUG("------------mclk_calibrate rate=%d----------.", rate);
            }
            root_rate = rate * round_up(div, NUM_TWO);
            ret = clk_set_rate(g_rkI2SDev->mclk_root, root_rate);
            if (ret) {
                AUDIO_DRIVER_LOG_DEBUG("------------mclk_calibrate ret1=%d----------.", ret);
                return ret;
            }
            g_rkI2SDev->mclk_root_rate = clk_get_rate(g_rkI2SDev->mclk_root);
        }
    }

    ret = clk_set_rate(g_rkI2SDev->mclk, rate);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("Fail to set mclk %d", ret);
    }

    ret = clk_prepare_enable(g_rkI2SDev->mclk);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("mclock enable failed %d\n", ret);
        return ret;
    }
    ret = clk_prepare_enable(g_rkI2SDev->hclk);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("hclock enable failed %d\n", ret);
        return ret;
    }

    ret = clk_enable(g_rkI2SDev->hclk);
    if (ret) {
        AUDIO_DRIVER_LOG_DEBUG("Could not register PCM\n");
        return ret;
    }

    pm_runtime_enable(g_rkI2SDev->dev);

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t DaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param)
{
    uint32_t bitWidth;
    unsigned int val = 0;
    unsigned int mclk_rate, bclk_rate, div_bclk, div_lrck;

    AUDIO_DRIVER_LOG_DEBUG("entry.");

    if (card == NULL || card->rtd == NULL || card->rtd->cpuDai == NULL ||
        param == NULL || param->cardServiceName == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is nullptr.");
        return HDF_FAILURE;
    }

    if (DaiCheckSampleRate(param->rate) != HDF_SUCCESS) {
        return HDF_ERR_NOT_SUPPORT;
    }

    struct DaiData *data = DaiDataFromCard(card);
    if (data == NULL) {
        AUDIO_DRIVER_LOG_ERR("platformHost is nullptr.");
        return HDF_FAILURE;
    }

    data->pcmInfo.channels = param->channels;

    if (AudioFramatToBitWidth(param->format, &bitWidth) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    data->pcmInfo.bitWidth = bitWidth;
    data->pcmInfo.rate = param->rate;
    data->pcmInfo.streamType = param->streamType;
    if (g_rkI2SDev->is_master_mode) {

        mclk_rate = clk_get_rate(g_rkI2SDev->mclk);
        bclk_rate = g_rkI2SDev->bclk_fs * data->pcmInfo.rate;
        if (!bclk_rate) {
            AUDIO_DRIVER_LOG_ERR("bclk_rate is error.");
            return HDF_FAILURE;
        }
        div_bclk = DIV_ROUND_CLOSEST(mclk_rate, bclk_rate);
        div_lrck = bclk_rate / data->pcmInfo.rate;
        regmap_update_bits(g_rkI2SDev->regmap, I2S_CKR,
            I2S_CKR_MDIV_MASK,
            I2sCkrMdiv(div_bclk));

        regmap_update_bits(g_rkI2SDev->regmap, I2S_CKR,
            I2S_CKR_TSD_MASK |
            I2S_CKR_RSD_MASK,
            I2sCkrTsd(div_lrck) |
            I2sCkrRsd(div_lrck));
    }

    switch (data->pcmInfo.bitWidth) {
        case DATA_BIT_WIDTH16:
            val |= I2sTxcrVdw(NUM_SIXTEEN);
            break;

        case DATA_BIT_WIDTH24:
            val |= I2sTxcrVdw(NUM_TWENTY_FOUR);
            break;

        default:
            return HDF_FAILURE;
    }

    val |= I2S_CHN_2;

    if (data->pcmInfo.streamType == AUDIO_CAPTURE_STREAM)
        regmap_update_bits(g_rkI2SDev->regmap, I2S_RXCR,
            I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK,
            val);
    else
        regmap_update_bits(g_rkI2SDev->regmap, I2S_TXCR,
            I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK,
            val);

    if (!IS_ERR(g_rkI2SDev->grf) && g_rkI2SDev->pins) {
        regmap_read(g_rkI2SDev->regmap, I2S_TXCR, &val);
        val &= I2S_TXCR_CSR_MASK;

        switch (val) {
            case I2S_CHN_4:
                val = I2S_IO_4CH_OUT_6CH_IN;
                break;
            case I2S_CHN_6:
                val = I2S_IO_6CH_OUT_4CH_IN;
                break;
            case I2S_CHN_8:
                val = I2S_IO_8CH_OUT_2CH_IN;
                break;
            default:
                val = I2S_IO_2CH_OUT_8CH_IN;
                break;
        }

        val <<= g_rkI2SDev->pins->shift;
        val |= (I2S_IO_DIRECTION_MASK << g_rkI2SDev->pins->shift) << NUM_SIXTEEN;
        regmap_write(g_rkI2SDev->grf, g_rkI2SDev->pins->reg_offset, val);
    }

    regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_TDL_MASK,
        I2sDmacrTdl(NUM_SIXTEEN));
    regmap_update_bits(g_rkI2SDev->regmap, I2S_DMACR, I2S_DMACR_RDL_MASK,
        I2sDmacrRdl(NUM_SIXTEEN));

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

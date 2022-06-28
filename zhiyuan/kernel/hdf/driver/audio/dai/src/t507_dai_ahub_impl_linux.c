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

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/regmap.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "t507_dai_ahub_impl_linux.h"
#include "audio_control.h"
#include "audio_core.h"
#include "audio_driver_log.h"
#include "audio_platform_base.h"
#include "osal_io.h"
#include <asm/io.h>

#define DRV_NAME    "sunxi-snd-ahub"

static struct platform_device *g_ahub_pdev;

static struct regmap_config g_regmap_config = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,
    .max_register = SUNXI_AHUB_MAX_REG,
    .cache_type = REGCACHE_NONE,
};

struct sunxi_ahub_mem_info {
    struct resource res;
    void __iomem *membase;
    struct resource *memregion;
    struct regmap *regmap;
};

struct sunxi_ahub_clk_info {
    struct clk *clk_pll;
    struct clk *clk_pllx4;
    struct clk *clk_module;

    struct clk *clk_bus;
    struct reset_control *clk_rst;
};

struct sunxi_ahub_pinctl_info {
    struct pinctrl *pinctrl;
    struct pinctrl_state *pinstate;
    struct pinctrl_state *pinstate_sleep;

    bool pinctrl_used;
};

struct sunxi_ahub_dts_info {
    uint32_t dai_type;
    uint32_t apb_num;
    uint32_t tdm_num;
    uint32_t tx_pin;
    uint32_t rx_pin;

    /* value must be (2^n)Kbyte */
    size_t playback_cma;
    size_t playback_fifo_size;
    size_t capture_cma;
    size_t capture_fifo_size;
};

struct sunxi_ahub_regulator_info {
    struct regulator *regulator;
    const char *regulator_name;
};

struct sunxi_ahub_info {
    struct device *dev;

    struct sunxi_ahub_mem_info mem_info;
    struct sunxi_ahub_clk_info clk_info;
    struct sunxi_ahub_pinctl_info pin_info;
    struct sunxi_ahub_dts_info dts_info;
    struct sunxi_ahub_regulator_info rglt_info;

    /* for Hardware param setting */
    uint32_t fmt;
    uint32_t pllclk_freq;
    uint32_t moduleclk_freq;
    uint32_t mclk_freq;
    uint32_t lrck_freq;
    uint32_t bclk_freq;

    /* for hdmi audio */
    /* enum HDMI_FORMAT hdmi_fmt; */
};

#define REG_LABEL(constant) {#constant, constant, 0}
#define REG_LABEL_END       {NULL, 0, 0}

struct reg_label {
    const char *name;
    const uint32_t address;
    uint32_t value;
};
static struct reg_label g_reg_labels[] = {
    REG_LABEL(SUNXI_AHUB_CTL),
    REG_LABEL(SUNXI_AHUB_VER),
    REG_LABEL(SUNXI_AHUB_RST),
    REG_LABEL(SUNXI_AHUB_GAT),
    {"SUNXI_AHUB_APBIF_TX_CTL",         SUNXI_AHUB_APBIF_TX_CTL(0),         0},
    {"SUNXI_AHUB_APBIF_TX_IRQ_CTL",     SUNXI_AHUB_APBIF_TX_IRQ_CTL(0),     0},
    {"SUNXI_AHUB_APBIF_TX_IRQ_STA",     SUNXI_AHUB_APBIF_TX_IRQ_STA(0),     0},
    {"SUNXI_AHUB_APBIF_TXFIFO_CTL",     SUNXI_AHUB_APBIF_TXFIFO_CTL(0),     0},
    {"SUNXI_AHUB_APBIF_TXFIFO_STA",     SUNXI_AHUB_APBIF_TXFIFO_STA(0),     0},
    /* {"SUNXI_AHUB_APBIF_TXFIFO",      SUNXI_AHUB_APBIF_TXFIFO(0),         0}, */
    {"SUNXI_AHUB_APBIF_TXFIFO_CNT",     SUNXI_AHUB_APBIF_TXFIFO_CNT(0),     0},
    {"SUNXI_AHUB_APBIF_RX_CTL",         SUNXI_AHUB_APBIF_RX_CTL(0),         0},
    {"SUNXI_AHUB_APBIF_RX_IRQ_CTL",     SUNXI_AHUB_APBIF_RX_IRQ_CTL(0),     0},
    {"SUNXI_AHUB_APBIF_RX_IRQ_STA",     SUNXI_AHUB_APBIF_RX_IRQ_STA(0),     0},
    {"SUNXI_AHUB_APBIF_RXFIFO_CTL",     SUNXI_AHUB_APBIF_RXFIFO_CTL(0),     0},
    {"SUNXI_AHUB_APBIF_RXFIFO_STA",     SUNXI_AHUB_APBIF_RXFIFO_STA(0),     0},
    {"SUNXI_AHUB_APBIF_RXFIFO_CONT",    SUNXI_AHUB_APBIF_RXFIFO_CONT(0),    0},
    /* {"SUNXI_AHUB_APBIF_RXFIFO",      SUNXI_AHUB_APBIF_RXFIFO(0),         0}, */
    {"SUNXI_AHUB_APBIF_RXFIFO_CNT",     SUNXI_AHUB_APBIF_RXFIFO_CNT(0),     0},
    {"SUNXI_AHUB_I2S_CTL",              SUNXI_AHUB_I2S_CTL(0),              0},
    {"SUNXI_AHUB_I2S_FMT0",             SUNXI_AHUB_I2S_FMT0(0),             0},
    {"SUNXI_AHUB_I2S_FMT1",             SUNXI_AHUB_I2S_FMT1(0),             0},
    {"SUNXI_AHUB_I2S_CLKD",             SUNXI_AHUB_I2S_CLKD(0),             0},
    {"SUNXI_AHUB_I2S_RXCONT",           SUNXI_AHUB_I2S_RXCONT(0),           0},
    {"SUNXI_AHUB_I2S_CHCFG",            SUNXI_AHUB_I2S_CHCFG(0),            0},
    {"SUNXI_AHUB_I2S_IRQ_CTL",          SUNXI_AHUB_I2S_IRQ_CTL(0),          0},
    {"SUNXI_AHUB_I2S_IRQ_STA",          SUNXI_AHUB_I2S_IRQ_STA(0),          0},
    {"SUNXI_AHUB_I2S_OUT_SLOT",         SUNXI_AHUB_I2S_OUT_SLOT(0, 0),      0},
    {"SUNXI_AHUB_I2S_OUT_CHMAP0",       SUNXI_AHUB_I2S_OUT_CHMAP0(0, 0),    0},
    {"SUNXI_AHUB_I2S_OUT_CHMAP1",       SUNXI_AHUB_I2S_OUT_CHMAP1(0, 0),    0},
    {"SUNXI_AHUB_I2S_IN_SLOT",          SUNXI_AHUB_I2S_IN_SLOT(0),          0},
    {"SUNXI_AHUB_I2S_IN_CHMAP0",        SUNXI_AHUB_I2S_IN_CHMAP0(0),        0},
    {"SUNXI_AHUB_I2S_IN_CHMAP1",        SUNXI_AHUB_I2S_IN_CHMAP1(0),        0},
    {"SUNXI_AHUB_I2S_IN_CHMAP2",        SUNXI_AHUB_I2S_IN_CHMAP2(0),        0},
    {"SUNXI_AHUB_I2S_IN_CHMAP3",        SUNXI_AHUB_I2S_IN_CHMAP3(0),        0},
    REG_LABEL_END,
};

/*******************************************************************************
 *  for adm api
 ******************************************************************************/
void T507AhubImplDeviceInit(void)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = ahub_info->mem_info.regmap;
    uint32_t apb_num = ahub_info->dts_info.apb_num;
    uint32_t tdm_num = ahub_info->dts_info.tdm_num;
    uint32_t tx_pin = ahub_info->dts_info.tx_pin;
    uint32_t rx_pin = ahub_info->dts_info.rx_pin;

    uint32_t reg_val = 0;
    uint32_t rx_pin_map = 0;
    uint32_t tdm_to_apb = 0;
    uint32_t apb_to_tdm = 0;

    AUDIO_DRIVER_LOG_DEBUG("");

    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << I2S_CTL_GEN, 0x1 << I2S_CTL_GEN);
    regmap_update_bits(regmap, SUNXI_AHUB_RST, 0x1 << (I2S0_RST - tdm_num), 0x1 << (I2S0_RST - tdm_num));
    regmap_update_bits(regmap, SUNXI_AHUB_GAT, 0x1 << (I2S0_GAT - tdm_num), 0x1 << (I2S0_GAT - tdm_num));

    regmap_update_bits(regmap, SUNXI_AHUB_RST, 0x1 << (APBIF_TXDIF0_RST - apb_num), 0x1 << (APBIF_TXDIF0_RST - apb_num));
    regmap_update_bits(regmap, SUNXI_AHUB_GAT, 0x1 << (APBIF_TXDIF0_GAT - apb_num), 0x1 << (APBIF_TXDIF0_GAT - apb_num));
    regmap_update_bits(regmap, SUNXI_AHUB_RST, 0x1 << (APBIF_RXDIF0_RST - apb_num), 0x1 << (APBIF_RXDIF0_RST - apb_num));
    regmap_update_bits(regmap, SUNXI_AHUB_GAT, 0x1 << (APBIF_RXDIF0_GAT - apb_num), 0x1 << (APBIF_RXDIF0_GAT - apb_num));

    /* tdm tx channels map */
    regmap_write(regmap, SUNXI_AHUB_I2S_OUT_CHMAP0(tdm_num, tx_pin), 0x76543210);
    regmap_write(regmap, SUNXI_AHUB_I2S_OUT_CHMAP1(tdm_num, tx_pin), 0xFEDCBA98);

    /* tdm rx channels map */
    rx_pin_map = (rx_pin << 4) | (rx_pin << 12) | (rx_pin << 20) | (rx_pin << 28);
    reg_val = 0x03020100 | rx_pin_map;
    regmap_write(regmap, SUNXI_AHUB_I2S_IN_CHMAP0(tdm_num), reg_val);
    reg_val = 0x07060504 | rx_pin_map;
    regmap_write(regmap, SUNXI_AHUB_I2S_IN_CHMAP1(tdm_num), reg_val);
    reg_val = 0x0B0A0908 | rx_pin_map;
    regmap_write(regmap, SUNXI_AHUB_I2S_IN_CHMAP2(tdm_num), reg_val);
    reg_val = 0x0F0E0D0C | rx_pin_map;
    regmap_write(regmap, SUNXI_AHUB_I2S_IN_CHMAP3(tdm_num), reg_val);

    /* tdm tx & rx data fmt
     * 1. MSB first
     * 2. transfer 0 after each sample in each slot
     * 3. linear PCM
     */
    regmap_write(regmap, SUNXI_AHUB_I2S_FMT1(tdm_num), 0x30);

    /* apbif tx & rx data fmt
     * 1. MSB first
     * 2. trigger level tx -> 0x20, rx -> 0x40
     */
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x1 << APBIF_TX_TXIM, 0x0 << APBIF_TX_TXIM);
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x3f << APBIF_TX_LEVEL, 0x20 << APBIF_TX_LEVEL);
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x3 << APBIF_RX_RXOM, 0x0 << APBIF_RX_RXOM);
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x7f << APBIF_RX_LEVEL, 0x40 << APBIF_RX_LEVEL);

    /* apbif <-> tdm */
    switch (tdm_num) {
        case 0:
            tdm_to_apb = APBIF_RX_I2S0_TXDIF;
            break;
        case 1:
            tdm_to_apb = APBIF_RX_I2S1_TXDIF;
            break;
        case 2:
            tdm_to_apb = APBIF_RX_I2S2_TXDIF;
            break;
        case 3:
            tdm_to_apb = APBIF_RX_I2S3_TXDIF;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unspport tdm num");
            return;
    }
    regmap_write(regmap, SUNXI_AHUB_APBIF_RXFIFO_CONT(apb_num), 0x1 << tdm_to_apb);

    switch (apb_num) {
        case 0:
            apb_to_tdm = I2S_RX_APBIF_TXDIF0;
            break;
        case 1:
            apb_to_tdm = I2S_RX_APBIF_TXDIF1;
            break;
        case 2:
            apb_to_tdm = I2S_RX_APBIF_TXDIF2;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unspport apb num");
            return;
    }
    regmap_write(regmap, SUNXI_AHUB_I2S_RXCONT(tdm_num), 0x1 << apb_to_tdm);

    AUDIO_DRIVER_LOG_DEBUG("success!");
}

void T507AhubImplRegmapWrite(uint32_t reg, uint32_t val)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = ahub_info->mem_info.regmap;

    regmap_write(regmap, reg, val);
}

void T507AhubImplRegmapRead(uint32_t reg, uint32_t *val)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = ahub_info->mem_info.regmap;

    regmap_read(regmap, reg, val);
}

int32_t T507AhubImplStartup(enum AudioStreamType streamType)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (streamType == AUDIO_RENDER_STREAM) {
    /* ahub unuse render */
    return HDF_SUCCESS;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static int sunxi_ahub_dai_set_sysclk(uint32_t freq)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t tdm_num;
    uint32_t mclk_ratio, mclk_ratio_map;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null.");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    tdm_num = ahub_info->dts_info.tdm_num;

    if (freq == 0) {
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_CLKD(tdm_num), 0x1 << I2S_CLKD_MCLK, 0x0 << I2S_CLKD_MCLK);
        AUDIO_DRIVER_LOG_DEBUG("mclk freq: 0");
        return 0;
    }
    if (ahub_info->pllclk_freq == 0) {
        AUDIO_DRIVER_LOG_ERR("pllclk freq is invalid");
        return -ENOMEM;
    }
    mclk_ratio = ahub_info->pllclk_freq / freq;

    switch (mclk_ratio) {
        case 1:
            mclk_ratio_map = 1;
            break;
        case 2:
            mclk_ratio_map = 2;
            break;
        case 4:
            mclk_ratio_map = 3;
            break;
        case 6:
            mclk_ratio_map = 4;
            break;
        case 8:
            mclk_ratio_map = 5;
            break;
        case 12:
            mclk_ratio_map = 6;
            break;
        case 16:
            mclk_ratio_map = 7;
            break;
        case 24:
            mclk_ratio_map = 8;
            break;
        case 32:
            mclk_ratio_map = 9;
            break;
        case 48:
            mclk_ratio_map = 10;
            break;
        case 64:
            mclk_ratio_map = 11;
            break;
        case 96:
            mclk_ratio_map = 12;
            break;
        case 128:
            mclk_ratio_map = 13;
            break;
        case 176:
            mclk_ratio_map = 14;
            break;
        case 192:
            mclk_ratio_map = 15;
            break;
        default:
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_CLKD(tdm_num), 0x1 << I2S_CLKD_MCLK, 0x0 << I2S_CLKD_MCLK);
            AUDIO_DRIVER_LOG_ERR("mclk freq div unsupport");
            return -EINVAL;
    }

    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CLKD(tdm_num), 0xf << I2S_CLKD_MCLKDIV, mclk_ratio_map << I2S_CLKD_MCLKDIV);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CLKD(tdm_num), 0x1 << I2S_CLKD_MCLK, 0x1 << I2S_CLKD_MCLK);

    return 0;
}

static int sunxi_ahub_dai_set_bclk_ratio(uint32_t ratio)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t tdm_num;
    uint32_t bclk_ratio;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null.");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    tdm_num = ahub_info->dts_info.tdm_num;

    /* ratio -> cpudai pllclk / pcm rate */
    switch (ratio) {
        case 1:
            bclk_ratio = 1;
            break;
        case 2:
            bclk_ratio = 2;
            break;
        case 4:
            bclk_ratio = 3;
            break;
        case 6:
            bclk_ratio = 4;
            break;
        case 8:
            bclk_ratio = 5;
            break;
        case 12:
            bclk_ratio = 6;
            break;
        case 16:
            bclk_ratio = 7;
            break;
        case 24:
            bclk_ratio = 8;
            break;
        case 32:
            bclk_ratio = 9;
            break;
        case 48:
            bclk_ratio = 10;
            break;
        case 64:
            bclk_ratio = 11;
            break;
        case 96:
            bclk_ratio = 12;
            break;
        case 128:
            bclk_ratio = 13;
            break;
        case 176:
            bclk_ratio = 14;
            break;
        case 192:
            bclk_ratio = 15;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("bclk freq div unsupport");
            return -EINVAL;
    }

    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CLKD(tdm_num), 0xf << I2S_CLKD_BCLKDIV, bclk_ratio << I2S_CLKD_BCLKDIV);

    return 0;
}

#define SND_SOC_DAIFMT_FORMAT_MASK  0x000f
#define SND_SOC_DAIFMT_CLOCK_MASK   0x00f0
#define SND_SOC_DAIFMT_INV_MASK     0x0f00
#define SND_SOC_DAIFMT_MASTER_MASK  0xf000

#define SND_SOC_DAIFMT_I2S          1
#define SND_SOC_DAIFMT_RIGHT_J      2
#define SND_SOC_DAIFMT_LEFT_J       3
#define SND_SOC_DAIFMT_DSP_A        4
#define SND_SOC_DAIFMT_DSP_B        5
#define SND_SOC_DAIFMT_AC97         6
#define SND_SOC_DAIFMT_PDM          7

#define SND_SOC_DAIFMT_CONT         (1 << 4) /* continuous clock */
#define SND_SOC_DAIFMT_GATED        (0 << 4) /* clock is gated */

#define SND_SOC_DAIFMT_NB_NF        (0 << 8) /* normal bit clock + frame */
#define SND_SOC_DAIFMT_NB_IF        (2 << 8) /* normal BCLK + inv FRM */
#define SND_SOC_DAIFMT_IB_NF        (3 << 8) /* invert BCLK + nor FRM */
#define SND_SOC_DAIFMT_IB_IF        (4 << 8) /* invert BCLK + FRM */

#define SND_SOC_DAIFMT_CBM_CFM      (1 << 12) /* codec clk & FRM master */
#define SND_SOC_DAIFMT_CBS_CFM      (2 << 12) /* codec clk slave & FRM master */
#define SND_SOC_DAIFMT_CBM_CFS      (3 << 12) /* codec clk master & frame slave */
#define SND_SOC_DAIFMT_CBS_CFS      (4 << 12) /* codec clk & FRM slave */

static int sunxi_ahub_dai_set_fmt(uint32_t fmt)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t tdm_num, tx_pin, rx_pin;
    uint32_t mode, offset;
    uint32_t lrck_polarity, brck_polarity;

    AUDIO_DRIVER_LOG_DEBUG("");

    ahub_info->fmt = fmt;

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null.");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    tdm_num = ahub_info->dts_info.tdm_num;
    tx_pin = ahub_info->dts_info.tx_pin;
    rx_pin = ahub_info->dts_info.rx_pin;

    /* set TDM format */
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_I2S:
            mode = 1;
            offset = 1;
            break;
        case SND_SOC_DAIFMT_RIGHT_J:
            mode = 2;
            offset = 0;
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            mode = 1;
            offset = 0;
            break;
        case SND_SOC_DAIFMT_DSP_A:
            mode = 0;
            offset = 1;
            /* L data MSB after FRM LRC (short frame) */
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x1 << I2S_FMT0_LRCK_WIDTH, 0x0 << I2S_FMT0_LRCK_WIDTH);
            break;
        case SND_SOC_DAIFMT_DSP_B:
            mode = 0;
            offset = 0;
            /* L data MSB during FRM LRC (long frame) */
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x1 << I2S_FMT0_LRCK_WIDTH, 0x1 << I2S_FMT0_LRCK_WIDTH);
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("format setting failed");
            return -EINVAL;
    }
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x3 << I2S_CTL_MODE, mode << I2S_CTL_MODE);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, 0), 0x3 << I2S_OUT_OFFSET, offset << I2S_OUT_OFFSET);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, 1), 0x3 << I2S_OUT_OFFSET, offset << I2S_OUT_OFFSET);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, 2), 0x3 << I2S_OUT_OFFSET, offset << I2S_OUT_OFFSET);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, 3), 0x3 << I2S_OUT_OFFSET, offset << I2S_OUT_OFFSET);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_IN_SLOT(tdm_num), 0x3 << I2S_IN_OFFSET, offset << I2S_IN_OFFSET);

    /* set lrck & bclk polarity */
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
            lrck_polarity = 0;
            brck_polarity = 0;
            break;
        case SND_SOC_DAIFMT_NB_IF:
            lrck_polarity = 1;
            brck_polarity = 0;
            break;
        case SND_SOC_DAIFMT_IB_NF:
            lrck_polarity = 0;
            brck_polarity = 1;
            break;
        case SND_SOC_DAIFMT_IB_IF:
            lrck_polarity = 1;
            brck_polarity = 1;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("invert clk setting failed");
            return -EINVAL;
    }
    if (((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) || ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_B)) {
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x1 << I2S_FMT0_LRCK_POLARITY, (lrck_polarity^1) << I2S_FMT0_LRCK_POLARITY);
    } else {
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x1 << I2S_FMT0_LRCK_POLARITY, lrck_polarity << I2S_FMT0_LRCK_POLARITY);
    }

    regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x1 << I2S_FMT0_BCLK_POLARITY, brck_polarity << I2S_FMT0_BCLK_POLARITY);

    /* set master/slave */
    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBM_CFM:
            /* lrck & bclk dir input */
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << I2S_CTL_CLK_OUT, 0x0 << I2S_CTL_CLK_OUT);
            break;
        case SND_SOC_DAIFMT_CBS_CFS:
            /* lrck & bclk dir output */
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << I2S_CTL_CLK_OUT, 0x1 << I2S_CTL_CLK_OUT);
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unknown master/slave format");
            return -EINVAL;
    }

    return 0;
}

static int sunxi_ahub_dai_set_tdm_slot(int slots, int slot_width)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t tdm_num, tx_pin, rx_pin;
    uint32_t slot_width_map, lrck_width_map;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    tdm_num = ahub_info->dts_info.tdm_num;
    tx_pin = ahub_info->dts_info.tx_pin;
    rx_pin = ahub_info->dts_info.rx_pin;

    switch (slot_width) {
        case 8:
            slot_width_map = 1;
            break;
        case 12:
            slot_width_map = 2;
            break;
        case 16:
            slot_width_map = 3;
            break;
        case 20:
            slot_width_map = 4;
            break;
        case 24:
            slot_width_map = 5;
            break;
        case 28:
            slot_width_map = 6;
            break;
        case 32:
            slot_width_map = 7;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unknown slot width");
            return -EINVAL;
    }
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x7 << I2S_FMT0_SW, slot_width_map << I2S_FMT0_SW);

    /* bclk num of per channel
     * I2S/RIGHT_J/LEFT_J    -> lrck long total is lrck_width_map * 2
     * DSP_A/DAP_B        -> lrck long total is lrck_width_map * 1
     */
    switch (ahub_info->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
    case SND_SOC_DAIFMT_I2S:
    case SND_SOC_DAIFMT_RIGHT_J:
    case SND_SOC_DAIFMT_LEFT_J:
        slots /= 2;
        break;
    case SND_SOC_DAIFMT_DSP_A:
    case SND_SOC_DAIFMT_DSP_B:
        break;
    default:
        AUDIO_DRIVER_LOG_ERR("unsupoort format");
        return -EINVAL;
    }
    lrck_width_map = slots * slot_width - 1;
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x3ff << I2S_FMT0_LRCK_PERIOD, lrck_width_map << I2S_FMT0_LRCK_PERIOD);

    return 0;
}

static int sunxi_ahub_dai_hw_params(enum AudioStreamType streamType, enum AudioFormat format, uint32_t channels, uint32_t rate)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t apb_num, tdm_num, tx_pin, rx_pin;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null.");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    apb_num = ahub_info->dts_info.apb_num;
    tdm_num = ahub_info->dts_info.tdm_num;
    tx_pin = ahub_info->dts_info.tx_pin;
    rx_pin = ahub_info->dts_info.rx_pin;

    /* set bits */
    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            /* apbifn bits */
            if (streamType == AUDIO_RENDER_STREAM) {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TX_CTL(apb_num), 0x7 << APBIF_TX_WS, 0x3 << APBIF_TX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x1 << APBIF_TX_TXIM, 0x1 << APBIF_TX_TXIM);
            } else {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0x7 << APBIF_RX_WS, 0x3 << APBIF_RX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x3 << APBIF_RX_RXOM, 0x1 << APBIF_RX_RXOM);
            }
            /* tdmn bits */
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x7 << I2S_FMT0_SR, 0x3 << I2S_FMT0_SR);
            break;
        case AUDIO_FORMAT_PCM_24_BIT:
            if (streamType == AUDIO_RENDER_STREAM) {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TX_CTL(apb_num), 0x7 << APBIF_TX_WS, 0x5 << APBIF_TX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x1 << APBIF_TX_TXIM, 0x1 << APBIF_TX_TXIM);
            } else {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0x7 << APBIF_RX_WS, 0x5 << APBIF_RX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x3 << APBIF_RX_RXOM, 0x1 << APBIF_RX_RXOM);
            }
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x7 << I2S_FMT0_SR, 0x5 << I2S_FMT0_SR);
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            if (streamType == AUDIO_RENDER_STREAM) {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TX_CTL(apb_num), 0x7 << APBIF_TX_WS, 0x7 << APBIF_TX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x1 << APBIF_TX_TXIM, 0x1 << APBIF_TX_TXIM);
            } else {
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0x7 << APBIF_RX_WS, 0x7 << APBIF_RX_WS);
                regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x3 << APBIF_RX_RXOM, 0x1 << APBIF_RX_RXOM);
            }
            regmap_update_bits(regmap, SUNXI_AHUB_I2S_FMT0(tdm_num), 0x7 << I2S_FMT0_SR, 0x7 << I2S_FMT0_SR);
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unrecognized format bits");
            return -EINVAL;
    }

    /* set channels */
    if (streamType == AUDIO_RENDER_STREAM) {
        uint32_t channels_en[16] = {
            0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
            0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
        };

        /* apbifn channels */
        regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TX_CTL(apb_num), 0xf << APBIF_TX_CHAN_NUM, (channels - 1) << APBIF_TX_CHAN_NUM);
        /* tdmn channels */
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_CHCFG(tdm_num), 0xf << I2S_CHCFG_TX_CHANNUM, (channels - 1) << I2S_CHCFG_TX_CHANNUM);

        regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, tx_pin), 0xf << I2S_OUT_SLOT_NUM, (channels - 1) << I2S_OUT_SLOT_NUM);
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_OUT_SLOT(tdm_num, tx_pin), 0xffff << I2S_OUT_SLOT_EN, channels_en[channels - 1] << I2S_OUT_SLOT_EN);
    } else {
        /* apbifn channels */
        regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0xf << APBIF_RX_CHAN_NUM, (channels - 1) << APBIF_RX_CHAN_NUM);
        /* tdmn channels */
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_CHCFG(tdm_num), 0xf << I2S_CHCFG_RX_CHANNUM, (channels - 1) << I2S_CHCFG_RX_CHANNUM);
        regmap_update_bits(regmap, SUNXI_AHUB_I2S_IN_SLOT(tdm_num), 0xf << I2S_IN_SLOT_NUM, (channels - 1) << I2S_IN_SLOT_NUM);
    }

    /* set rate (set at lrck & bclk, nothing set at this function) */

    return 0;
}

static int sunxi_ahub_dai_prepare(enum AudioStreamType streamType)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = NULL;
    uint32_t apb_num;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("ahub_info is null.");
        return -ENOMEM;
    }
    regmap = ahub_info->mem_info.regmap;
    apb_num = ahub_info->dts_info.apb_num;

    if (streamType == AUDIO_RENDER_STREAM) {
        /* clear txfifo */
        regmap_update_bits(regmap, SUNXI_AHUB_APBIF_TXFIFO_CTL(apb_num), 0x1 << APBIF_TX_FTX, 0x1 << APBIF_TX_FTX);
        /* clear tx o/u irq */
        regmap_write(regmap, SUNXI_AHUB_APBIF_TX_IRQ_STA(apb_num), (0x1 << APBIF_TX_OV_PEND) | (0x1 << APBIF_TX_EM_PEND));
        /* clear tx fifo cnt */
        regmap_write(regmap, SUNXI_AHUB_APBIF_TXFIFO_CNT(apb_num), 0);
    } else {
        /* clear rxfifo */
        regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RXFIFO_CTL(apb_num), 0x1 << APBIF_RX_FRX, 0x1 << APBIF_RX_FRX);
        /* clear rx o/u irq */
        regmap_write(regmap, SUNXI_AHUB_APBIF_RX_IRQ_STA(apb_num), (0x1 << APBIF_RX_UV_PEND) | (0x1 << APBIF_RX_AV_PEND));
        /* clear rx fifo cnt */
        regmap_write(regmap, SUNXI_AHUB_APBIF_RXFIFO_CNT(apb_num), 0);
    }

    return 0;
}

int32_t T507AhubImplHwParams(enum AudioStreamType streamType, enum AudioFormat format, uint32_t channels, uint32_t rate)
{
    int ret;
    uint32_t freq_point;
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct sunxi_ahub_clk_info *clk_info = &ahub_info->clk_info;
    uint32_t cpu_bclk_ratio;

    uint32_t cpu_pll_fs = 4;
    int slots = 2;
    int slot_width = 32;
    uint32_t dai_fmt = 0;
    dai_fmt |= SND_SOC_DAIFMT_I2S;
    dai_fmt |= SND_SOC_DAIFMT_NB_NF;
    dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;

    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            AUDIO_DRIVER_LOG_DEBUG(" format 16");
            break;
        case AUDIO_FORMAT_PCM_24_BIT:
            AUDIO_DRIVER_LOG_DEBUG(" format 24");
            break;
    case AUDIO_FORMAT_PCM_32_BIT:
            AUDIO_DRIVER_LOG_DEBUG(" format 32");
            break;
    default:
        AUDIO_DRIVER_LOG_ERR("format: %d is not define.", format);
        return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG(" rate %u", rate);
    AUDIO_DRIVER_LOG_DEBUG(" channels %u", channels);

    if (streamType == AUDIO_RENDER_STREAM) {
        /* ahub unuse render */
        return HDF_SUCCESS;
    }

    /* set pll */
    switch (rate) {
        case 8000:
        case 12000:
        case 16000:
        case 24000:
        case 32000:
        case 48000:
        case 64000:
        case 96000:
        case 192000:
            freq_point = 24576000;
            break;
        case 11025:
        case 22050:
        case 44100:
        case 88200:
        case 176400:
            freq_point = 22579200;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("rate: %d is not define.", rate);
            return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG(" freq_point %u", freq_point);

    ahub_info->pllclk_freq = freq_point * cpu_pll_fs;
    ahub_info->moduleclk_freq = ahub_info->pllclk_freq;

    if (clk_set_rate(clk_info->clk_pllx4, ahub_info->pllclk_freq)) {
        AUDIO_DRIVER_LOG_ERR("clk pllaudio set rate failed");
        return -EINVAL;
    }
    if (clk_set_rate(clk_info->clk_module, ahub_info->moduleclk_freq)) {
        AUDIO_DRIVER_LOG_ERR("clk audio set rate failed");
        return -EINVAL;
    }

    /* set mclk */
    ahub_info->mclk_freq = freq_point / 2;    /* ac107 need mclk freq 12.288M or 11.2896MHz */

    ret = sunxi_ahub_dai_set_sysclk(ahub_info->mclk_freq);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_set_sysclk failed");
        return HDF_FAILURE;
    }

    /* set bclk */
    cpu_bclk_ratio = ahub_info->pllclk_freq / (slots * slot_width * rate);
    ret = sunxi_ahub_dai_set_bclk_ratio(cpu_bclk_ratio);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_set_bclk_ratio failed");
        return HDF_FAILURE;
    }

    /* set fmt */
    ret = sunxi_ahub_dai_set_fmt(dai_fmt);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_set_fmt failed");
        return HDF_FAILURE;
    }

    /* set tdm slot */
    ret = sunxi_ahub_dai_set_tdm_slot(slots, slot_width);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_set_tdm_slot failed");
        return HDF_FAILURE;
    }

    /* set pcm info */
    ret = sunxi_ahub_dai_hw_params(streamType, format, channels, rate);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_hw_params failed");
        return HDF_FAILURE;
    }

    /* clear fifo */
    ret = sunxi_ahub_dai_prepare(streamType);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sunxi_ahub_dai_prepare failed");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static void sunxi_ahub_dai_rx_route(struct sunxi_ahub_info *ahub_info, bool enable)
{
    struct regmap *regmap = NULL;
    uint32_t tdm_num, rx_pin;
    uint32_t apb_num;

    AUDIO_DRIVER_LOG_DEBUG("%s", enable ? "on" : "off");

    regmap = ahub_info->mem_info.regmap;
    tdm_num = ahub_info->dts_info.tdm_num;
    rx_pin = ahub_info->dts_info.rx_pin;
    apb_num = ahub_info->dts_info.apb_num;

    if (enable)
        goto rx_route_enable;
    else
        goto rx_route_disable;

rx_route_enable:
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << (I2S_CTL_SDI0_EN + rx_pin), 0x1 << (I2S_CTL_SDI0_EN + rx_pin));
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << I2S_CTL_RXEN, 0x1 << I2S_CTL_RXEN);
    /* start apbif rx */
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0x1 << APBIF_RX_START, 0x1 << APBIF_RX_START);
    /* enable rx drq */
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_IRQ_CTL(apb_num), 0x1 << APBIF_RX_DRQ, 0x1 << APBIF_RX_DRQ);
    return;

rx_route_disable:
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << I2S_CTL_RXEN, 0x0 << I2S_CTL_RXEN);
    regmap_update_bits(regmap, SUNXI_AHUB_I2S_CTL(tdm_num), 0x1 << (I2S_CTL_SDI0_EN + rx_pin), 0x0 << (I2S_CTL_SDI0_EN + rx_pin));
    /* stop apbif rx */
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_CTL(apb_num), 0x1 << APBIF_RX_START, 0x0 << APBIF_RX_START);
    /* disable rx drq */
    regmap_update_bits(regmap, SUNXI_AHUB_APBIF_RX_IRQ_CTL(apb_num), 0x1 << APBIF_RX_DRQ, 0x0 << APBIF_RX_DRQ);
    return;
}

int32_t T507AhubImplTrigger(enum AudioStreamType streamType, bool enable)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);

    AUDIO_DRIVER_LOG_DEBUG("");

    if (streamType == AUDIO_RENDER_STREAM) {
    /* ahub unuse render */
    return HDF_SUCCESS;
    }

    sunxi_ahub_dai_rx_route(ahub_info, enable);

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

/*******************************************************************************
 * for linux probe
 ******************************************************************************/
static int snd_sunxi_ahub_mem_init(struct platform_device *pdev, struct sunxi_ahub_mem_info *mem_info)
{
    int ret = 0;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    ret = of_address_to_resource(np, 0, &mem_info->res);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("parse device node resource failed");
        ret = -EINVAL;
        goto err_of_addr_to_resource;
    }

    mem_info->memregion = devm_request_mem_region(&pdev->dev, mem_info->res.start, resource_size(&mem_info->res), DRV_NAME);
    if (IS_ERR_OR_NULL(mem_info->memregion)) {
        AUDIO_DRIVER_LOG_ERR("memory region already claimed");
        ret = -EBUSY;
        goto err_devm_request_region;
    }

    mem_info->membase = devm_ioremap(&pdev->dev, mem_info->memregion->start, resource_size(mem_info->memregion));
    if (IS_ERR_OR_NULL(mem_info->membase)) {
        AUDIO_DRIVER_LOG_ERR("ioremap failed");
        ret = -EBUSY;
        goto err_devm_ioremap;
    }

    mem_info->regmap = devm_regmap_init_mmio(&pdev->dev, mem_info->membase, &g_regmap_config);
    if (IS_ERR_OR_NULL(mem_info->regmap)) {
        AUDIO_DRIVER_LOG_ERR("regmap init failed");
        ret = -EINVAL;
        goto err_devm_regmap_init;
    }

    return 0;

err_devm_regmap_init:
    devm_iounmap(&pdev->dev, mem_info->membase);
err_devm_ioremap:
    devm_release_mem_region(&pdev->dev, mem_info->memregion->start, resource_size(mem_info->memregion));
err_devm_request_region:
err_of_addr_to_resource:
    return ret;
};

static void snd_sunxi_ahub_mem_exit(struct platform_device *pdev, struct sunxi_ahub_mem_info *mem_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    devm_iounmap(&pdev->dev, mem_info->membase);
    devm_release_mem_region(&pdev->dev, mem_info->memregion->start, resource_size(mem_info->memregion));
}

static int snd_sunxi_ahub_clk_init(struct platform_device *pdev, struct sunxi_ahub_clk_info *clk_info)
{
    int ret = 0;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    /* get rst clk */
    clk_info->clk_rst = devm_reset_control_get(&pdev->dev, NULL);
    if (IS_ERR_OR_NULL(clk_info->clk_rst)) {
        AUDIO_DRIVER_LOG_ERR("clk rst get failed\n");
        ret =  PTR_ERR(clk_info->clk_rst);
        goto err_get_clk_rst;
    }

    /* get bus clk */
    clk_info->clk_bus = of_clk_get_by_name(np, "clk_bus_audio_hub");
    if (IS_ERR_OR_NULL(clk_info->clk_bus)) {
        AUDIO_DRIVER_LOG_ERR("clk bus get failed\n");
        ret = PTR_ERR(clk_info->clk_bus);
        goto err_get_clk_bus;
    }

    /* get clk of ahub */
    clk_info->clk_module = of_clk_get_by_name(np, "clk_audio_hub");
    if (IS_ERR_OR_NULL(clk_info->clk_module)) {
        AUDIO_DRIVER_LOG_ERR("clk module get failed");
        ret = -EBUSY;
        goto err_module_clk;
    }
    clk_info->clk_pll = of_clk_get_by_name(np, "clk_pll_audio");
    if (IS_ERR_OR_NULL(clk_info->clk_pll)) {
        AUDIO_DRIVER_LOG_ERR("clk pll get failed");
        ret = -EBUSY;
        goto err_pll_clk;
    }
    clk_info->clk_pllx4 = of_clk_get_by_name(np, "clk_pll_audio_4x");
    if (IS_ERR_OR_NULL(clk_info->clk_pllx4)) {
        AUDIO_DRIVER_LOG_ERR("clk pllx4 get failed");
        ret = -EBUSY;
        goto err_pllx4_clk;
    }

    /* set ahub clk parent */
    if (clk_set_parent(clk_info->clk_module, clk_info->clk_pllx4)) {
        AUDIO_DRIVER_LOG_ERR("set parent of clk_module to pllx4 failed");
        ret = -EINVAL;
        goto err_set_parent_clk;
    }

    /* enable clk of ahub */
    if (reset_control_deassert(clk_info->clk_rst)) {
        AUDIO_DRIVER_LOG_ERR("clk rst deassert failed\n");
        ret = -EINVAL;
        goto err_deassert_rst;
    }

    if (clk_prepare_enable(clk_info->clk_bus)) {
        AUDIO_DRIVER_LOG_ERR("clk bus enable failed\n");
        goto err_enable_clk_bus;
    }

    if (clk_prepare_enable(clk_info->clk_pll)) {
        AUDIO_DRIVER_LOG_ERR("clk_pll enable failed");
        ret = -EBUSY;
        goto err_pll_clk_enable;
    }
    if (clk_prepare_enable(clk_info->clk_pllx4)) {
        AUDIO_DRIVER_LOG_ERR("clk_pllx4 enable failed");
        ret = -EBUSY;
        goto err_pllx4_clk_enable;
    }
    if (clk_prepare_enable(clk_info->clk_module)) {
        AUDIO_DRIVER_LOG_ERR("clk_module enable failed");
        ret = -EBUSY;
        goto err_module_clk_enable;
    }

    return 0;

err_module_clk_enable:
    clk_disable_unprepare(clk_info->clk_pllx4);
err_pllx4_clk_enable:
    clk_disable_unprepare(clk_info->clk_pll);
err_pll_clk_enable:
    clk_disable_unprepare(clk_info->clk_bus);
err_enable_clk_bus:
    reset_control_assert(clk_info->clk_rst);
err_deassert_rst:
err_set_parent_clk:
    clk_put(clk_info->clk_pllx4);
err_pllx4_clk:
    clk_put(clk_info->clk_pll);
err_pll_clk:
    clk_put(clk_info->clk_module);
err_module_clk:
    clk_put(clk_info->clk_bus);
err_get_clk_bus:
err_get_clk_rst:
    return ret;
}

static void snd_sunxi_clk_exit(struct sunxi_ahub_clk_info *clk_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    clk_disable_unprepare(clk_info->clk_module);
    clk_put(clk_info->clk_module);
    clk_disable_unprepare(clk_info->clk_pll);
    clk_put(clk_info->clk_pll);
    clk_disable_unprepare(clk_info->clk_pllx4);
    clk_put(clk_info->clk_pllx4);
    clk_disable_unprepare(clk_info->clk_bus);
    clk_put(clk_info->clk_bus);
    reset_control_assert(clk_info->clk_rst);
}

static int snd_sunxi_ahub_dts_params_init(struct platform_device *pdev, struct sunxi_ahub_dts_info *dts_info)
{
    int ret = 0;
    uint32_t temp_val = 0;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    /* get dma params */
    /* unused */

    /* get tdm fmt of apb_num & tdm_num & tx/rx_pin */
    ret = of_property_read_u32(np, "apb-num", &temp_val);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("apb-num config missing");
        dts_info->apb_num = 0;
    } else {
        if (temp_val > 2) {    /* APBIFn (n = 0~2) */
            dts_info->apb_num = 0;
            AUDIO_DRIVER_LOG_ERR("apb-num config invalid");
        } else {
            dts_info->apb_num = temp_val;
        }
    }
    ret = of_property_read_u32(np, "tdm-num", &temp_val);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("tdm-num config missing");
        dts_info->tdm_num = 0;
    } else {
        if (temp_val > 3) {    /* I2Sn (n = 0~3) */
            dts_info->tdm_num = 0;
            AUDIO_DRIVER_LOG_ERR("tdm-num config invalid");
        } else {
            dts_info->tdm_num = temp_val;
        }
    }
    ret = of_property_read_u32(np, "tx-pin", &temp_val);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("tx-pin config missing");
        dts_info->tx_pin = 0;
    } else {
        if (temp_val > 3) {    /* I2S_DOUTn (n = 0~3) */
            dts_info->tx_pin = 0;
            AUDIO_DRIVER_LOG_ERR("tx-pin config invalid");
        } else {
            dts_info->tx_pin = temp_val;
        }
    }
    ret = of_property_read_u32(np, "rx-pin", &temp_val);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("rx-pin config missing");
        dts_info->rx_pin = 0;
    } else {
        if (temp_val > 3) {    /* I2S_DINTn (n = 0~3) */
            dts_info->rx_pin = 0;
            AUDIO_DRIVER_LOG_ERR("rx-pin config invalid");
        } else {
            dts_info->rx_pin = temp_val;
        }
    }

    AUDIO_DRIVER_LOG_DEBUG("apb-num      : %u", dts_info->apb_num);
    AUDIO_DRIVER_LOG_DEBUG("tdm-num      : %u", dts_info->tdm_num);
    AUDIO_DRIVER_LOG_DEBUG("tx-pin       : %u", dts_info->tx_pin);
    AUDIO_DRIVER_LOG_DEBUG("rx-pin       : %u", dts_info->rx_pin);

    return 0;
};

static int snd_sunxi_ahub_pin_init(struct platform_device *pdev, struct sunxi_ahub_pinctl_info *pin_info)
{
    int ret = 0;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (of_property_read_bool(np, "pinctrl-used")) {
        pin_info->pinctrl_used = 1;
    } else {
        pin_info->pinctrl_used = 0;
        AUDIO_DRIVER_LOG_ERR("unused pinctrl");
        return 0;
    }

    pin_info->pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR_OR_NULL(pin_info->pinctrl)) {
        AUDIO_DRIVER_LOG_ERR("pinctrl get failed");
        ret = -EINVAL;
        return ret;
    }
    pin_info->pinstate = pinctrl_lookup_state(pin_info->pinctrl, PINCTRL_STATE_DEFAULT);
    if (IS_ERR_OR_NULL(pin_info->pinstate)) {
        AUDIO_DRIVER_LOG_ERR("pinctrl default state get fail");
        ret = -EINVAL;
        goto err_loopup_pinstate;
    }
    pin_info->pinstate_sleep = pinctrl_lookup_state(pin_info->pinctrl, PINCTRL_STATE_SLEEP);
    if (IS_ERR_OR_NULL(pin_info->pinstate_sleep)) {
        AUDIO_DRIVER_LOG_ERR("pinctrl sleep state get failed");
        ret = -EINVAL;
        goto err_loopup_pin_sleep;
    }
    ret = pinctrl_select_state(pin_info->pinctrl, pin_info->pinstate);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("daudio set pinctrl default state fail");
        ret = -EBUSY;
        goto err_pinctrl_select_default;
    }

    return 0;

err_pinctrl_select_default:
err_loopup_pin_sleep:
err_loopup_pinstate:
    devm_pinctrl_put(pin_info->pinctrl);
    return ret;
}

static int snd_sunxi_ahub_regulator_init(struct platform_device *pdev, struct sunxi_ahub_regulator_info *rglt_info)
{
    int ret = 0;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    rglt_info->regulator_name = NULL;
    if (of_property_read_string(np, "ahub_regulator", &rglt_info->regulator_name)) {
        AUDIO_DRIVER_LOG_ERR("regulator missing");
        rglt_info->regulator = NULL;
        return 0;
    }

    rglt_info->regulator = regulator_get(NULL, rglt_info->regulator_name);
    if (IS_ERR_OR_NULL(rglt_info->regulator)) {
        AUDIO_DRIVER_LOG_ERR("get duaido vcc-pin failed");
        ret = -EFAULT;
        goto err_regulator_get;
    }
    ret = regulator_set_voltage(rglt_info->regulator, 3300000, 3300000);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("set duaido voltage failed");
        ret = -EFAULT;
        goto err_regulator_set_vol;
    }
    ret = regulator_enable(rglt_info->regulator);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("enable duaido vcc-pin failed");
        ret = -EFAULT;
        goto err_regulator_enable;
    }

    return 0;

err_regulator_enable:
err_regulator_set_vol:
    if (rglt_info->regulator) {
        regulator_put(rglt_info->regulator);
    }
err_regulator_get:
    return ret;
};

static void snd_sunxi_ahub_regulator_exit(struct sunxi_ahub_regulator_info *rglt_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (rglt_info->regulator) {
        if (!IS_ERR_OR_NULL(rglt_info->regulator)) {
            regulator_disable(rglt_info->regulator);
            regulator_put(rglt_info->regulator);
        }
    }
}

/* sysfs debug */
static ssize_t snd_sunxi_debug_show_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = ahub_info->mem_info.regmap;
    size_t count = 0, i = 0;
    uint32_t reg_val;
    uint32_t size = ARRAY_SIZE(g_reg_labels);

    while ((i < size) && (g_reg_labels[i].name != NULL)) {
        regmap_read(regmap, g_reg_labels[i].address, &reg_val);
        printk("%-30s [0x%03x]: 0x%8x save_val:0x%x\n",
                g_reg_labels[i].name,
                g_reg_labels[i].address, reg_val,
                g_reg_labels[i].value);
        i++;
    }

    return count;
}

static ssize_t snd_sunxi_debug_store_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&g_ahub_pdev->dev);
    struct regmap *regmap = ahub_info->mem_info.regmap;
    int scanf_cnt;
    uint32_t reg_val;
    uint32_t input_reg_val = 0;
    uint32_t input_reg_offset = 0;

    scanf_cnt = sscanf(buf, "0x%x 0x%x", &input_reg_offset, &input_reg_val);
    if (scanf_cnt == 0 || scanf_cnt > 2) {
        pr_info("usage read : echo 0x > audio_reg\n");
        pr_info("usage write: echo 0x 0x > audio_reg\n");
        return count;
    }

    if (input_reg_offset > SUNXI_AHUB_MAX_REG) {
        pr_info("reg offset > audio max reg[0x%x]\n", SUNXI_AHUB_MAX_REG);
        return count;
    }

    if (scanf_cnt == 1) {
        regmap_read(regmap, input_reg_offset, &reg_val);
        pr_info("reg[0x%03x]: 0x%x\n", input_reg_offset, reg_val);
        return count;
    } else if (scanf_cnt == 2) {
        regmap_read(regmap, input_reg_offset, &reg_val);
        pr_info("reg[0x%03x]: 0x%x (old)\n", input_reg_offset, reg_val);
        regmap_write(regmap, input_reg_offset, input_reg_val);
        regmap_read(regmap, input_reg_offset, &reg_val);
        pr_info("reg[0x%03x]: 0x%x (new)\n", input_reg_offset, reg_val);
    }

    return count;
}

static DEVICE_ATTR(audio_reg, 0644, snd_sunxi_debug_show_reg, snd_sunxi_debug_store_reg);

static struct attribute *audio_debug_attrs[] = {
    &dev_attr_audio_reg.attr,
    NULL,
};

static struct attribute_group debug_attr = {
    .name   = "audio_debug",
    .attrs  = audio_debug_attrs,
};

static int sunxi_ahub_dev_probe(struct platform_device *pdev)
{
    int ret;
    struct device_node *np = pdev->dev.of_node;
    struct sunxi_ahub_info *ahub_info;
    struct sunxi_ahub_mem_info *mem_info;
    struct sunxi_ahub_clk_info *clk_info;
    struct sunxi_ahub_pinctl_info *pin_info;
    struct sunxi_ahub_dts_info *dts_info;
    struct sunxi_ahub_regulator_info *rglt_info;

    AUDIO_DRIVER_LOG_DEBUG("");

    ahub_info = devm_kzalloc(&pdev->dev, sizeof(struct sunxi_ahub_info), GFP_KERNEL);
    if (IS_ERR_OR_NULL(ahub_info)) {
        AUDIO_DRIVER_LOG_ERR("alloc sunxi_ahub_info failed");
        ret = -ENOMEM;
        goto err_devm_kzalloc;
    }
    dev_set_drvdata(&pdev->dev, ahub_info);
    ahub_info->dev = &pdev->dev;
    mem_info = &ahub_info->mem_info;
    clk_info = &ahub_info->clk_info;
    pin_info = &ahub_info->pin_info;
    dts_info = &ahub_info->dts_info;
    rglt_info = &ahub_info->rglt_info;

    ret = snd_sunxi_ahub_regulator_init(pdev, rglt_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("rglt_info init failed");
        ret = -EINVAL;
        goto err_snd_sunxi_ahub_regulator_init;
    }

    ret = snd_sunxi_ahub_mem_init(pdev, mem_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("remap init failed");
        ret = -EINVAL;
        goto err_snd_sunxi_ahub_mem_init;
    }

    ret = snd_sunxi_ahub_clk_init(pdev, clk_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("clk init failed");
        ret = -EINVAL;
        goto err_snd_sunxi_ahub_clk_init;
    }

    ret = snd_sunxi_ahub_dts_params_init(pdev, dts_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("dts init failed");
        ret = -EINVAL;
        goto err_snd_sunxi_ahub_dts_params_init;
    }

    ret = snd_sunxi_ahub_pin_init(pdev, pin_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("pinctrl init failed");
        ret = -EINVAL;
        goto err_snd_sunxi_ahub_pin_init;
    }

    ret = sysfs_create_group(&pdev->dev.kobj, &debug_attr);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("sysfs debug create failed");
    }

    g_ahub_pdev = pdev;

    AUDIO_DRIVER_LOG_DEBUG("register ahub platform success");

    return 0;

err_snd_sunxi_ahub_pin_init:
err_snd_sunxi_ahub_dts_params_init:
    snd_sunxi_clk_exit(clk_info);
err_snd_sunxi_ahub_clk_init:
    snd_sunxi_ahub_mem_exit(pdev, mem_info);
err_snd_sunxi_ahub_mem_init:
    snd_sunxi_ahub_regulator_exit(rglt_info);
err_snd_sunxi_ahub_regulator_init:
    devm_kfree(&pdev->dev, ahub_info);
err_devm_kzalloc:
    of_node_put(np);
    return ret;
}

static int sunxi_ahub_dev_remove(struct platform_device *pdev)
{
    struct sunxi_ahub_info *ahub_info = dev_get_drvdata(&pdev->dev);
    struct sunxi_ahub_mem_info *mem_info = &ahub_info->mem_info;
    struct sunxi_ahub_clk_info *clk_info = &ahub_info->clk_info;
    struct sunxi_ahub_regulator_info *rglt_info = &ahub_info->rglt_info;

    AUDIO_DRIVER_LOG_DEBUG("");

    sysfs_remove_group(&pdev->dev.kobj, &debug_attr);

    snd_sunxi_ahub_mem_exit(pdev, mem_info);
    snd_sunxi_clk_exit(clk_info);
    snd_sunxi_ahub_regulator_exit(rglt_info);

    AUDIO_DRIVER_LOG_DEBUG("unregister ahub platform success");

    return 0;
}

static const struct of_device_id sunxi_ahub_of_match[] = {
    { .compatible = "allwinner," DRV_NAME, },
    {},
};
MODULE_DEVICE_TABLE(of, sunxi_ahub_of_match);

static struct platform_driver sunxi_ahub_driver = {
    .driver = {
        .name           = DRV_NAME,
        .owner          = THIS_MODULE,
        .of_match_table = sunxi_ahub_of_match,
    },
    .probe  = sunxi_ahub_dev_probe,
    .remove = sunxi_ahub_dev_remove,
};

module_platform_driver(sunxi_ahub_driver);

MODULE_DESCRIPTION("sunxi soundcard platform of ahub");

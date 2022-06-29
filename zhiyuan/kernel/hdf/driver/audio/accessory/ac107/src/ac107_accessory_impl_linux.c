/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "ac107_accessory_impl_linux.h"
// #include "audio_accessory_base.h"
#include "audio_stream_dispatch.h"
#include "audio_driver_log.h"

#define HDF_LOG_TAG     "ac107_codec"

#define AC107_ADC_PATTERN_SEL   ADC_PTN_NORMAL    /* 0:ADC normal,  1:0x5A5A5A,  2:0x123456,  3:0x000000,  4~7:I2S_RX_DATA,  other:reserved */

/* AC107 config */
#define AC107_CHIP_NUMS         1   /* range[1, 8] */
#define AC107_CHIP_NUMS_MAX     8   /* range[1, 8] */
#define AC107_SLOT_WIDTH        32  /* 8/12/16/20/24/28/32bit Slot Width */
#define AC107_ENCODING_EN       0   /* TX Encoding mode enable */
#define AC107_ENCODING_CH_NUMS  2   /* TX Encoding channel numbers, must be dual, range[1, 16] */
#define AC107_ENCODING_FMT      0   /* TX Encoding format:    0:first channel number 0,  other:first channel number 1 */
/*range[1, 1024], default PCM mode, I2S/LJ/RJ mode shall divide by 2 */
#define AC107_LRCK_PERIOD       (AC107_SLOT_WIDTH*(AC107_ENCODING_EN ? 2 : AC107_CHIP_NUMS))
#define AC107_MATCH_DTS_EN      1   /* AC107 match method select: 0: i2c_detect, 1:devices tree */

#define AC107_KCONTROL_EN       1
#define AC107_DAPM_EN           0
#define AC107_CODEC_RW_USER_EN  1
#define AC107_PGA_GAIN          ADC_PGA_GAIN_28dB   /* -6dB and 0dB, 3~30dB, 1dB step */
#define AC107_DMIC_EN           0   /* 0:ADC  1:DMIC */
#define AC107_PDM_EN            0   /* 0:I2S  1:PDM */

#define AC107_DVCC_NAME         "ac107_dvcc_1v8"
#define AC107_AVCC_VCCIO_NAME   "ac107_avcc_vccio_3v3"
#define AC107_RATES             (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)
#define AC107_FORMATS           (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

struct i2c_client *g_ac107_i2c;

static int ac107_regulator_en;
struct i2c_client *i2c_ctrl[AC107_CHIP_NUMS_MAX];

struct ac107_voltage_supply {
    struct regulator *dvcc_1v8;
    struct regulator *avcc_vccio_3v3;
};

struct ac107_priv {
    struct i2c_client *i2c;
    struct snd_soc_component *component;
    struct ac107_voltage_supply vol_supply;
    int reset_gpio;
};

static const struct regmap_config ac107_regmap_config = {
    .reg_bits = 8,        /* Number of bits in a register address */
    .val_bits = 8,        /* Number of bits in a register value */
};

struct real_val_to_reg_val {
    uint32_t real_val;
    uint32_t reg_val;
};

struct reg_default_value {
    uint8_t reg_addr;
    uint8_t default_val;
};

#if 0
struct pll_div {
    uint32_t freq_in;
    uint32_t freq_out;
    uint32_t m1;
    uint32_t m2;
    uint32_t n;
    uint32_t k1;
    uint32_t k2;
};
#endif

static const struct real_val_to_reg_val ac107_sample_rate[] = {
    {8000, 0},
    {11025, 1},
    {12000, 2},
    {16000, 3},
    {22050, 4},
    {24000, 5},
    {32000, 6},
    {44100, 7},
    {48000, 8},
};

static const struct real_val_to_reg_val ac107_bclk_div[] = {
    {0, 0},
    {1, 1},
    {2, 2},
    {4, 3},
    {6, 4},
    {8, 5},
    {12, 6},
    {16, 7},
    {24, 8},
    {32, 9},
    {48, 10},
    {64, 11},
    {96, 12},
    {128, 13},
    {176, 14},
    {192, 15},
};

#if 0
//FOUT =(FIN * N) / [(M1+1) * (M2+1)*(K1+1)*(K2+1)] ;    M1[0,31],  M2[0,1],  N[0,1023],  K1[0,31],  K2[0,1]
static const struct pll_div ac107_pll_div[] = {
    {400000, 12288000, 0, 0, 983, 15, 1},           //<out: 12.2875M>
    {512000, 12288000, 0, 0, 960, 19, 1},           //24576000/48
    {768000, 12288000, 0, 0, 640, 19, 1},           //24576000/32
    {800000, 12288000, 0, 0, 768, 24, 1},
    {1024000, 12288000, 0, 0, 480, 19, 1},          //24576000/24
    {1600000, 12288000, 0, 0, 384, 24, 1},
    {2048000, 12288000, 0, 0, 240, 19, 1},          //24576000/12
    {3072000, 12288000, 0, 0, 160, 19, 1},          //24576000/8
    {4096000, 12288000, 0, 0, 120, 19, 1},          //24576000/6
    {6000000, 12288000, 4, 0, 512, 24, 1},
    {6144000, 12288000, 1, 0, 160, 19, 1},          //24576000/4
    {12000000, 12288000, 9, 0, 512, 24, 1},
    {13000000, 12288000, 12, 0, 639, 25, 1},        //<out: 12.2885M>
    {15360000, 12288000, 9, 0, 320, 19, 1},
    {16000000, 12288000, 9, 0, 384, 24, 1},
    {19200000, 12288000, 11, 0, 384, 24, 1},
    {19680000, 12288000, 15, 1, 999, 24, 1},        //<out: 12.2877M>
    {24000000, 12288000, 9, 0, 256, 24, 1},

    {400000, 11289600, 0, 0, 1016, 17, 1},          //<out: 11.2889M>
    {512000, 11289600, 0, 0, 882, 19, 1},
    {768000, 11289600, 0, 0, 588, 19, 1},
    {800000, 11289600, 0, 0, 508, 17, 1},           //<out: 11.2889M>
    {1024000, 11289600, 0, 0, 441, 19, 1},
    {1600000, 11289600, 0, 0, 254, 17, 1},          //<out: 11.2889M>
    {2048000, 11289600, 1, 0, 441, 19, 1},
    {3072000, 11289600, 0, 0, 147, 19, 1},
    {4096000, 11289600, 3, 0, 441, 19, 1},
    {6000000, 11289600, 1, 0, 143, 18, 1},          //<out: 11.2895M>
    {6144000, 11289600, 1, 0, 147, 19, 1},
    {12000000, 11289600, 3, 0, 143, 18, 1},         //<out: 11.2895M>
    {13000000, 11289600, 12, 0, 429, 18, 1},        //<out: 11.2895M>
    {15360000, 11289600, 14, 0, 441, 19, 1},
    {16000000, 11289600, 24, 0, 882, 24, 1},
    {19200000, 11289600, 4, 0, 147, 24, 1},
    {19680000, 11289600, 13, 1, 771, 23, 1},        //<out: 11.28964M>
    {24000000, 11289600, 24, 0, 588, 24, 1},

    {12288000, 12288000, 9, 0, 400, 19, 1},         //24576000/2
    {11289600, 11289600, 9, 0, 400, 19, 1},         //22579200/2

    {24576000 / 1, 12288000, 9, 0, 200, 19, 1},     //24576000
    {24576000 / 16, 12288000, 0, 0, 320, 19, 1},    //1536000
    {24576000 / 64, 12288000, 0, 0, 640, 9, 1},     //384000
    {24576000 / 96, 12288000, 0, 0, 960, 9, 1},     //256000
    {24576000 / 128, 12288000, 0, 0, 512, 3, 1},    //192000
    {24576000 / 176, 12288000, 0, 0, 880, 4, 1},    //140000
    {24576000 / 192, 12288000, 0, 0, 960, 4, 1},    //128000

    {22579200 / 1, 11289600, 9, 0, 200, 19, 1},     //22579200
    {22579200 / 4, 11289600, 4, 0, 400, 19, 1},     //5644800
    {22579200 / 16, 11289600, 0, 0, 320, 19, 1},    //1411200
    {22579200 / 64, 11289600, 0, 0, 640, 9, 1},     //352800
    {22579200 / 96, 11289600, 0, 0, 960, 9, 1},     //235200
    {22579200 / 128, 11289600, 0, 0, 512, 3, 1},    //176400
    {22579200 / 176, 11289600, 0, 0, 880, 4, 1},    //128290
    {22579200 / 192, 11289600, 0, 0, 960, 4, 1},    //117600

    {22579200 / 6, 11289600, 2, 0, 360, 19, 1},     //3763200
    {22579200 / 8, 11289600, 0, 0, 160, 19, 1},     //2822400
    {22579200 / 12, 11289600, 0, 0, 240, 19, 1},    //1881600
    {22579200 / 24, 11289600, 0, 0, 480, 19, 1},    //940800
    {22579200 / 32, 11289600, 0, 0, 640, 19, 1},    //705600
    {22579200 / 48, 11289600, 0, 0, 960, 19, 1},    //470400
};
#endif

const struct reg_default_value ac107_reg_default_value[] = {
    /*** Chip reset ***/
    {CHIP_AUDIO_RST, 0x4B},

    /*** Power Control ***/
    {PWR_CTRL1, 0x00},
    {PWR_CTRL2, 0x11},

    /*** PLL Configure Control ***/
    {PLL_CTRL1, 0x48},
    {PLL_CTRL2, 0x00},
    {PLL_CTRL3, 0x03},
    {PLL_CTRL4, 0x0D},
    {PLL_CTRL5, 0x00},
    {PLL_CTRL6, 0x0F},
    {PLL_CTRL7, 0xD0},
    {PLL_LOCK_CTRL, 0x00},

    /*** System Clock Control ***/
    {SYSCLK_CTRL, 0x00},
    {MOD_CLK_EN, 0x00},
    {MOD_RST_CTRL, 0x00},

    /*** I2S Common Control ***/
    {I2S_CTRL, 0x00},
    {I2S_BCLK_CTRL, 0x00},
    {I2S_LRCK_CTRL1, 0x00},
    {I2S_LRCK_CTRL2, 0x00},
    {I2S_FMT_CTRL1, 0x00},
    {I2S_FMT_CTRL2, 0x55},
    {I2S_FMT_CTRL3, 0x60},

    /*** I2S TX Control ***/
    {I2S_TX_CTRL1, 0x00},
    {I2S_TX_CTRL2, 0x00},
    {I2S_TX_CTRL3, 0x00},
    {I2S_TX_CHMP_CTRL1, 0x00},
    {I2S_TX_CHMP_CTRL2, 0x00},

    /*** I2S RX Control ***/
    {I2S_RX_CTRL1, 0x00},
    {I2S_RX_CTRL2, 0x03},
    {I2S_RX_CTRL3, 0x00},
    {I2S_RX_CHMP_CTRL1, 0x00},
    {I2S_RX_CHMP_CTRL2, 0x00},

    /*** PDM Control ***/
    {PDM_CTRL, 0x00},

    /*** ADC Common Control ***/
    {ADC_SPRC, 0x00},
    {ADC_DIG_EN, 0x00},
    {DMIC_EN, 0x00},
    {HPF_EN, 0x03},

    /*** ADC Digital Channel Volume Control ***/
    {ADC1_DVOL_CTRL, 0xA0},
    {ADC2_DVOL_CTRL, 0xA0},

    /*** ADC Digital Mixer Source and Gain Control ***/
    {ADC1_DMIX_SRC, 0x01},
    {ADC2_DMIX_SRC, 0x02},

    /*** ADC_DIG_DEBUG ***/
    {ADC_DIG_DEBUG, 0x00},

    /*** Pad Function and Drive Control ***/
    {ADC_ANA_DEBUG1, 0x11},
    {ADC_ANA_DEBUG2, 0x11},
    {I2S_PADDRV_CTRL, 0x55},

    /*** ADC1 Analog Control ***/
    {ANA_ADC1_CTRL1, 0x00},
    {ANA_ADC1_CTRL2, 0x00},
    {ANA_ADC1_CTRL3, 0x00},
    {ANA_ADC1_CTRL4, 0x00},
    {ANA_ADC1_CTRL5, 0x00},

    /*** ADC2 Analog Control ***/
    {ANA_ADC2_CTRL1, 0x00},
    {ANA_ADC2_CTRL2, 0x00},
    {ANA_ADC2_CTRL3, 0x00},
    {ANA_ADC2_CTRL4, 0x00},
    {ANA_ADC2_CTRL5, 0x00},

    /*** ADC Dither Control ***/
    {ADC_DITHER_CTRL, 0x00},
};

#define REG_LABEL(constant) {#constant, constant, 0}
#define REG_LABEL_END       {NULL, 0, 0}

struct reg_label {
    const char *name;
    const uint32_t address;
    uint32_t value;
};

static struct reg_label g_reg_labels[] = {
    REG_LABEL(CHIP_AUDIO_RST),
    REG_LABEL(PWR_CTRL1),
    REG_LABEL(PWR_CTRL2),
    REG_LABEL(PLL_CTRL1),
    REG_LABEL(PLL_CTRL2),
    REG_LABEL(PLL_CTRL3),
    REG_LABEL(PLL_CTRL4),
    REG_LABEL(PLL_CTRL5),
    REG_LABEL(PLL_CTRL6),
    REG_LABEL(PLL_CTRL7),
    REG_LABEL(PLL_LOCK_CTRL),
    REG_LABEL(SYSCLK_CTRL),
    REG_LABEL(MOD_CLK_EN),
    REG_LABEL(MOD_RST_CTRL),
    REG_LABEL(I2S_CTRL),
    REG_LABEL(I2S_BCLK_CTRL),
    REG_LABEL(I2S_LRCK_CTRL1),
    REG_LABEL(I2S_LRCK_CTRL2),
    REG_LABEL(I2S_FMT_CTRL1),
    REG_LABEL(I2S_FMT_CTRL2),
    REG_LABEL(I2S_FMT_CTRL3),
    REG_LABEL(I2S_TX_CTRL1),
    REG_LABEL(I2S_TX_CTRL2),
    REG_LABEL(I2S_TX_CTRL3),
    REG_LABEL(I2S_TX_CHMP_CTRL1),
    REG_LABEL(I2S_TX_CHMP_CTRL2),
    REG_LABEL(I2S_RX_CTRL1),
    REG_LABEL(I2S_RX_CTRL2),
    REG_LABEL(I2S_RX_CTRL3),
    REG_LABEL(I2S_RX_CHMP_CTRL1),
    REG_LABEL(I2S_RX_CHMP_CTRL2),
    REG_LABEL(PDM_CTRL),
    REG_LABEL(ADC_SPRC),
    REG_LABEL(ADC_DIG_EN),
    REG_LABEL(DMIC_EN),
    REG_LABEL(HPF_EN),
    REG_LABEL(ADC1_DVOL_CTRL),
    REG_LABEL(ADC2_DVOL_CTRL),
    REG_LABEL(ADC1_DMIX_SRC),
    REG_LABEL(ADC2_DMIX_SRC),
    REG_LABEL(ADC_DIG_DEBUG),
    REG_LABEL(ADC_ANA_DEBUG1),
    REG_LABEL(ADC_ANA_DEBUG2),
    REG_LABEL(I2S_PADDRV_CTRL),
    REG_LABEL(ANA_ADC1_CTRL1),
    REG_LABEL(ANA_ADC1_CTRL2),
    REG_LABEL(ANA_ADC1_CTRL3),
    REG_LABEL(ANA_ADC1_CTRL4),
    REG_LABEL(ANA_ADC1_CTRL5),
    REG_LABEL(ANA_ADC2_CTRL1),
    REG_LABEL(ANA_ADC2_CTRL2),
    REG_LABEL(ANA_ADC2_CTRL3),
    REG_LABEL(ANA_ADC2_CTRL4),
    REG_LABEL(ANA_ADC2_CTRL5),
    REG_LABEL(ADC_DITHER_CTRL),
    REG_LABEL_END,
};

const uint8_t ac107_kcontrol_dapm_reg[] = {
#if AC107_KCONTROL_EN
    ANA_ADC1_CTRL3, ANA_ADC2_CTRL3, ADC1_DVOL_CTRL, ADC2_DVOL_CTRL,
    ADC1_DMIX_SRC, ADC2_DMIX_SRC, ADC_DIG_DEBUG,
#endif

#if AC107_DAPM_EN
    DMIC_EN, ADC1_DMIX_SRC, ADC2_DMIX_SRC, I2S_TX_CHMP_CTRL1,
    I2S_TX_CHMP_CTRL2, ANA_ADC1_CTRL5, ANA_ADC2_CTRL5, ADC_DIG_EN,
#endif
};

static int ac107_read(uint8_t reg, uint8_t *rt_value, struct i2c_client *client);
static int ac107_write(uint8_t reg, unsigned char value, struct i2c_client *client);
static int ac107_update_bits(uint8_t reg, uint8_t mask, uint8_t value, struct i2c_client *client);
static int ac107_update_bits(uint8_t reg, uint8_t mask, uint8_t value, struct i2c_client *client);
static int ac107_multi_chips_write(uint8_t reg, unsigned char value);
static int ac107_multi_chips_update_bits(uint8_t reg, uint8_t mask, uint8_t value);

/*******************************************************************************
 *  for adm api
 ******************************************************************************/
int32_t Ac107DeviceInit(struct AudioCard *audioCard)
{
    //struct ac107_priv *ac107 = dev_get_drvdata(&g_ac107_i2c->dev);

    (void)audioCard;
 
    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

int32_t Ac107DeviceReadReg(uint32_t reg, uint32_t *value)
{
    ac107_read((unsigned char)reg, (unsigned char *)(value), g_ac107_i2c);

    return HDF_SUCCESS;
}

int32_t Ac107DeviceWriteReg(uint32_t reg, uint32_t value)
{
    ac107_write((unsigned char)reg, (unsigned char)(value), g_ac107_i2c);

    return HDF_SUCCESS;
}

int32_t Ac107DaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device)
{
    (void)card;
    (void)device;

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

int32_t Ac107DaiStartup(const struct AudioCard *card, const struct DaiDevice *device)
{
    (void)card;
    (void)device;

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static int ac107_set_pll(uint32_t freq)
{
    uint32_t freq_in = freq;
    int pll_id = PLLCLK_SRC_MCLK;
    int clk_id = SYSCLK_SRC_MCLK;
    int div_id = 0;

    /* set pllclk (from mclk) */
    freq_in = freq_in / 2;
    if (freq_in < 128000 || freq_in > 24576000) {
        AUDIO_DRIVER_LOG_ERR("AC107 PLLCLK source input freq only support [128K,24M],while now %u", freq_in);
        return -EINVAL;
    } else if ((freq_in == 12288000 || freq_in == 11289600) && (pll_id == PLLCLK_SRC_MCLK || pll_id == PLLCLK_SRC_BCLK)) {
        //System Clock Source Select MCLK/BCLK, SYSCLK Enable
        AUDIO_DRIVER_LOG_DEBUG("AC107 don't need to use PLL, SYSCLK source select %s", pll_id ? "BCLK" : "MCLK");
        ac107_multi_chips_update_bits(SYSCLK_CTRL, 0x3 << SYSCLK_SRC | 0x1 << SYSCLK_EN, pll_id << SYSCLK_SRC | 0x1 << SYSCLK_EN);
        return 0;
    }
    //Don't need to use PLL

    /* set sysclk (from mclk) */
    switch (clk_id) {
        case SYSCLK_SRC_MCLK:
            AUDIO_DRIVER_LOG_DEBUG("AC107 SYSCLK source select MCLK");
            ac107_multi_chips_update_bits(SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_MCLK << SYSCLK_SRC);
            break;
        case SYSCLK_SRC_BCLK:
            AUDIO_DRIVER_LOG_DEBUG("AC107 SYSCLK source select BCLK");
            ac107_multi_chips_update_bits(SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_BCLK << SYSCLK_SRC);
            break;
        case SYSCLK_SRC_PLL:
            AUDIO_DRIVER_LOG_DEBUG("AC107 SYSCLK source select PLL");
            ac107_multi_chips_update_bits(SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_PLL << SYSCLK_SRC);
            break;
        default:
            AUDIO_DRIVER_LOG_DEBUG("AC107 SYSCLK source config error:%d", clk_id);
            return -EINVAL;
    }

    /* set clkdiv (Slave mode) */
    if (!div_id) {    /* use div_id to judge Master/Slave mode,  0: Slave mode, 1: Master mode */
        AUDIO_DRIVER_LOG_DEBUG("AC107 work as Slave mode, don't need to config BCLK_DIV");
    } else {
        return -EINVAL;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
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

static int ac107_set_fmt(uint32_t fmt)
{
    uint8_t i, tx_offset, i2s_mode, sign_ext, lrck_polarity, brck_polarity;
    struct ac107_priv *ac107 = dev_get_drvdata(&g_ac107_i2c->dev);

    AUDIO_DRIVER_LOG_DEBUG("");

    //AC107 config Master/Slave mode
    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBM_CFM:    //AC107 Master
            AUDIO_DRIVER_LOG_DEBUG("AC107 set to work as Master");
            ac107_update_bits(I2S_CTRL, 0x3 << LRCK_IOEN, 0x3 << LRCK_IOEN, ac107->i2c);    //BCLK & LRCK output
            break;
        case SND_SOC_DAIFMT_CBS_CFS:    //AC107 Slave
            AUDIO_DRIVER_LOG_DEBUG("AC107 set to work as Slave");
            ac107_update_bits(I2S_CTRL, 0x3 << LRCK_IOEN, 0x0 << LRCK_IOEN, ac107->i2c);    //BCLK & LRCK input
            break;
        default:
            AUDIO_DRIVER_LOG_DEBUG("AC107 Master/Slave mode config error:%u", (fmt & SND_SOC_DAIFMT_MASTER_MASK) >> 12);
            return -EINVAL;
    }
    for (i = 0; i < AC107_CHIP_NUMS; i++) {    //multi_chips: only one chip set as Master, and the others also need to set as Slave
        if (i2c_ctrl[i] == ac107->i2c) {
            continue;
        }
        ac107_update_bits(I2S_CTRL, 0x3 << LRCK_IOEN, 0x0 << LRCK_IOEN, i2c_ctrl[i]);
    }

    //AC107 config I2S/LJ/RJ/PCM format
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_I2S:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config I2S format");
            i2s_mode = LEFT_JUSTIFIED_FORMAT;
            tx_offset = 1;
            sign_ext = TRANSFER_ZERO_AFTER;
            break;
        case SND_SOC_DAIFMT_RIGHT_J:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config RIGHT-JUSTIFIED format");
            i2s_mode = RIGHT_JUSTIFIED_FORMAT;
            tx_offset = 0;
            sign_ext = SIGN_EXTENSION_MSB;
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config LEFT-JUSTIFIED format");
            i2s_mode = LEFT_JUSTIFIED_FORMAT;
            tx_offset = 0;
            sign_ext = TRANSFER_ZERO_AFTER;
            break;
        case SND_SOC_DAIFMT_DSP_A:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config PCM-A format");
            i2s_mode = PCM_FORMAT;
            tx_offset = 1;
            sign_ext = TRANSFER_ZERO_AFTER;
            break;
        case SND_SOC_DAIFMT_DSP_B:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config PCM-B format");
            i2s_mode = PCM_FORMAT;
            tx_offset = 0;
            sign_ext = TRANSFER_ZERO_AFTER;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("AC107 I2S format config error:%u", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
            return -EINVAL;
    }
    ac107_multi_chips_update_bits(I2S_FMT_CTRL1,
                      0x3 << MODE_SEL | 0x1 << TX_OFFSET,
                      i2s_mode << MODE_SEL | tx_offset <<
                      TX_OFFSET);
    ac107_multi_chips_update_bits(I2S_FMT_CTRL3, 0x3 << SEXT,
                      sign_ext << SEXT);

    //AC107 config BCLK&LRCK polarity
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config BCLK&LRCK polarity: BCLK_normal,LRCK_normal");
            brck_polarity = BCLK_NORMAL_DRIVE_N_SAMPLE_P;
            lrck_polarity = LRCK_LEFT_LOW_RIGHT_HIGH;
            break;
        case SND_SOC_DAIFMT_NB_IF:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config BCLK&LRCK polarity: BCLK_normal,LRCK_invert");
            brck_polarity = BCLK_NORMAL_DRIVE_N_SAMPLE_P;
            lrck_polarity = LRCK_LEFT_HIGH_RIGHT_LOW;
            break;
        case SND_SOC_DAIFMT_IB_NF:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config BCLK&LRCK polarity: BCLK_invert,LRCK_normal");
            brck_polarity = BCLK_INVERT_DRIVE_P_SAMPLE_N;
            lrck_polarity = LRCK_LEFT_LOW_RIGHT_HIGH;
            break;
        case SND_SOC_DAIFMT_IB_IF:
            AUDIO_DRIVER_LOG_DEBUG("AC107 config BCLK&LRCK polarity: BCLK_invert,LRCK_invert");
            brck_polarity = BCLK_INVERT_DRIVE_P_SAMPLE_N;
            lrck_polarity = LRCK_LEFT_HIGH_RIGHT_LOW;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("AC107 config BCLK/LRCLK polarity error:%u", (fmt & SND_SOC_DAIFMT_INV_MASK) >> 8);
            return -EINVAL;
    }
    ac107_multi_chips_update_bits(I2S_BCLK_CTRL, 0x1 << BCLK_POLARITY, brck_polarity << BCLK_POLARITY);
    ac107_multi_chips_update_bits(I2S_LRCK_CTRL1, 0x1 << LRCK_POLARITY, lrck_polarity << LRCK_POLARITY);

    return 0;
}

static void ac107_hw_init(struct i2c_client *i2c)
{
    uint8_t reg_val;

    /*** Analog voltage enable ***/
    ac107_write(PWR_CTRL1, 0x80, i2c);  /*0x01=0x80: VREF Enable */
    ac107_write(PWR_CTRL2, 0x55, i2c);  /*0x02=0x55: MICBIAS1&2 Enable */

    /*** SYSCLK Config ***/
    ac107_update_bits(SYSCLK_CTRL, 0x1 << SYSCLK_EN, 0x1 << SYSCLK_EN, i2c);    /*SYSCLK Enable */
    ac107_write(MOD_CLK_EN, 0x07, i2c);     /*0x21=0x07: Module clock enable<I2S, ADC digital,  ADC analog> */
    ac107_write(MOD_RST_CTRL, 0x03, i2c);   /*0x22=0x03: Module reset de-asserted<I2S, ADC digital> */

    /*** I2S Common Config ***/
    ac107_update_bits(I2S_CTRL, 0x1 << SDO_EN, 0x1 << SDO_EN, i2c); /*SDO enable */
    ac107_update_bits(I2S_BCLK_CTRL, 0x1 << EDGE_TRANSFER, 0x0 << EDGE_TRANSFER, i2c);  /*SDO drive data and SDI sample data at the different BCLK edge */
    ac107_update_bits(I2S_LRCK_CTRL1, 0x3 << LRCK_PERIODH, ((AC107_LRCK_PERIOD - 1) >> 8) << LRCK_PERIODH, i2c);
    ac107_write(I2S_LRCK_CTRL2, (uint8_t) (AC107_LRCK_PERIOD - 1), i2c); /*config LRCK period */
    /*Encoding mode format select 0~N-1, Encoding mode enable, Turn to hi-z state (TDM) when not transferring slot */
    ac107_update_bits(I2S_FMT_CTRL1, (0x1 << ENCD_FMT | 0x1 << ENCD_SEL | 0x1 << TX_SLOT_HIZ | 0x1 << TX_STATE),
                     (((!!AC107_ENCODING_FMT) << ENCD_FMT) | (((!!AC107_ENCODING_EN) << ENCD_SEL) | 0x0 << TX_SLOT_HIZ | 0x1 << TX_STATE)),
                     i2c);
    ac107_update_bits(I2S_FMT_CTRL2, 0x7 << SLOT_WIDTH_SEL, (AC107_SLOT_WIDTH / 4 - 1) << SLOT_WIDTH_SEL, i2c);    /*8/12/16/20/24/28/32bit Slot Width */
    /*0x36=0x60: TX MSB first, SDOUT normal, PCM frame type, Linear PCM Data Mode */
    ac107_update_bits(I2S_FMT_CTRL3,
                     (0x1 << TX_MLS | 0x1 << SDOUT_MUTE | 0x1 << LRCK_WIDTH | 0x3 << TX_PDM),
                     (0x0 << TX_MLS | 0x0 << SDOUT_MUTE | 0x0 << LRCK_WIDTH | 0x0 << TX_PDM),
                     i2c);

    ac107_update_bits(I2S_TX_CHMP_CTRL1, !AC107_DAPM_EN * 0xff, 0xaa, i2c);    /*0x3c=0xaa: TX CH1/3/5/7 map to adc1, TX CH2/4/6/8 map to adc2 */
    ac107_update_bits(I2S_TX_CHMP_CTRL2, !AC107_DAPM_EN * 0xff, 0xaa, i2c);    /*0x3d=0xaa: TX CH9/11/13/15 map to adc1, TX CH10/12/14/16 map to adc2 */

    /*PDM Interface Latch ADC1 data on rising clock edge. Latch ADC2 data on falling clock edge, PDM Enable */
    ac107_update_bits(PDM_CTRL, (0x1 << PDM_TIMING | 0x1 << PDM_EN), (0x0 << PDM_TIMING | ((!!AC107_PDM_EN) << PDM_EN)), i2c);

    /*** ADC DIG part Config***/
    ac107_update_bits(ADC_DIG_EN, !AC107_DAPM_EN * 0x7, 0x7, i2c);    /*0x61=0x07: Digital part globe enable, ADCs digital part enable */
    ac107_update_bits(DMIC_EN, !AC107_DAPM_EN * 0x1, !!AC107_DMIC_EN, i2c);    /*DMIC Enable */

    /* ADC pattern select */
#if AC107_KCONTROL_EN
    ac107_read(ADC_DIG_DEBUG, &reg_val, i2c);
    ac107_write(HPF_EN, !(reg_val & 0x7) * 0x03, i2c);
#else
    ac107_write(HPF_EN, !AC107_ADC_PATTERN_SEL * 0x03, i2c);
    ac107_update_bits(ADC_DIG_DEBUG, 0x7 << ADC_PTN_SEL, (AC107_ADC_PATTERN_SEL & 0x7) << ADC_PTN_SEL, i2c);
#endif

    //ADC Digital Volume Config
    ac107_update_bits(ADC1_DVOL_CTRL, !AC107_KCONTROL_EN * 0xff, 0xA0, i2c);
    ac107_update_bits(ADC2_DVOL_CTRL, !AC107_KCONTROL_EN * 0xff, 0xA0, i2c);

    /*** ADCs analog PGA gain Config***/
    ac107_update_bits(ANA_ADC1_CTRL3, !AC107_KCONTROL_EN * 0x1f << RX1_PGA_GAIN_CTRL, AC107_PGA_GAIN << RX1_PGA_GAIN_CTRL, i2c);
    ac107_update_bits(ANA_ADC2_CTRL3, !AC107_KCONTROL_EN * 0x1f << RX2_PGA_GAIN_CTRL, AC107_PGA_GAIN << RX2_PGA_GAIN_CTRL, i2c);

    /*** ADCs analog global Enable***/
    ac107_update_bits(ANA_ADC1_CTRL5, !AC107_DAPM_EN * 0x1 << RX1_GLOBAL_EN, 0x1 << RX1_GLOBAL_EN, i2c);
    ac107_update_bits(ANA_ADC2_CTRL5, !AC107_DAPM_EN * 0x1 << RX2_GLOBAL_EN, 0x1 << RX2_GLOBAL_EN, i2c);

    //VREF Fast Start-up Disable
    ac107_update_bits(PWR_CTRL1, 0x1 << VREF_FSU_DISABLE, 0x1 << VREF_FSU_DISABLE, i2c);
}

static int ac107_hw_params(enum AudioStreamType streamType, enum AudioFormat format, uint32_t channels, uint32_t rate)
{
    u16 i, channels_tmp, channels_en, sample_resolution;
    struct ac107_priv *ac107 = dev_get_drvdata(&g_ac107_i2c->dev);

    AUDIO_DRIVER_LOG_DEBUG("");

    //AC107 hw init
    ac107_hw_init(i2c_ctrl[0]);

    //AC107 set sample rate
    for (i = 0; i < ARRAY_SIZE(ac107_sample_rate); i++) {
        if (ac107_sample_rate[i].real_val == rate / (AC107_ENCODING_EN ? AC107_ENCODING_CH_NUMS / 2 : 1)) {
            ac107_multi_chips_update_bits(ADC_SPRC, 0xf << ADC_FS_I2S, ac107_sample_rate[i].reg_val << ADC_FS_I2S);
            break;
        }
    }

    //AC107 set channels
    channels_tmp = channels * (AC107_ENCODING_EN ? AC107_ENCODING_CH_NUMS / 2 : 1);
    for (i = 0; i < (channels_tmp + 1) / 2; i++) {
        channels_en = (channels_tmp >= 2 * (i + 1)) ? 0x0003 << (2 * i) : ((1 << (channels_tmp % 2)) - 1) << (2 * i);
        ac107_write(I2S_TX_CTRL1, channels_tmp - 1, i2c_ctrl[i]);
        ac107_write(I2S_TX_CTRL2, (uint8_t) channels_en, i2c_ctrl[i]);
        ac107_write(I2S_TX_CTRL3, channels_en >> 8, i2c_ctrl[i]);
    }
    for (; i < AC107_CHIP_NUMS; i++) {
        ac107_write(I2S_TX_CTRL1, 0, i2c_ctrl[i]);
        ac107_write(I2S_TX_CTRL2, 0, i2c_ctrl[i]);
        ac107_write(I2S_TX_CTRL3, 0, i2c_ctrl[i]);
    }

    //AC107 set sample resorution
    switch (format) {
        case AUDIO_FORMAT_PCM_8_BIT:
            sample_resolution = 8;
            break;
        case AUDIO_FORMAT_PCM_16_BIT:
            sample_resolution = 16;
            break;
        /*
        case SNDRV_PCM_FORMAT_S20_3LE:
            sample_resolution = 20;
            break;
        */
        case AUDIO_FORMAT_PCM_24_BIT:
            sample_resolution = 24;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            sample_resolution = 32;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("AC107 don't supported the sample resolution: %u", format);
            return -EINVAL;
    }
    ac107_multi_chips_update_bits(I2S_FMT_CTRL2, 0x7 << SAMPLE_RESOLUTION, (sample_resolution / 4 - 1) << SAMPLE_RESOLUTION);

#if 0
    //AC107 TX enable, Globle enable
    ac107_multi_chips_update_bits(I2S_CTRL, 0x1 << TXEN | 0x1 << GEN, 0x1 << TXEN | 0x1 << GEN);
#endif

    //AC107 PLL Enable and through MCLK Pin output Enable
    ac107_read(SYSCLK_CTRL, (uint8_t *)&i, ac107->i2c);
    if (i & 0x80) {         //PLLCLK Enable
        if (!(i & 0x0c)) {  //SYSCLK select MCLK
            //MCLK output Clock 24MHz from DPLL
            ac107_update_bits(I2S_CTRL, 0x1 << MCLK_IOEN, 0x1 << MCLK_IOEN, ac107->i2c);
            ac107_update_bits(I2S_PADDRV_CTRL, 0x03 << MCLK_DRV, 0x03 << MCLK_DRV, ac107->i2c);
            for (i = 0; i < AC107_CHIP_NUMS; i++) {    //multi_chips: only one chip MCLK output PLL_test, and the others MCLK config as input
                if (i2c_ctrl[i] == ac107->i2c) {
                    continue;
                }
                ac107_update_bits(I2S_CTRL, 0x1 << MCLK_IOEN, 0x0 << MCLK_IOEN, i2c_ctrl[i]);
            }
            //the chip which MCLK config as output, should select PLL as its SYCCLK source
            ac107_update_bits(SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_PLL << SYSCLK_SRC, ac107->i2c);
            //the chip which MCLK config as output, PLL Common voltage Enable, PLL Enable
            ac107_update_bits(PLL_CTRL1, 0x1 << PLL_EN | 0x1 << PLL_COM_EN, 0x1 << PLL_EN | 0x1 << PLL_COM_EN, ac107->i2c);
        } else if ((i & 0x0c) >> 2 == 0x2) {    //SYSCLK select PLL
            ac107_multi_chips_update_bits(PLL_LOCK_CTRL, 0x7 << SYSCLK_HOLD_TIME, 0x3 << SYSCLK_HOLD_TIME);
            //All chips PLL Common voltage Enable, PLL Enable
            ac107_multi_chips_update_bits(PLL_CTRL1, 0x1 << PLL_EN | 0x1 << PLL_COM_EN, 0x1 << PLL_EN | 0x1 << PLL_COM_EN);
        }
    }

    return 0;
}

int32_t Ac107DaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param)
{
    int ret;
    uint32_t freq_point;

    uint32_t dai_fmt = 0;
    dai_fmt |= SND_SOC_DAIFMT_I2S;
    dai_fmt |= SND_SOC_DAIFMT_NB_NF;
    dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;

    (void)card;

    if (param->streamType == AUDIO_RENDER_STREAM) {
        /* ac107 unsupport render */
        return HDF_SUCCESS;
    }

    /* set pll clk */
    switch (param->rate) {
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
            AUDIO_DRIVER_LOG_ERR("rate: %d is not define.", param->rate);
            return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG(" freq_point %u", freq_point);

    ret = ac107_set_pll(freq_point);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("ac107_set_pll failed");
        return HDF_FAILURE;
    }

    /* set fmt */
    ret = ac107_set_fmt(dai_fmt);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("ac107_set_fmt failed");
        return HDF_FAILURE;
    }

    /* set pcm info */
    ret = ac107_hw_params(param->streamType, param->format, param->channels, param->rate);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("ac107_set_fmt failed");
        return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

int32_t Ac107DaiTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *dai)
{
    uint8_t i, j;

    AUDIO_DRIVER_LOG_DEBUG(" cmd -> %d", cmd);

    (void)card;
    (void)dai;

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
            /* ac107 unsupport render */
            break;
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
            /* ac107 unsupport render */
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
            //AC107 TX enable, Globle enable
            ac107_multi_chips_update_bits(I2S_CTRL, 0x1 << TXEN | 0x1 << GEN, 0x1 << TXEN | 0x1 << GEN);
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            ac107_multi_chips_update_bits(I2S_CTRL, 0x1 << TXEN | 0x0 << GEN, 0x1 << TXEN | 0x0 << GEN);

            #if AC107_KCONTROL_EN || AC107_DAPM_EN
                for (i = 0; i < ARRAY_SIZE(ac107_reg_default_value); i++) {
                    for (j = 0; j < sizeof(ac107_kcontrol_dapm_reg); j++) {
                        if (ac107_reg_default_value[i].reg_addr == ac107_kcontrol_dapm_reg[j]) {
                            break;
                        }
                    }
                    if (j == sizeof(ac107_kcontrol_dapm_reg)) {
                        ac107_multi_chips_write(ac107_reg_default_value[i].reg_addr,
                                                ac107_reg_default_value[i].default_val);
                    }
                }

            #else
                AUDIO_DRIVER_LOG_DEBUG("AC107 reset all register to their default value");
                ac107_multi_chips_write(CHIP_AUDIO_RST, 0x12);
            #endif
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("unsupport cmd %d", cmd);
            return HDF_FAILURE;
    }

    AUDIO_DRIVER_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

/*******************************************************************************
 * for linux probe
*******************************************************************************/
static int ac107_read(uint8_t reg, uint8_t *rt_value, struct i2c_client *client)
{
    int ret;
    uint8_t read_cmd[3] = { 0 };
    uint8_t cmd_len = 0;

    read_cmd[0] = reg;
    cmd_len = 1;

    if (client == NULL || client->adapter == NULL) {
        AUDIO_DRIVER_LOG_ERR("ac107_read client or client->adapter is NULL");
        return -1;
    }

    ret = i2c_master_send(client, read_cmd, cmd_len);
    if (ret != cmd_len) {
        AUDIO_DRIVER_LOG_ERR("ac107_read error1->[REG-0x%02x]", reg);
        return -1;
    }

    ret = i2c_master_recv(client, rt_value, 1);
    if (ret != 1) {
        AUDIO_DRIVER_LOG_ERR("ac107_read error2->[REG-0x%02x], ret=%d", reg, ret);
        return -1;
    }

    return 0;
}

static int ac107_write(uint8_t reg, unsigned char value, struct i2c_client *client)
{
    int ret = 0;
    uint8_t write_cmd[2] = { 0 };

    write_cmd[0] = reg;
    write_cmd[1] = value;

    if (client == NULL || client->adapter == NULL) {
        AUDIO_DRIVER_LOG_ERR("ac107_write client or client->adapter is NULL");
        return -1;
    }

    ret = i2c_master_send(client, write_cmd, 2);
    if (ret != 2) {
        AUDIO_DRIVER_LOG_ERR("ac107_write error->[REG-0x%02x,val-0x%02x]", reg, value);
        return -1;
    }

    return 0;
}

static int ac107_update_bits(uint8_t reg, uint8_t mask, uint8_t value, struct i2c_client *client)
{
    uint8_t val_old, val_new;

    ac107_read(reg, &val_old, client);
    val_new = (val_old & ~mask) | (value & mask);
    if (val_new != val_old) {
        ac107_write(reg, val_new, client);
    }

    return 0;
}

static int ac107_multi_chips_write(uint8_t reg, unsigned char value)
{
    uint8_t i;

    for (i = 0; i < AC107_CHIP_NUMS; i++) {
        ac107_write(reg, value, i2c_ctrl[i]);
    }

    return 0;
}

static int ac107_multi_chips_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t i;

    for (i = 0; i < AC107_CHIP_NUMS; i++) {
        ac107_update_bits(reg, mask, value, i2c_ctrl[i]);
    }

    return 0;
}

/* sys debug */
static ssize_t ac107_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct ac107_priv *ac107 = dev_get_drvdata(dev);
    int scanf_cnt;
    uint32_t reg_val;
    uint32_t input_reg_val = 0;
    uint32_t input_reg_offset = 0;

    scanf_cnt = sscanf(buf, "0x%x 0x%x", &input_reg_offset, &input_reg_val);
    if (scanf_cnt == 0 || scanf_cnt > 2) {
        pr_info("usage read : echo 0x > audio_reg");
        pr_info("usage write: echo 0x 0x > audio_reg");
        return count;
    }

    if (input_reg_offset > AC107_MAX_REG) {
        pr_info("reg offset > audio max reg[0x%x]", AC107_MAX_REG);
        return count;
    }

    if (scanf_cnt == 1) {
        ac107_read((unsigned char)input_reg_offset, (unsigned char *)(&reg_val), ac107->i2c);
        pr_info("reg[0x%03x]: 0x%x", input_reg_offset, reg_val);
        return count;
    } else if (scanf_cnt == 2) {
        ac107_read((unsigned char)input_reg_offset, (unsigned char *)(&reg_val), ac107->i2c);
        pr_info("reg[0x%03x]: 0x%x (old)", input_reg_offset, reg_val);
        ac107_write((unsigned char)input_reg_offset, (unsigned char)input_reg_val, ac107->i2c);
        ac107_read((unsigned char)input_reg_offset, (unsigned char *)(&reg_val), ac107->i2c);
        pr_info("reg[0x%03x]: 0x%x (new)", input_reg_offset, reg_val);
    }

    return count;
}

static ssize_t ac107_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct ac107_priv *ac107 = dev_get_drvdata(dev);
    size_t count = 0, i = 0;
    uint8_t reg_val;
    uint32_t size = ARRAY_SIZE(g_reg_labels);

    while ((i < size) && (g_reg_labels[i].name != NULL)) {
        ac107_read(g_reg_labels[i].address, &reg_val, ac107->i2c);
        pr_info("%-30s [0x%03x]: 0x%8x save_val:0x%x",
                g_reg_labels[i].name,
                g_reg_labels[i].address, reg_val,
                g_reg_labels[i].value);
        i++;
    }

    return count;
}

static DEVICE_ATTR(ac107, 0644, ac107_show, ac107_store);

static struct attribute *ac107_debug_attrs[] = {
    &dev_attr_ac107.attr,
    NULL,
};

static struct attribute_group ac107_debug_attr_group = {
    .name = "ac107_debug",
    .attrs = ac107_debug_attrs,
};

static int ac107_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *i2c_id)
{
    struct ac107_priv *ac107;
    struct device_node *np = i2c->dev.of_node;
    const char *regulator_name = NULL;
    int ret = 0;

    AUDIO_DRIVER_LOG_DEBUG("");

    ac107 = devm_kzalloc(&i2c->dev, sizeof(struct ac107_priv), GFP_KERNEL);
    if (ac107 == NULL) {
        dev_err(&i2c->dev, "Unable to allocate ac107 private data");
        return -ENOMEM;
    }

    ac107->i2c = i2c;
    dev_set_drvdata(&i2c->dev, ac107);

#if AC107_MATCH_DTS_EN
    if (!ac107_regulator_en) {
        ret = of_property_read_string(np, AC107_DVCC_NAME, &regulator_name);    //(const char**)
        if (ret) {
            AUDIO_DRIVER_LOG_ERR("get ac107 DVCC regulator name failed ");
        } else {
            ac107->vol_supply.dvcc_1v8 = regulator_get(NULL, regulator_name);
            if (IS_ERR(ac107->vol_supply.dvcc_1v8) || !ac107->vol_supply.dvcc_1v8) {
                AUDIO_DRIVER_LOG_ERR("get ac107 dvcc_1v8 failed, return!");
                return -EFAULT;
            }
            regulator_set_voltage(ac107->vol_supply.dvcc_1v8, 1800000, 1800000);
            ret = regulator_enable(ac107->vol_supply.dvcc_1v8);
            if (ret != 0) {
                AUDIO_DRIVER_LOG_ERR("some error happen, fail to enable regulator dvcc_1v8!");
            }
            ac107_regulator_en |= 0x1;
        }

        ret = of_property_read_string(np, AC107_AVCC_VCCIO_NAME, &regulator_name);    //(const char**)
        if (ret) {
            AUDIO_DRIVER_LOG_ERR("get ac107 AVCC_VCCIO regulator name failed ");
        } else {
            ac107->vol_supply.avcc_vccio_3v3 = regulator_get(NULL, regulator_name);
            if (IS_ERR(ac107->vol_supply.avcc_vccio_3v3) || !ac107->vol_supply.avcc_vccio_3v3) {
                AUDIO_DRIVER_LOG_ERR("get ac107 avcc_vccio_3v3 failed, return!");
                return -EFAULT;
            }
            regulator_set_voltage(ac107->vol_supply.avcc_vccio_3v3, 3300000, 3300000);
            ret = regulator_enable(ac107->vol_supply.avcc_vccio_3v3);
            if (ret != 0) {
                AUDIO_DRIVER_LOG_ERR("some error happen, fail to enable regulator avcc_vccio_3v3!");
            }
            ac107_regulator_en |= 0x2;
        }

        /*gpio reset enable */
        ac107->reset_gpio = of_get_named_gpio(np,"gpio-reset", 0);
        if (gpio_is_valid(ac107->reset_gpio)) {
            ret = gpio_request(ac107->reset_gpio, "reset gpio");
            if (!ret) {
                gpio_direction_output(ac107->reset_gpio, 1);
                gpio_set_value(ac107->reset_gpio, 1);
                msleep(20);
            } else {
                AUDIO_DRIVER_LOG_ERR("failed request reset gpio: %d!", ac107->reset_gpio);
            }
        }
    }
#endif

    if (i2c_id->driver_data < AC107_CHIP_NUMS) {
        i2c_ctrl[i2c_id->driver_data] = i2c;
        /* change:used alsa */
    } else {
        AUDIO_DRIVER_LOG_ERR("The wrong i2c_id number :%d", (int)(i2c_id->driver_data));
    }

    g_ac107_i2c = i2c;

    ret = sysfs_create_group(&i2c->dev.kobj, &ac107_debug_attr_group);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("failed to create attr group");
    }

    AUDIO_DRIVER_LOG_DEBUG("register ac107-codec codec success");

    return ret;
}

static int ac107_i2c_remove(struct i2c_client *i2c)
{
    /* change:used alsa */

    sysfs_remove_group(&i2c->dev.kobj, &ac107_debug_attr_group);
    return 0;
}

#if !AC107_MATCH_DTS_EN
static int ac107_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    uint8_t ac107_chip_id;
    struct i2c_adapter *adapter = client->adapter;

    ac107_read(CHIP_AUDIO_RST, &ac107_chip_id, client);
    AUDIO_DRIVER_LOG_DEBUG("AC107_Chip_ID on I2C-%d:0x%02X", adapter->nr, ac107_chip_id);

    if (ac107_chip_id == 0x4B) {
        if (client->addr == 0x36) {
            strlcpy(info->type, "ac107_0", I2C_NAME_SIZE);
            return 0;
        } else if (client->addr == 0x37) {
            strlcpy(info->type, "ac107_1", I2C_NAME_SIZE);
            return 0;
        } else if (client->addr == 0x38) {
            strlcpy(info->type, "ac107_2", I2C_NAME_SIZE);
            return 0;
        } else if (client->addr == 0x39) {
            strlcpy(info->type, "ac107_3", I2C_NAME_SIZE);
            return 0;
        }
    }

    return -ENODEV;
}
#endif

static const unsigned short ac107_i2c_addr[] = {
#if AC107_CHIP_NUMS > 0
    0x36,
#endif

#if AC107_CHIP_NUMS > 1
    0x37,
#endif

#if AC107_CHIP_NUMS > 2
    0x38,
#endif

#if AC107_CHIP_NUMS > 3
    0x39,
#endif

#if AC107_CHIP_NUMS > 4
    0x36,
#endif

#if AC107_CHIP_NUMS > 5
    0x37,
#endif

#if AC107_CHIP_NUMS > 6
    0x38,
#endif

#if AC107_CHIP_NUMS > 7
    0x39,
#endif

    I2C_CLIENT_END,
};

static struct i2c_board_info const ac107_i2c_board_info[] = {
#if AC107_CHIP_NUMS > 0
    {I2C_BOARD_INFO("ac107_0", 0x36),},
#endif

#if AC107_CHIP_NUMS > 1
    {I2C_BOARD_INFO("ac107_1", 0x37),},
#endif

#if AC107_CHIP_NUMS > 2
    {I2C_BOARD_INFO("ac107_2", 0x38),},
#endif

#if AC107_CHIP_NUMS > 3
    {I2C_BOARD_INFO("ac107_3", 0x39),},
#endif

#if AC107_CHIP_NUMS > 4
    {I2C_BOARD_INFO("ac107_4", 0x36),},
#endif

#if AC107_CHIP_NUMS > 5
    {I2C_BOARD_INFO("ac107_5", 0x37),},
#endif

#if AC107_CHIP_NUMS > 6
    {I2C_BOARD_INFO("ac107_6", 0x38),},
#endif

#if AC107_CHIP_NUMS > 7
    {I2C_BOARD_INFO("ac107_7", 0x39),},
#endif

};

static const struct i2c_device_id ac107_i2c_id[] = {
#if AC107_CHIP_NUMS > 0
    {"ac107_0", 0},
#endif

#if AC107_CHIP_NUMS > 1
    {"ac107_1", 1},
#endif

#if AC107_CHIP_NUMS > 2
    {"ac107_2", 2},
#endif

#if AC107_CHIP_NUMS > 3
    {"ac107_3", 3},
#endif

#if AC107_CHIP_NUMS > 4
    {"ac107_4", 4},
#endif

#if AC107_CHIP_NUMS > 5
    {"ac107_5", 5},
#endif

#if AC107_CHIP_NUMS > 6
    {"ac107_6", 6},
#endif

#if AC107_CHIP_NUMS > 7
    {"ac107_7", 7},
#endif

    {}
};

MODULE_DEVICE_TABLE(i2c, ac107_i2c_id);

static struct of_device_id ac107_dt_ids[] = {
#if AC107_CHIP_NUMS > 0
    {.compatible = "ac107_0",},
#endif

#if AC107_CHIP_NUMS > 1
    {.compatible = "ac107_1",},
#endif

#if AC107_CHIP_NUMS > 2
    {.compatible = "ac107_2",},
#endif

#if AC107_CHIP_NUMS > 3
    {.compatible = "ac107_3",},
#endif

#if AC107_CHIP_NUMS > 4
    {.compatible = "ac107_4",},
#endif

#if AC107_CHIP_NUMS > 5
    {.compatible = "ac107_5",},
#endif

#if AC107_CHIP_NUMS > 6
    {.compatible = "ac107_6",},
#endif

#if AC107_CHIP_NUMS > 7
    {.compatible = "ac107_7",},
#endif
    {},
};
MODULE_DEVICE_TABLE(of, ac107_dt_ids);

static struct i2c_driver ac107_i2c_driver = {
    .class = I2C_CLASS_HWMON,
    .driver = {
           .name = "ac107",
           .owner = THIS_MODULE,
#if AC107_MATCH_DTS_EN
           .of_match_table = ac107_dt_ids,
#endif
           },
    .probe = ac107_i2c_probe,
    .remove = ac107_i2c_remove,
    .id_table = ac107_i2c_id,
#if !AC107_MATCH_DTS_EN
    .address_list = ac107_i2c_addr,
    .detect = ac107_i2c_detect,
#endif
};

static int __init ac107_init(void)
{
    int ret;
    ret = i2c_add_driver(&ac107_i2c_driver);
    if (ret != 0) {
        AUDIO_DRIVER_LOG_ERR("Failed to register ac107 i2c driver : %d ", ret);
    }

    return ret;
}

module_init(ac107_init);

static void __exit ac107_exit(void)
{
    i2c_del_driver(&ac107_i2c_driver);
}

module_exit(ac107_exit);

MODULE_DESCRIPTION("sunxi soundcard codec of ex-ac107");

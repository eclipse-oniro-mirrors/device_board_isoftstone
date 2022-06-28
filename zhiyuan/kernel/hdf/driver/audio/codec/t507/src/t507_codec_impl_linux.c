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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/regmap.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "t507_codec_impl_linux.h"
#include "audio_control.h"
#include "audio_core.h"
#include "audio_driver_log.h"
#include "audio_platform_base.h"
#include "osal_io.h"
#include <asm/io.h>

#include "t507_codec_impl_linux.h"

#define DRV_NAME    "sunxi-snd-codec"

static struct platform_device *g_codec_pdev;

static struct regmap_config g_codec_regmap_config = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,
    .max_register = SUNXI_AUDIO_MAX_REG,
    .cache_type = REGCACHE_NONE,
};

struct sunxi_codec_mem_info {
    struct resource res;
    void __iomem *membase;
    struct resource *memregion;
    struct regmap *regmap;
};

struct sunxi_codec_clk_info {
    struct clk *clk_pll_audio;
    struct clk *clk_pll_audiox4;
    struct clk *clk_audio;

    struct clk *clk_bus;
    struct reset_control *clk_rst;
};

struct sunxi_codec_rglt_info {
    struct regulator *avcc;
};

struct sunxi_codec_dts_info {
    uint32_t lineout_vol;
};

struct sunxi_codec_info {
    struct platform_device *pdev;

    struct sunxi_codec_mem_info mem_info;
    struct sunxi_codec_clk_info clk_info;
    struct sunxi_codec_rglt_info rglt_info;
    struct sunxi_codec_dts_info dts_info;

    /* uint32_t pa_pin_max; */
    /* struct pa_config *pa_cfg; */
};

#define REG_LABEL(constant) {#constant, constant, 0}
#define REG_LABEL_END       {NULL, 0, 0}
struct reg_label {
    const char *name;
    const uint32_t address;
    uint32_t value;
};
static struct reg_label g_reg_labels[] = {
    REG_LABEL(SUNXI_DAC_DPC),
    REG_LABEL(SUNXI_DAC_FIFO_CTL),
    REG_LABEL(SUNXI_DAC_FIFO_STA),
    REG_LABEL(SUNXI_DAC_CNT),
    REG_LABEL(SUNXI_DAC_DG_REG),
    REG_LABEL(AC_DAC_REG),
    REG_LABEL(AC_MIXER_REG),
    REG_LABEL(AC_RAMP_REG),
    REG_LABEL_END,
};

struct sample_rate {
    uint32_t samplerate;
    uint32_t rate_bit;
};
static const struct sample_rate g_sample_rate_conv[] = {
    {44100, 0},
    {48000, 0},
    {8000, 5},
    {32000, 1},
    {22050, 2},
    {24000, 2},
    {16000, 3},
    {11025, 4},
    {12000, 4},
    {192000, 6},
    {96000, 7},
};

#define KCONTROL_RENDER_VOL     0x00
#define KCONTROL_RENDER_MUTE    0x02

struct KcontrolRender {
    uint32_t vol;
    uint32_t mute;

    uint32_t todo;
};

struct KcontrolRender g_KctrlRender = {
    .vol = 0x1f,
    .mute = false,

    .todo = 0,
};

/*******************************************************************************
 *  for adm api
 ******************************************************************************/
void T507CodecImplRegmapWrite(uint32_t reg, uint32_t val)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    regmap_write(regmap, reg, val);
}

void T507CodecImplRegmapRead(uint32_t reg, uint32_t *val)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    regmap_read(regmap, reg, val);
}

int32_t T507CodecImplGetCtrlOps(const struct AudioKcontrol *kcontrol, struct AudioCtrlElemValue *elemValue)
{
    uint32_t curValue;
    uint32_t rcurValue;
    struct AudioMixerControl *mixerCtrl = NULL;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (kcontrol == NULL || kcontrol->privateValue <= 0 || elemValue == NULL) {
        AUDIO_DRIVER_LOG_ERR("Audio input param is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    mixerCtrl = (struct AudioMixerControl *)((volatile uintptr_t)kcontrol->privateValue);
    if (mixerCtrl == NULL) {
        AUDIO_DRIVER_LOG_ERR("mixerCtrl is NULL.");
        return HDF_FAILURE;
    }

    switch (mixerCtrl->reg) {
        case KCONTROL_RENDER_VOL:
            curValue = g_KctrlRender.vol;
            rcurValue = g_KctrlRender.vol;
            break;
        case KCONTROL_RENDER_MUTE:
            curValue = g_KctrlRender.mute;
            rcurValue = g_KctrlRender.mute;
            break;
        default:
            curValue = g_KctrlRender.todo;
            rcurValue = g_KctrlRender.todo;
            break;
    }

    if (AudioGetCtrlOpsReg(elemValue, mixerCtrl, curValue) != HDF_SUCCESS ||
        AudioGetCtrlOpsRReg(elemValue, mixerCtrl, rcurValue) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Audio codec get kcontrol reg and rreg failed.");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void T507CodecImplUpdateRegBits(const struct AudioMixerControl *mixerControl, uint32_t value)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    if (mixerControl == NULL) {
        AUDIO_DRIVER_LOG_ERR("param mixerControl is null.");
        return;
    }

    AUDIO_DRIVER_LOG_DEBUG("value -> %u", value);

    switch (mixerControl->reg) {
        case KCONTROL_RENDER_VOL:
            g_KctrlRender.vol = value;
            break;
        case KCONTROL_RENDER_MUTE:
            g_KctrlRender.mute = value;
            break;
        default:
            break;
    }

    if (g_KctrlRender.mute) {
        regmap_update_bits(regmap, AC_DAC_REG, 0x1F << LINEOUT_VOL, 0 << LINEOUT_VOL);
    } else {
        regmap_update_bits(regmap, AC_DAC_REG, 0x1F << LINEOUT_VOL, g_KctrlRender.vol << LINEOUT_VOL);
    }
}

int32_t T507CodecImplSetCtrlOps(const struct AudioKcontrol *kcontrol, const struct AudioCtrlElemValue *elemValue)
{
    uint32_t value;
    uint32_t rvalue;
    bool updateRReg = false;
    struct AudioMixerControl *mixerCtrl = NULL;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (kcontrol == NULL || (kcontrol->privateValue <= 0) || elemValue == NULL) {
        AUDIO_DRIVER_LOG_ERR("Audio input param is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    mixerCtrl = (struct AudioMixerControl *)((volatile uintptr_t)kcontrol->privateValue);
    if (AudioSetCtrlOpsReg(kcontrol, elemValue, mixerCtrl, &value) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioSetCtrlOpsReg is failed.");
        return HDF_ERR_INVALID_OBJECT;
    }
    T507CodecImplUpdateRegBits(mixerCtrl, value);

    if (AudioSetCtrlOpsRReg(elemValue, mixerCtrl, &rvalue, &updateRReg) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioSetCtrlOpsRReg is failed.");
        return HDF_ERR_INVALID_OBJECT;
    }
    if (updateRReg) {
        T507CodecImplUpdateRegBits(mixerCtrl, rvalue);
    }

    return HDF_SUCCESS;
}

/* ops api: read default reg value form codec_config.hcs */
int32_t T507CodecImplRegDefaultInit(struct AudioRegCfgGroupNode **regCfgGroup)
{
    int32_t i;
    struct AudioAddrConfig *regAttr = NULL;

    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (regCfgGroup == NULL || regCfgGroup[AUDIO_INIT_GROUP] == NULL ||
        regCfgGroup[AUDIO_INIT_GROUP]->addrCfgItem == NULL || regCfgGroup[AUDIO_INIT_GROUP]->itemNum <= 0) {
        AUDIO_DRIVER_LOG_ERR("input invalid parameter.");
        return HDF_FAILURE;
    }
    regAttr = regCfgGroup[AUDIO_INIT_GROUP]->addrCfgItem;

    for (i = 0; i < regCfgGroup[AUDIO_INIT_GROUP]->itemNum; i++) {
        regmap_write(regmap, regAttr[i].addr, regAttr[i].value);
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t T507CodecImplStartup(enum AudioStreamType streamType)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (streamType == AUDIO_CAPTURE_STREAM) {
        /* audiocodec unsupport capture */
        return HDF_SUCCESS;
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t T507CodecImplHwParams(enum AudioStreamType streamType, enum AudioFormat format, uint32_t channels, uint32_t rate)
{
    int i;
    uint32_t freq_point;
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct sunxi_codec_clk_info *clk_info = &codec_info->clk_info;
    struct regmap *regmap = codec_info->mem_info.regmap;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (streamType == AUDIO_CAPTURE_STREAM) {
        /* audiocodec unsupport capture */
        return HDF_SUCCESS;
    }

    /* set pll clk */
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

    /* moduleclk freq = 49.152/45.1584M, audio clk source = 98.304/90.3168M, own sun50iw9 */
    if (clk_set_rate(clk_info->clk_pll_audiox4, freq_point * 4)) {
        AUDIO_DRIVER_LOG_ERR("clk pllaudio set rate failed");
        return -EINVAL;
    }
    if (clk_set_rate(clk_info->clk_audio, freq_point * 2)) {
        AUDIO_DRIVER_LOG_ERR("clk audio set rate failed");
        return -EINVAL;
    }

    /* set bits */
    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x3 << DAC_FIFO_MODE, 0x3 << DAC_FIFO_MODE);
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x1 << TX_SAMPLE_BITS, 0x0 << TX_SAMPLE_BITS);
            AUDIO_DRIVER_LOG_DEBUG(" format 16");
            break;
        case AUDIO_FORMAT_PCM_24_BIT:
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x3 << DAC_FIFO_MODE, 0x0 << DAC_FIFO_MODE);
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x1 << TX_SAMPLE_BITS, 0x1 << TX_SAMPLE_BITS);
            AUDIO_DRIVER_LOG_DEBUG(" format 24");
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("format: %d is not define.", format);
            return HDF_FAILURE;
    }

    /* set rate */
    i = 0;
    for (i = 0; i < ARRAY_SIZE(g_sample_rate_conv); i++) {
        if (g_sample_rate_conv[i].samplerate == rate)
        break;
    }
    regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x7 << DAC_FS, g_sample_rate_conv[i].rate_bit << DAC_FS);
    AUDIO_DRIVER_LOG_DEBUG(" rate %u", rate);

    /* set channels */
    switch (channels) {
        case 1:
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x1 << DAC_MONO_EN, 0x1 << DAC_MONO_EN);
            break;
        case 2:
            regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x1 << DAC_MONO_EN, 0x0 << DAC_MONO_EN);
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("channel: %d is not define.", channels);
            return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG(" channels %u", channels);

    /* clear fifo */
    regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 0x1 << DAC_FIFO_FLUSH, 0x1 << DAC_FIFO_FLUSH);
    regmap_write(regmap, SUNXI_DAC_FIFO_STA, 1 << DAC_TXE_INT | 1 << DAC_TXU_INT | 1 << DAC_TXO_INT);
    regmap_write(regmap, SUNXI_DAC_CNT, 0);

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static void renderRouteCtrl(bool enable)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (enable) {
        regmap_update_bits(regmap, SUNXI_DAC_DPC, 0x1 << EN_DA, 0x1 << EN_DA);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << DACLEN, 0x1 << DACLEN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << DACREN, 0x1 << DACREN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << LINEOUTL_EN, 0x1 << LINEOUTL_EN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << LINEOUTR_EN, 0x1 << LINEOUTR_EN);
        regmap_update_bits(regmap, AC_RAMP_REG, 0x1 << RDEN, 0x1 << RDEN);
    } else {
        regmap_update_bits(regmap, AC_RAMP_REG, 0x1 << RDEN, 0x0 << RDEN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << LINEOUTL_EN, 0x0 << LINEOUTL_EN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << LINEOUTR_EN, 0x0 << LINEOUTR_EN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << DACLEN, 0x0 << DACLEN);
        regmap_update_bits(regmap, AC_DAC_REG, 0x1 << DACREN, 0x0 << DACREN);
        regmap_update_bits(regmap, SUNXI_DAC_DPC, 0x1 << EN_DA, 0x0 << EN_DA);
    }
}

int32_t T507CodecImplTrigger(enum AudioStreamType streamType, bool enable)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (streamType == AUDIO_CAPTURE_STREAM) {
        /* audiocodec unsupport capture */
        return HDF_SUCCESS;
    }

    renderRouteCtrl(enable);

    if (enable) {
        regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 1 << DAC_DRQ_EN, 1 << DAC_DRQ_EN);
    } else {
        regmap_update_bits(regmap, SUNXI_DAC_FIFO_CTL, 1 << DAC_DRQ_EN, 0 << DAC_DRQ_EN);
    }

    AUDIO_DRIVER_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

/*******************************************************************************
 * for linux probe
*******************************************************************************/
static int snd_sunxi_codec_clk_init(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info);
static void snd_sunxi_codec_clk_exit(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info);
static int snd_sunxi_codec_clk_enable(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info);
static void snd_sunxi_codec_clk_disable(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info);

static int snd_sunxi_codec_rglt_init(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info);
static void snd_sunxi_codec_rglt_exit(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info);
/* static int snd_sunxi_codec_rglt_enable(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info); */
/* static void snd_sunxi_codec_rglt_disable(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info); */

static int snd_sunxi_codec_mem_init(struct platform_device *pdev, struct sunxi_codec_mem_info *mem_info)
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

    mem_info->regmap = devm_regmap_init_mmio(&pdev->dev, mem_info->membase, &g_codec_regmap_config);
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
}

static void snd_sunxi_codec_mem_exit(struct platform_device *pdev, struct sunxi_codec_mem_info *mem_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    devm_iounmap(&pdev->dev, mem_info->membase);
    devm_release_mem_region(&pdev->dev, mem_info->memregion->start, resource_size(mem_info->memregion));
}

static int snd_sunxi_codec_clk_init(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info)
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
    clk_info->clk_bus = of_clk_get_by_name(np, "clk_bus_audio");
    if (IS_ERR_OR_NULL(clk_info->clk_bus)) {
        AUDIO_DRIVER_LOG_ERR("clk bus get failed\n");
        ret = PTR_ERR(clk_info->clk_bus);
        goto err_get_clk_bus;
    }

    /* get parent clk */
    clk_info->clk_pll_audio = of_clk_get_by_name(np, "clk_pll_audio");
    if (IS_ERR_OR_NULL(clk_info->clk_pll_audio)) {
        AUDIO_DRIVER_LOG_ERR("clk pll audio get failed");
        ret = PTR_ERR(clk_info->clk_pll_audio);
        goto err_get_clk_pll_audio;
    }

    clk_info->clk_pll_audiox4 = of_clk_get_by_name(np, "clk_pll_audio_4x");
    if (IS_ERR_OR_NULL(clk_info->clk_pll_audiox4)) {
        AUDIO_DRIVER_LOG_ERR("clk pll audio4x get failed");
        ret = PTR_ERR(clk_info->clk_pll_audiox4);
        goto err_get_clk_pll_audiox4;
    }

    /* get module clk */
    clk_info->clk_audio = of_clk_get_by_name(np, "clk_audio");
    if (IS_ERR_OR_NULL(clk_info->clk_audio)) {
        AUDIO_DRIVER_LOG_ERR("clk audio get failed");
        ret = PTR_ERR(clk_info->clk_audio);
        goto err_get_clk_audio;
    }

    /* set clk audio parent of pllaudio */
    if (clk_set_parent(clk_info->clk_audio, clk_info->clk_pll_audiox4)) {
        AUDIO_DRIVER_LOG_ERR("set parent clk audio failed");
        ret = -EINVAL;
        goto err_set_parent;
    }

    ret = snd_sunxi_codec_clk_enable(pdev, clk_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("clk enable failed");
        ret = -EINVAL;
        goto err_clk_enable;
    }

    return 0;

err_clk_enable:
err_set_parent:
    clk_put(clk_info->clk_audio);
err_get_clk_audio:
    clk_put(clk_info->clk_pll_audiox4);
err_get_clk_pll_audiox4:
    clk_put(clk_info->clk_pll_audio);
err_get_clk_pll_audio:
    clk_put(clk_info->clk_bus);
err_get_clk_bus:
err_get_clk_rst:
    return ret;
}

static void snd_sunxi_codec_clk_exit(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    snd_sunxi_codec_clk_disable(pdev, clk_info);
    clk_put(clk_info->clk_audio);
    clk_put(clk_info->clk_pll_audiox4);
    clk_put(clk_info->clk_pll_audio);
    clk_put(clk_info->clk_bus);
}

static int snd_sunxi_codec_clk_enable(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info)
{
    int ret = 0;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (reset_control_deassert(clk_info->clk_rst)) {
        AUDIO_DRIVER_LOG_ERR("clk rst deassert failed\n");
        ret = -EINVAL;
        goto err_deassert_rst;
    }

    if (clk_prepare_enable(clk_info->clk_bus)) {
        AUDIO_DRIVER_LOG_ERR("clk bus enable failed\n");
        goto err_enable_clk_bus;
    }

    if (clk_prepare_enable(clk_info->clk_pll_audio)) {
        AUDIO_DRIVER_LOG_ERR("pllaudio enable failed");
        goto err_enable_clk_pll_audio;
    }

    if (clk_prepare_enable(clk_info->clk_pll_audiox4)) {
        AUDIO_DRIVER_LOG_ERR("pllaudiox4 enable failed");
        goto err_enable_clk_pll_audiox4;
    }

    if (clk_prepare_enable(clk_info->clk_audio)) {
        AUDIO_DRIVER_LOG_ERR("dacclk enable failed");
        goto err_enable_clk_audio;
    }

    return 0;

err_enable_clk_audio:
    clk_disable_unprepare(clk_info->clk_pll_audiox4);
err_enable_clk_pll_audiox4:
    clk_disable_unprepare(clk_info->clk_pll_audio);
err_enable_clk_pll_audio:
    clk_disable_unprepare(clk_info->clk_bus);
err_enable_clk_bus:
    reset_control_assert(clk_info->clk_rst);
err_deassert_rst:
    return ret;
}

static void snd_sunxi_codec_clk_disable(struct platform_device *pdev, struct sunxi_codec_clk_info *clk_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    clk_disable_unprepare(clk_info->clk_audio);
    clk_disable_unprepare(clk_info->clk_pll_audiox4);
    clk_disable_unprepare(clk_info->clk_pll_audio);
    clk_disable_unprepare(clk_info->clk_bus);
    reset_control_assert(clk_info->clk_rst);
}

static int snd_sunxi_codec_rglt_init(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info)
{
    int ret = 0;

    AUDIO_DRIVER_LOG_DEBUG("");

    rglt_info->avcc = regulator_get(&pdev->dev, "avcc");
    if (IS_ERR_OR_NULL(rglt_info->avcc)) {
        AUDIO_DRIVER_LOG_ERR("get avcc failed");
        ret = -EFAULT;
        goto err_regulator_get_avcc;
    }
    ret = regulator_set_voltage(rglt_info->avcc, 1800000, 1800000);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("set avcc voltage failed");
        ret = -EFAULT;
        goto err_regulator_set_vol_avcc;
    }
    ret = regulator_enable(rglt_info->avcc);
    if (ret < 0) {
        AUDIO_DRIVER_LOG_ERR("enable avcc failed");
        ret = -EFAULT;
        goto err_regulator_enable_avcc;
    }

    return 0;

err_regulator_enable_avcc:
err_regulator_set_vol_avcc:
    if (rglt_info->avcc) {
        regulator_put(rglt_info->avcc);
    }
err_regulator_get_avcc:
    return ret;
}

static void snd_sunxi_codec_rglt_exit(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (rglt_info->avcc) {
        if (!IS_ERR_OR_NULL(rglt_info->avcc)) {
            regulator_disable(rglt_info->avcc);
            regulator_put(rglt_info->avcc);
        }
    }
}

#if 0
static int snd_sunxi_codec_rglt_enable(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info)
{
    int ret;

    AUDIO_DRIVER_LOG_DEBUG("");

    if (rglt_info->avcc)
        if (!IS_ERR_OR_NULL(rglt_info->avcc)) {
            ret = regulator_enable(rglt_info->avcc);
            if (ret) {
                AUDIO_DRIVER_LOG_ERR("enable avcc failed");
                return -1;
            }
    }

    return 0;
}

static void snd_sunxi_codec_rglt_disable(struct platform_device *pdev, struct sunxi_codec_rglt_info *rglt_info)
{
    AUDIO_DRIVER_LOG_DEBUG("");

    if (rglt_info->avcc) {
        if (!IS_ERR_OR_NULL(rglt_info->avcc)) {
            regulator_disable(rglt_info->avcc);
        }
    }
}
#endif

static void snd_sunxi_dts_params_init(struct platform_device *pdev, struct sunxi_codec_dts_info *dts_info)
{
    int ret = 0;
    uint32_t temp_val;
    struct device_node *np = pdev->dev.of_node;

    AUDIO_DRIVER_LOG_DEBUG("");

    /* lineout volume */
    ret = of_property_read_u32(np, "lineout-vol", &temp_val);
    if (ret < 0) {
        dts_info->lineout_vol = 0;
    } else {
        dts_info->lineout_vol = temp_val;
    }

    AUDIO_DRIVER_LOG_DEBUG("lineout vol -> %u\n", dts_info->lineout_vol);
}

/* sysfs debug */
static ssize_t snd_sunxi_debug_show_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;
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
    struct sunxi_codec_info *codec_info = dev_get_drvdata(&g_codec_pdev->dev);
    struct regmap *regmap = codec_info->mem_info.regmap;
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

    if (input_reg_offset > SUNXI_AUDIO_MAX_REG) {
        pr_info("reg offset > audio max reg[0x%x]\n", SUNXI_AUDIO_MAX_REG);
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

static int sunxi_internal_codec_dev_probe(struct platform_device *pdev)
{
    int ret;
    struct device *dev = &pdev->dev;
    struct device_node *np = pdev->dev.of_node;
    struct sunxi_codec_info *codec_info;
    struct sunxi_codec_mem_info *mem_info;
    struct sunxi_codec_clk_info *clk_info;
    struct sunxi_codec_rglt_info *rglt_info;
    struct sunxi_codec_dts_info *dts_info;

    AUDIO_DRIVER_LOG_DEBUG("");

    /* sunxi codec info */
    codec_info = devm_kzalloc(dev, sizeof(struct sunxi_codec_info), GFP_KERNEL);
    if (!codec_info) {
        AUDIO_DRIVER_LOG_ERR("can't allocate sunxi codec memory");
        ret = -ENOMEM;
        goto err_devm_kzalloc;
    }
    dev_set_drvdata(dev, codec_info);
    mem_info = &codec_info->mem_info;
    clk_info = &codec_info->clk_info;
    rglt_info = &codec_info->rglt_info;
    dts_info = &codec_info->dts_info;

    /* memio init */
    ret = snd_sunxi_codec_mem_init(pdev, mem_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("mem init failed");
        ret = -ENOMEM;
        goto err_mem_init;
    }

    /* clk init */
    ret = snd_sunxi_codec_clk_init(pdev, clk_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("clk init failed");
        ret = -ENOMEM;
        goto err_clk_init;
    }

    /* regulator init */
    ret = snd_sunxi_codec_rglt_init(pdev, rglt_info);
    if (ret) {
        AUDIO_DRIVER_LOG_ERR("regulator init failed");
        ret = -ENOMEM;
        goto err_regulator_init;
    }

    /* dts_params init */
    snd_sunxi_dts_params_init(pdev, dts_info);

    ret = sysfs_create_group(&pdev->dev.kobj, &debug_attr);
    if (ret)
        AUDIO_DRIVER_LOG_ERR("sysfs debug create failed");

    g_codec_pdev = pdev;

    AUDIO_DRIVER_LOG_DEBUG("register internal-codec codec success");

    return 0;

    snd_sunxi_codec_rglt_exit(pdev, rglt_info);
err_regulator_init:
    snd_sunxi_codec_clk_exit(pdev, clk_info);
err_clk_init:
    snd_sunxi_codec_mem_exit(pdev, mem_info);
err_mem_init:
    devm_kfree(dev, codec_info);
err_devm_kzalloc:
    of_node_put(np);

    return ret;
}

static int sunxi_internal_codec_dev_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct sunxi_codec_info *codec_info = dev_get_drvdata(dev);
    struct sunxi_codec_mem_info *mem_info = &codec_info->mem_info;
    struct sunxi_codec_clk_info *clk_info = &codec_info->clk_info;
    struct sunxi_codec_rglt_info *rglt_info = &codec_info->rglt_info;

    AUDIO_DRIVER_LOG_DEBUG("");

    sysfs_remove_group(&pdev->dev.kobj, &debug_attr);

    snd_sunxi_codec_mem_exit(pdev, mem_info);
    snd_sunxi_codec_clk_exit(pdev, clk_info);
    snd_sunxi_codec_rglt_exit(pdev, rglt_info);

    /* sunxi codec custom info free */
    devm_kfree(dev, codec_info);
    of_node_put(pdev->dev.of_node);

    AUDIO_DRIVER_LOG_DEBUG("unregister internal-codec codec success");

    return 0;
}

static const struct of_device_id sunxi_internal_codec_of_match[] = {
    { .compatible = "allwinner," DRV_NAME, },
    {},
};
MODULE_DEVICE_TABLE(of, sunxi_internal_codec_of_match);

static struct platform_driver sunxi_internal_codec_driver = {
    .driver = {
        .name   = DRV_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = sunxi_internal_codec_of_match,
    },
    .probe  = sunxi_internal_codec_dev_probe,
    .remove = sunxi_internal_codec_dev_remove,
};

module_platform_driver(sunxi_internal_codec_driver);

MODULE_DESCRIPTION("sunxi soundcard codec of internal-codec");

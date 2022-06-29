/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef T507_CODEC_IMPL_LINUX_H
#define T507_CODEC_IMPL_LINUX_H

#include "audio_codec_if.h"
#include "securec.h"
#include "audio_control.h"
#include "audio_parse.h"
#include "osal_mem.h"
#include "osal_time.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define SUNXI_DAC_DPC       0x00
#define SUNXI_DAC_FIFO_CTL  0x10
#define SUNXI_DAC_FIFO_STA  0x14

/* left blank */
#define SUNXI_DAC_TXDATA    0X20
#define SUNXI_DAC_CNT       0x24
#define SUNXI_DAC_DG_REG    0x28

#define SUNXI_ADC_FIFO_CTL  0x30
#define SUNXI_ADC_FIFO_STA  0x34
#define SUNXI_ADC_RXDATA    0x40
#define SUNXI_ADC_CNT       0x44
#define SUNXI_ADC_DG_REG    0x4C

/*left blank */
#define SUNXI_DAC_DAP_CTL   0xf0
#define SUNXI_ADC_DAP_CTL   0xf8

/* DAC */
#define AC_DAC_REG          0x310
#define AC_MIXER_REG        0x314
#define AC_RAMP_REG         0x31c
#define SUNXI_AUDIO_MAX_REG AC_RAMP_REG

/* SUNXI_DAC_DPC:0x00 */
#define EN_DA               31
#define MODQU               25
#define DWA_EN              24
#define HPF_EN              18
#define DVOL                12
#define DAC_HUB_EN          0

/* SUNXI_DAC_FIFO_CTL:0x10 */
#define DAC_FS              29
#define FIR_VER             28
#define SEND_LASAT          26
#define DAC_FIFO_MODE       24
#define DAC_DRQ_CLR_CNT     21
#define TX_TRIG_LEVEL       8
#define DAC_MONO_EN         6
#define TX_SAMPLE_BITS      5
#define DAC_DRQ_EN          4
#define DAC_IRQ_EN          3
#define DAC_FIFO_UNDERRUN_IRQ_EN    2
#define DAC_FIFO_OVERRUN_IRQ_EN     1
#define DAC_FIFO_FLUSH      0

/* SUNXI_DAC_FIFO_STA:0x14 */
#define DAC_TX_EMPTY        23
#define DAC_TXE_CNT         8
#define DAC_TXE_INT         3
#define DAC_TXU_INT         2
#define DAC_TXO_INT         1

/* SUNXI_DAC_DG_REG:0x28 */
#define DAC_MODU_SELECT     11
#define DAC_PATTERN_SEL     9
#define CODEC_CLK_SEL       8
#define DA_SWP              6
#define ADDA_LOOP_MODE      0

/* SUNXI_ADC_FIFO_CTL:0x30 */
#define ADC_FS              29
#define EN_AD               28
#define ADCFDT              26
#define ADCDFEN             25
#define RX_FIFO_MODE        24
#define RX_SAMPLE_BITS      16
#define ADCY_EN             15
#define ADCX_EN             14
#define ADCR_EN             13
#define ADCL_EN             12
#define ADC_CHAN_SEL        12
#define RX_FIFO_TRG_LEVEL   4
#define ADC_DRQ_EN          3
#define ADC_IRQ_EN          2
#define ADC_OVERRUN_IRQ_EN  1
#define ADC_FIFO_FLUSH      0

/* SUNXI_ADC_FIFO_STA:0x38 */
#define ADC_RXA             23
#define ADC_RXA_CNT         8
#define ADC_RXA_INT         3
#define ADC_RXO_INT         1

/* SUNXI_ADC_DG_REG:0x4c */
#define ADXY_SWP            25
#define ADLR_SWP            24

/* DAC */
/* AC_DAC_REG */
#define CURRENT_TEST_SEL    23
#define IOPVRS              20
#define ILINEOUTAMPS        18
#define IOPDACS             16
#define DACLEN              15
#define DACREN              14
#define LINEOUTL_EN         13
#define LMUTE               12
#define LINEOUTR_EN         11
#define RMUTE               10
#define RSWITCH             9
#define RAMPEN              8
#define LINEOUTL_SEL        6
#define LINEOUTR_SEL        5
#define LINEOUT_VOL         0

/* AC_MIXER_REG    */
#define LMIX_LDAC           21
#define LMIX_RDAC           20
#define LMIXMUTE            20
#define RMIX_RDAC           17
#define RMIX_LDAC           16
#define RMIXMUTE            16
#define LMIXEN              11
#define RMIXEN              10
#define IOPMIXS             8

/* AC_RAMP_REG */
#define RAMP_STEP           4
#define RMDEN               3
#define RMUEN               2
#define RMCEN               1
#define RDEN                0

void T507CodecImplRegmapWrite(uint32_t reg, uint32_t val);
void T507CodecImplRegmapRead(uint32_t reg, uint32_t *val);

int32_t T507CodecImplGetCtrlOps(const struct AudioKcontrol *kcontrol, struct AudioCtrlElemValue *elemValue);
int32_t T507CodecImplSetCtrlOps(const struct AudioKcontrol *kcontrol, const struct AudioCtrlElemValue *elemValue);

int32_t T507CodecImplStartup(enum AudioStreamType streamType);
int32_t T507CodecImplHwParams(enum AudioStreamType streamType, enum AudioFormat format, uint32_t channels, uint32_t rate);
int32_t T507CodecImplTrigger(enum AudioStreamType streamType, bool enable);

int32_t T507CodecImplRegDefaultInit(struct AudioRegCfgGroupNode **regCfgGroup);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* T507_CODEC_IMPL_LINUX_H */

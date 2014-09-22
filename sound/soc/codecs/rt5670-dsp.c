/*
 * rt5670-dsp.c  --  RT5670 ALSA SoC DSP driver
 *
 * Copyright 2014 Realtek Semiconductor Corp.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "rt5670.h"
#include "rt5670-dsp.h"

#define DSP_CLK_RATE RT5670_DSP_CLK_96K

static const struct firmware *rt5670_dsp_fw;

/**
 * rt5670_dsp_done - Wait until DSP is ready.
 * @codec: SoC Audio Codec device.
 *
 * To check voice DSP status and confirm it's ready for next work.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_done(struct snd_soc_codec *codec)
{
	unsigned int count = 0, dsp_val;

	dsp_val = snd_soc_read(codec, RT5670_DSP_CTRL1);
	while (dsp_val & RT5670_DSP_BUSY_MASK) {
		if (count > 10)
			return -EBUSY;
		dsp_val = snd_soc_read(codec, RT5670_DSP_CTRL1);
		count++;
	}

	return 0;
}

/**
 * rt5670_dsp_write - Write DSP register.
 * @codec: SoC audio codec device.
 * @param: DSP parameters.
  *
 * Modify voice DSP register for sound effect. The DSP can be controlled
 * through DSP addr (0xe1), data (0xe2) and cmd (0xe0)
 * registers. It has to wait until the DSP is ready.
 *
 * Returns 0 for success or negative error code.
 */
int rt5670_dsp_write(struct snd_soc_codec *codec,
		unsigned int addr, unsigned int data)
{
	unsigned int dsp_val;
	int ret;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, addr);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5670_DSP_CTRL3, data);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP data reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_I2C_AL_16 | RT5670_DSP_DL_2 |
		RT5670_DSP_CMD_MW | DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}
	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;
}

/**
 * rt5670_dsp_read - Read DSP register.
 * @codec: SoC audio codec device.
 * @reg: DSP register index.
 *
 * Read DSP setting value from voice DSP. The DSP can be controlled
 * through DSP addr (0xe1), data (0xe2) and cmd (0xe0) registers. Each
 * command has to wait until the DSP is ready.
 *
 * Returns DSP register value or negative error code.
 */
unsigned int rt5670_dsp_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int value;
	unsigned int dsp_val;
	int ret = 0;

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_I2C_AL_16 | RT5670_DSP_DL_0 | RT5670_DSP_RW_MASK |
		RT5670_DSP_CMD_MR | DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, 0x26);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5670_DSP_DL_1 | RT5670_DSP_CMD_RR | RT5670_DSP_RW_MASK |
		DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5670_DSP_CTRL2, 0x25);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}

	dsp_val = RT5670_DSP_DL_1 | RT5670_DSP_CMD_RR | RT5670_DSP_RW_MASK |
		DSP_CLK_RATE | RT5670_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5670_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5670_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_read(codec, RT5670_DSP_CTRL5);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read DSP data reg: %d\n", ret);
		goto err;
	}

	value = ret;
	return value;

err:
	return ret;
}

static int rt5670_dsp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5670->dsp_sw;

	return 0;
}

static int rt5670_dsp_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	if (rt5670->dsp_sw != ucontrol->value.integer.value[0])
		rt5670->dsp_sw = ucontrol->value.integer.value[0];

	return 0;
}

/* DSP SRC Control */
static const char * const rt5670_src_rxdp_mode[] = {
	"Normal", "Divided by 2", "Divided by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_src_rxdp_enum, RT5670_DSP_PATH1,
	RT5670_RXDP_SRC_SFT, rt5670_src_rxdp_mode);

static const char * const rt5670_src_txdp_mode[] = {
	"Normal", "Multiplied by 2", "Multiplied by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5670_src_txdp_enum, RT5670_DSP_PATH1,
	RT5670_TXDP_SRC_SFT, rt5670_src_txdp_mode);

/* DSP Mode */
static const char * const rt5670_dsp_mode[] = {
	"Mode 1", "Mode 2", "Mode 3", "Mode 4", "Mode 5"
};

static const SOC_ENUM_SINGLE_DECL(rt5670_dsp_enum, 0, 0,
	rt5670_dsp_mode);

static const struct snd_kcontrol_new rt5670_dsp_snd_controls[] = {
	SOC_ENUM("RxDP SRC Switch", rt5670_src_rxdp_enum),
	SOC_ENUM("TxDP SRC Switch", rt5670_src_txdp_enum),
	SOC_ENUM_EXT("DSP Function Switch", rt5670_dsp_enum,
		rt5670_dsp_mode_get, rt5670_dsp_mode_put),
};

/**
 * rt5670_dsp_set_mode - Set DSP mode parameters.
 *
 * @codec: SoC audio codec device.
 * @mode: DSP mode.
 *
 * Set parameters of mode to DSP.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5670_dsp_set_mode(struct snd_soc_codec *codec, int mode)
{
	int tab_num, n, ret = -EINVAL;
	unsigned int pos = 0;

	if (rt5670_dsp_fw) {
		n = rt5670_dsp_fw->data[0];

		if (mode <= n) {
			pos = rt5670_dsp_fw->data[mode * 3 + 2] |
				rt5670_dsp_fw->data[mode * 3 + 1] << 8;

			tab_num = rt5670_dsp_fw->data[mode * 3 + 3];
			if (pos + tab_num * 5 > rt5670_dsp_fw->size)
				return -EINVAL;
			ret = rt5670_write_fw(codec,
				rt5670_dsp_fw, pos, tab_num);
			if (ret < 0)
				dev_err(codec->dev,
					"Fail to set mode %d parameters: %d\n",
					mode, ret);
		}
	}

	return ret;
}

static int rt5670_dsp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5670_priv *rt5670 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		rt5670_dsp_write(codec, 0x22f9, 1);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5670_DIG_MISC,
			RT5670_RST_DSP, RT5670_RST_DSP);
		snd_soc_update_bits(codec, RT5670_DIG_MISC,
			RT5670_RST_DSP, 0);
		mdelay(10);
		rt5670_dsp_set_mode(codec, rt5670->dsp_sw);
		break;

	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt5670_dsp_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("Voice DSP", 1, SND_SOC_NOPM,
		0, 0, rt5670_dsp_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DSP Downstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP Upstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route rt5670_dsp_dapm_routes[] = {
	{"DSP Downstream", NULL, "Voice DSP"},
	{"DSP Downstream", NULL, "RxDP Mux"},
	{"DSP Upstream", NULL, "Voice DSP"},
	{"DSP Upstream", NULL, "TDM Data Mux"},
	{"DSP DL Mux", "DSP", "DSP Downstream"},
	{"DSP UL Mux", "DSP", "DSP Upstream"},
};

static void rt5670_dsp_fw_loaded(const struct firmware *fw, void *context)
{
	if (fw) {
		rt5670_dsp_fw = fw;
		pr_debug("fw->size=%d\n", fw->size);
	}

}

/**
 * rt5670_dsp_probe - register DSP for rt5670
 * @codec: audio codec
 *
 * To register DSP function for rt5670.
 *
 * Returns 0 for success or negative error code.
 */
int rt5670_dsp_probe(struct snd_soc_codec *codec)
{
	if (codec == NULL)
		return -EINVAL;

	snd_soc_update_bits(codec, RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP, RT5670_PWR_I2S_DSP);

	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP,
		RT5670_RST_DSP);
	snd_soc_update_bits(codec, RT5670_DIG_MISC, RT5670_RST_DSP, 0);

	mdelay(10);

	rt5670_dsp_set_mode(codec, 0);
	/* power down DSP */
	mdelay(15);
	rt5670_dsp_write(codec, 0x22f9, 1);

	snd_soc_update_bits(codec, RT5670_PWR_DIG2,
		RT5670_PWR_I2S_DSP, 0);

	snd_soc_add_codec_controls(codec, rt5670_dsp_snd_controls,
			ARRAY_SIZE(rt5670_dsp_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5670_dsp_dapm_widgets,
			ARRAY_SIZE(rt5670_dsp_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5670_dsp_dapm_routes,
			ARRAY_SIZE(rt5670_dsp_dapm_routes));

	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				"rt567x_dsp.bin", codec->dev, GFP_KERNEL,
				codec, rt5670_dsp_fw_loaded);

	return 0;
}

/*
 * qss_wm8994.c
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>

#include <asm/mach-types.h>

#include "../codecs/wm8994.h"

static struct snd_soc_card qss;
static struct platform_device *qss_snd_device;

/* 3.5 pie jack */
static struct snd_soc_jack jack;

/* 3.5 pie jack detection DAPM pins */
static struct snd_soc_jack_pin jack_pins[] = {
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	}, {
		.pin = "Headset Stereophone",
		.mask = SND_JACK_HEADPHONE,
	},
};

static const struct snd_soc_dapm_widget qss_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Back Spk", NULL),
	SND_SOC_DAPM_SPK("Front Earpiece", NULL),
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("2nd Mic", NULL),
};

static const struct snd_soc_dapm_route qss_dapm_routes[] = {
	{"Back Spk", NULL, "SPKOUTLP"},
	{"Back Spk", NULL, "SPKOUTLN"},

	{"Front Earpiece", NULL, "HPOUT2N"},
	{"Front Earpiece", NULL, "HPOUT2P"},

	{"Headset Stereophone", NULL, "HPOUT1L"},
	{"Headset Stereophone", NULL, "HPOUT1R"},

	{"IN1RN", NULL, "Headset Mic"},
	{"IN1RP", NULL, "Headset Mic"},

	{"IN1LN", NULL, "Main Mic"},
	{"IN1LP", NULL, "Main Mic"},

        {"IN2LN", NULL, "2nd Mic"},
};

static int qss_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	ret = snd_soc_dapm_new_controls(dapm, qss_dapm_widgets,
				ARRAY_SIZE(qss_dapm_widgets));
	if (ret)
		return ret;

	snd_soc_dapm_add_routes(dapm, qss_dapm_routes,
				ARRAY_SIZE(qss_dapm_routes));

	snd_soc_dapm_enable_pin(dapm, "Back Spk");
	snd_soc_dapm_enable_pin(dapm, "Front Earpiece");
	snd_soc_dapm_enable_pin(dapm, "Main Mic");
        snd_soc_dapm_enable_pin(dapm, "2nd Mic");

	/* Other pins NC */
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRN");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2P");
	snd_soc_dapm_nc_pin(dapm, "IN2RN");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");

	ret = snd_soc_dapm_sync(dapm);
	if (ret)
		return ret;

	/* Headset jack detection */
	ret = snd_soc_jack_new(codec, "Headset Jack",
			SND_JACK_HEADSET,
			&jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_pins(&jack, ARRAY_SIZE(jack_pins), jack_pins);
	if (ret)
		return ret;

	snd_soc_jack_report(&jack, SND_JACK_HEADSET, SND_JACK_HEADSET);

	return 0;
}

#define QSS_WM8994_FREQ 24000000

static int qss_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out;
	int ret = 0;

	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
		pll_out = params_rate(params) * 384;
	else if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	/* set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set the codec FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
				  QSS_WM8994_FREQ, pll_out);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
				     pll_out, SND_SOC_CLOCK_IN);

	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops qss_hifi_ops = {
	.hw_params = qss_hifi_hw_params,
};

static int qss_modem_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret = 0;

	if (params_rate(params) != 8000)
		return -EINVAL;

	pll_out = params_rate(params) * 256;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_IB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the codec FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2, WM8994_FLL_SRC_MCLK2,
				  QSS_WM8994_FREQ, pll_out);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops qss_modem_ops = {
	.hw_params = qss_modem_hw_params,
};

static struct snd_soc_dai_driver dai[] = {
	{
		.name = "qss-modem-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link qss_dai[] = {
	{
		.name = "WM8994",
		.stream_name = "WM8994 HiFi",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.init = qss_wm8994_init,
		.ops = &qss_hifi_ops,
	}, {
		.name = "WM8994 Modem",
		.stream_name = "Modem",
		.cpu_dai_name = "qss-modem-dai",
		.codec_dai_name = "wm8994-aif2",
		.codec_name = "wm8994-codec",
		.ops = &qss_modem_ops,
	},
};

static struct snd_soc_card qss = {
	.name = "qss",
	.owner = THIS_MODULE,
	.dai_link = qss_dai,
	.num_links = ARRAY_SIZE(qss_dai),
};

static int __init qss_init(void)
{
	int ret;

	if (!machine_is_qss())
		return -ENODEV;

	qss_snd_device = platform_device_alloc("soc-audio", -1);
	if (!qss_snd_device)
		return -ENOMEM;

	ret = snd_soc_register_dais(&qss_snd_device->dev, dai, ARRAY_SIZE(dai));
	if (ret)
		goto err;

	platform_set_drvdata(qss_snd_device, &qss);

	ret = platform_device_add(qss_snd_device);
	if (ret)
		goto err;

	return 0;

err:
	platform_device_put(qss_snd_device);
	return ret;
}

static void __exit qss_exit(void)
{
	platform_device_unregister(qss_snd_device);
}

module_init(qss_init);
module_exit(qss_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC WM8994 QSS(S5PV210)");
MODULE_LICENSE("GPL");

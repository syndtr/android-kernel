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

#include <asm/mach-types.h>
#include <mach/gpio.h>

#include "../codecs/wm8994.h"

#define MACHINE_NAME	0

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
		.mask = SND_JACK_HEADPHONE | SND_JACK_MECHANICAL |
			SND_JACK_AVOUT,
	},
};

/* 3.5 pie jack detection gpios */
static struct snd_soc_jack_gpio jack_gpios[] = {
	{
		.gpio = S5PV210_GPH0(6),
		.name = "DET_3.5",
		.report = SND_JACK_HEADSET | SND_JACK_MECHANICAL |
			SND_JACK_AVOUT,
		.debounce_time = 200,
	},
};

static const struct snd_soc_dapm_widget qss_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Left Spk", NULL),
	SND_SOC_DAPM_SPK("Ext Right Spk", NULL),
	SND_SOC_DAPM_SPK("Ext Rcv", NULL),
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("2nd Mic", NULL),
	SND_SOC_DAPM_LINE("Radio In", NULL),
};

static const struct snd_soc_dapm_route qss_dapm_routes[] = {
	{"Ext Left Spk", NULL, "SPKOUTLP"},
	{"Ext Left Spk", NULL, "SPKOUTLN"},

	{"Ext Right Spk", NULL, "SPKOUTRP"},
	{"Ext Right Spk", NULL, "SPKOUTRN"},

	{"Ext Rcv", NULL, "HPOUT2N"},
	{"Ext Rcv", NULL, "HPOUT2P"},

	{"Headset Stereophone", NULL, "HPOUT1L"},
	{"Headset Stereophone", NULL, "HPOUT1R"},

	{"IN1RN", NULL, "Headset Mic"},
	{"IN1RP", NULL, "Headset Mic"},

	{"IN1RN", NULL, "2nd Mic"},
	{"IN1RP", NULL, "2nd Mic"},

	{"IN1LN", NULL, "Main Mic"},
	{"IN1LP", NULL, "Main Mic"},

	{"IN2LN", NULL, "Radio In"},
	{"IN2RN", NULL, "Radio In"},
};

static int qss_wm8994_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	/* set endpoints to not connected */
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2P");

	/* Headset jack detection */
	ret = snd_soc_jack_new(codec, "Headset Jack",
			SND_JACK_HEADSET | SND_JACK_MECHANICAL | SND_JACK_AVOUT,
			&jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_pins(&jack, ARRAY_SIZE(jack_pins), jack_pins);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_gpios(&jack, ARRAY_SIZE(jack_gpios), jack_gpios);
	if (ret)
		return ret;

	return 0;
}

static int qss_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out = 24000000;
	int ret = 0;

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
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1, pll_out,
			params_rate(params) * 256);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
			params_rate(params) * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops qss_hifi_ops = {
	.hw_params = qss_hifi_hw_params,
};

static struct snd_soc_dai_link qss_dai[] = {
	{
		.name = "WM8994",
		.stream_name = "WM8994 HiFi",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-idma",
		.codec_name = "wm8994-codec",
		.init = qss_wm8994_init,
		.ops = &qss_hifi_ops,
	}
};

static struct snd_soc_card qss = {
	.name = "qss",
	.owner = THIS_MODULE,
	.dai_link = qss_dai,
	.num_links = ARRAY_SIZE(qss_dai),

	.dapm_widgets = qss_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(qss_dapm_widgets),
	.dapm_routes = qss_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(qss_dapm_routes),
};

static int __init qss_init(void)
{
	int ret;

	if (!machine_is_qss())
		return -ENODEV;

	qss_snd_device = platform_device_alloc("soc-audio", -1);
	if (!qss_snd_device)
		return -ENOMEM;

	platform_set_drvdata(qss_snd_device, &qss);
	ret = platform_device_add(qss_snd_device);

	if (ret) {
		platform_device_put(qss_snd_device);
	}

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

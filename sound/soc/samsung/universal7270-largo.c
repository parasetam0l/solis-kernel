/*
 * universal7270-largo Audio Machine driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>
#include <linux/input.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/exynos-audmixer.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/largo.h"

#define CODEC_BFS_48KHZ		32
#define CODEC_RFS_48KHZ		512
#define CODEC_SAMPLE_RATE_48KHZ	48000

#define CODEC_BFS_192KHZ		64
#define CODEC_RFS_192KHZ		128
#define CODEC_SAMPLE_RATE_192KHZ	192000

#define MCLK1_RATE	26000000
#define MCLK2_RATE	32768
#define SYSCLK_RATE	147456000

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

static struct clk *codec_mclk1;
static struct mutex codec_mclk1_mutex;
atomic_t codec_mclk1_enabled;
static int codec_mclk_used;

enum {
	CODEC_MCLK1 = 0,
	CODEC_MCLK2,
};

static const char *const codec_mclk_text[] = {
	"MCLK1", "MCLK2",
};

static const struct soc_enum codec_mclk_enum =
	SOC_ENUM_SINGLE_EXT(2, codec_mclk_text);


static struct input_dev *voice_input;

static struct snd_soc_card universal7270_largo_card;

static const struct snd_soc_component_driver universal7270_cmpnt = {
	.name = "universal7270-audio",
};

static int universal7270_ap_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
	struct snd_soc_dai *codec_dai = rtd->codec_dais[1];
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;
	int rfs, bfs;

	dev_info(card->dev, "ap: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	if (params_rate(params) == CODEC_SAMPLE_RATE_192KHZ) {
		rfs = CODEC_RFS_192KHZ;
		bfs = CODEC_BFS_192KHZ;
	} else {
		rfs = CODEC_RFS_48KHZ;
		bfs = CODEC_BFS_48KHZ;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set Codec DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set CPU DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set SAMSUNG_I2S_CDCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
						0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set SAMSUNG_I2S_OPCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0) {
		dev_err(card->dev,
				"ap: Failed to set SAMSUNG_I2S_RCLKSRC_1\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set BFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_RCLK, rfs);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to set RFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "ap: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7270_cp_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
	int bfs, ret;

	dev_info(card->dev, "cp: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		dev_err(card->dev, "cp: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "cp: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int external_bt_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret = 0;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev,
			"%s: set DAIFMT failed\n", codec_dai->name);
		return ret;
	}

	dev_info(codec_dai->dev, "%s, %d format, %dch, %dHz\n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
						? "Playback" : "Capture",
		params_format(params), params_channels(params),
		params_rate(params));
	return ret;
}

static int external_bt_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	if (!codec_dai->active) {
		dev_info(codec_dai->dev, "%s: unset tristate (%s)\n",
			codec_dai->name, substream->stream ? "C" : "P");
		snd_soc_dai_set_tristate(codec_dai, 0);
	}

	return 0;
}

void external_bt_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	if (!codec_dai->active) {
		dev_info(codec_dai->dev, "%s: set tristate (%s)\n",
			codec_dai->name, substream->stream ? "C" : "P");
		snd_soc_dai_set_tristate(codec_dai, 1);
	}
}

static int universal7270_ap_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = NULL;
	struct snd_soc_dai *largo_aif1 = NULL;
	int ret = 0;

	if (rtd->num_codecs > 1)
		largo_aif1 = rtd->codec_dais[1];
	else
		largo_aif1 = rtd->codec_dai;

	codec = largo_aif1->codec;

	dev_dbg(codec->dev, "%s: active %d, bias_level %d\n", __func__,
				largo_aif1->active, card->dapm.bias_level);

	if (!largo_aif1->active &&
			card->dapm.bias_level >= SND_SOC_BIAS_PREPARE) {
		dev_info(largo_aif1->dev, "%s: already activated\n", __func__);
		dev_info(codec->dev,
			"%s: AIF1BCLK is used for FLL1\n", __func__);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
					    ARIZONA_FLL_SRC_AIF1BCLK,
					    3072000, /* 48000 x 2 x 16 x 2*/
					    SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev, "%s: Failed to start FLL: %d\n",
								__func__, ret);
	}

	if (!largo_aif1->active) {
		ret = snd_soc_dai_set_tristate(largo_aif1, 0);
		if (ret < 0)
			dev_err(largo_aif1->dev,
			"%s: can't unset tristate %d\n", largo_aif1->name, ret);
		else
			dev_info(largo_aif1->dev,
				"%s: unset tristate\n", largo_aif1->name);
	}
	dev_info(card->dev, "ap: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);


	return 0;
}

void universal7270_ap_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = NULL;
	struct snd_soc_dai *largo_aif1 = NULL;
	int ret = 0;

	if (rtd->num_codecs > 1)
		largo_aif1 = rtd->codec_dais[1];
	else
		largo_aif1 = rtd->codec_dai;

	codec = largo_aif1->codec;

	dev_dbg(codec->dev, "%s: active %d, bias_level %d\n", __func__,
				largo_aif1->active, card->dapm.bias_level);
	if (!largo_aif1->active) {
		int fll_src;
		unsigned int mclk_rate;

		if (codec_mclk_used == CODEC_MCLK1) {
			fll_src = ARIZONA_FLL_SRC_MCLK1;
			mclk_rate = MCLK1_RATE;
		} else {
			fll_src = ARIZONA_FLL_SRC_MCLK2;
			mclk_rate = MCLK2_RATE;
		}
		dev_info(codec->dev, "%s: %s is used for FLL1\n", __func__,
					codec_mclk_text[codec_mclk_used]);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
						fll_src, mclk_rate,
						SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev, "%s: Failed to start FLL: %d\n",
								__func__, ret);

		dev_info(codec->dev, "%s: %s is used for FLL1_REFCLK\n",
				__func__, codec_mclk_text[codec_mclk_used]);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1_REFCLK,
						fll_src, mclk_rate,
						SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev, "%s: Failed to start FLL_REFCLK: %d\n",
								__func__, ret);

		ret = snd_soc_dai_set_tristate(largo_aif1, 1);
		if (ret < 0)
			dev_err(largo_aif1->dev,
			"%s: can't set tristate %d\n", largo_aif1->name, ret);
		else
			dev_info(largo_aif1->dev,
				"%s: set tristate\n", largo_aif1->name);
	}
	dev_info(card->dev, "ap: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);
}

static int universal7270_cp_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = NULL;
	struct snd_soc_dai *largo_aif1 = NULL;
	int ret = 0;

	if (rtd->num_codecs > 1)
		largo_aif1 = rtd->codec_dais[1];
	else
		largo_aif1 = rtd->codec_dai;

	codec = largo_aif1->codec;

	dev_dbg(codec->dev, "%s: active %d, bias_level %d\n", __func__,
				largo_aif1->active, card->dapm.bias_level);
	if (!largo_aif1->active &&
			card->dapm.bias_level >= SND_SOC_BIAS_PREPARE) {
		dev_info(largo_aif1->dev, "%s: already activated\n", __func__);
		dev_info(codec->dev,
				"%s: AIF1BCLK is used for FLL1\n", __func__);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
					    ARIZONA_FLL_SRC_AIF1BCLK,
					    3072000, /* 48000 x 2 x 16 x 2*/
					    SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev, "%s: Failed to start FLL: %d\n",
								__func__, ret);
	}
	if (!largo_aif1->active) {
		ret = snd_soc_dai_set_tristate(largo_aif1, 0);
		if (ret < 0)
			dev_err(largo_aif1->dev,
			"%s: can't unset tristate %d\n", largo_aif1->name, ret);
		else
			dev_info(largo_aif1->dev,
				"%s: unset tristate\n", largo_aif1->name);
	}
	dev_info(card->dev, "cp: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

void universal7270_cp_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = NULL;
	struct snd_soc_dai *largo_aif1 = NULL;
	int ret = 0;

	if (rtd->num_codecs > 1)
		largo_aif1 = rtd->codec_dais[1];
	else
		largo_aif1 = rtd->codec_dai;

	codec = largo_aif1->codec;

	dev_dbg(codec->dev, "%s: active %d, bias_level %d\n", __func__,
				largo_aif1->active, card->dapm.bias_level);

	if (!largo_aif1->active) {
		int fll_src;
		unsigned int mclk_rate;

		if (codec_mclk_used == CODEC_MCLK1) {
			fll_src = ARIZONA_FLL_SRC_MCLK1;
			mclk_rate = MCLK1_RATE;
		} else {
			fll_src = ARIZONA_FLL_SRC_MCLK2;
			mclk_rate = MCLK2_RATE;
		}
		dev_info(codec->dev, "%s: %s is used for FLL1\n", __func__,
					codec_mclk_text[codec_mclk_used]);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
						fll_src, mclk_rate,
						SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev, "%s: Failed to start FLL: %d\n",
								__func__, ret);

		ret = snd_soc_dai_set_tristate(largo_aif1, 1);
		if (ret < 0)
			dev_err(largo_aif1->dev, "%s: can't set tristate %d\n",
							largo_aif1->name, ret);
		else
			dev_info(largo_aif1->dev,
				"%s: set tristate\n", largo_aif1->name);
	}
	dev_info(card->dev, "cp: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);
}

static inline void dump_mclk1_registers(struct device *dev)
{
	void __iomem *reg_cmu_mif;
	u32 reg_con_mux;
	u32 reg_stat_mux;
	u32 reg_ena;

	reg_cmu_mif = ioremap(0x10460000, SZ_4K);
	reg_con_mux = readl(reg_cmu_mif + 0x0280);
	reg_stat_mux = readl(reg_cmu_mif + 0x0680);
	reg_ena = readl(reg_cmu_mif + 0x0888);
	iounmap(reg_cmu_mif);

	dev_info(dev, "CMU_REG: [0x%X] [0x%X] [0x%02X]\n",
			reg_con_mux, reg_stat_mux, reg_ena);
}

static void codec_mclk1_control(struct device *dev, bool enable)
{
	mutex_lock(&codec_mclk1_mutex);
	if (enable) {
		if (!atomic_read(&codec_mclk1_enabled)) {
			dev_info(dev, "mclk1 turning on\n");
			clk_prepare_enable(codec_mclk1);
			dump_mclk1_registers(dev);
			atomic_set(&codec_mclk1_enabled, true);
			mdelay(20);
		}
	} else {
		if (atomic_read(&codec_mclk1_enabled)) {
			dev_info(dev, "mclk1 turning off\n");
			clk_disable_unprepare(codec_mclk1);
			dump_mclk1_registers(dev);
			atomic_set(&codec_mclk1_enabled, false);
		}
	}
	mutex_unlock(&codec_mclk1_mutex);
}

static int universal7270_set_bias_level(struct snd_soc_card *card,
				 struct snd_soc_dapm_context *dapm,
				 enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = NULL;
	struct snd_soc_dai *largo_aif2 = NULL;
	struct snd_soc_codec *codec = NULL;
	int ret;
	int fll_src;
	unsigned int mclk_rate;

	if (codec_mclk_used == CODEC_MCLK1) {
		fll_src = ARIZONA_FLL_SRC_MCLK1;
		mclk_rate = MCLK1_RATE;
	} else {
		fll_src = ARIZONA_FLL_SRC_MCLK2;
		mclk_rate = MCLK2_RATE;
	}

	if (card->rtd[0].num_codecs > 1)
		codec_dai = card->rtd[0].codec_dais[1]; /* multi codec */
	else
		codec_dai = card->rtd[0].codec_dai;

	largo_aif2 = card->rtd[2].codec_dai;

	codec = codec_dai->codec;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level != SND_SOC_BIAS_STANDBY)
			break;

		if (codec_mclk_used == CODEC_MCLK1)
			codec_mclk1_control(card->dev, true);

		dev_info(codec->dev, "%s used for FLL1 REFCLK\n",
					codec_mclk_text[codec_mclk_used]);
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1_REFCLK,
					fll_src, mclk_rate,
					SYSCLK_RATE);
		if (ret < 0)
			dev_err(codec->dev,
				"Failed to start FLL REFCLK : %d\n", ret);

		if (codec_dai->active) {
			/* largo-aif1 is active.*/
			/* set AIF1BCLK as FLL1 SRC*/
			dev_info(codec->dev, "AIF1BCLK is used for FLL1\n");
			ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
					    ARIZONA_FLL_SRC_AIF1BCLK,
					    3072000, /* 48000 x 2 x 16 x 2*/
					    SYSCLK_RATE);
		} else {
			dev_info(codec->dev, "%s used for FLL1\n",
					codec_mclk_text[codec_mclk_used]);
			ret = snd_soc_codec_set_pll(codec, LARGO_FLL1,
						fll_src, mclk_rate,
						SYSCLK_RATE);
		}
		if (ret < 0)
			dev_err(codec->dev, "Failed to start FLL: %d\n", ret);

		break;
	default:
		break;
	}

	card->dapm.bias_level = level;

	return 0;
}

static int universal7270_set_bias_level_post(struct snd_soc_card *card,
				       struct snd_soc_dapm_context *dapm,
				       enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = NULL;
	struct snd_soc_codec *codec = NULL;
	int ret = 0;

	if (card->rtd[0].num_codecs > 1)
		codec_dai = card->rtd[0].codec_dais[1]; /* multi codec */
	else
		codec_dai = card->rtd[0].codec_dai;

	codec = codec_dai->codec;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		dev_info(card->dev, "pll in audio codec is disabled\n");
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1, 0, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop FLL: %d\n", ret);
			goto bias_level_post_err;
		}
		ret = snd_soc_codec_set_pll(codec, LARGO_FLL1_REFCLK,
					ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop FLL: %d\n", ret);
			goto bias_level_post_err;
		}
		/* we do not need mclk1 anyway at standby level */
		codec_mclk1_control(card->dev, false);
		break;
	default:
		break;
	}

	dapm->bias_level = level;

bias_level_post_err:

	return ret;
}

static void universal7270_ez2c_trigger(void)
{
	unsigned int keycode = KEY_VOICE_WAKEUP;

	pr_info("largo: Raise key event (%d)\n", keycode);

	input_report_key(voice_input, keycode, 1);
	input_sync(voice_input);
	usleep_range(10000, 10000 + 100);
	input_report_key(voice_input, keycode, 0);
	input_sync(voice_input);
}

static int universal7270_late_probe(struct snd_soc_card *card)
{

	struct snd_soc_dai *largo_aif1_dai = card->rtd[0].codec_dais[1];
	struct snd_soc_dai *largo_aif2_dai = card->rtd[2].codec_dai;
	struct snd_soc_codec *codec = largo_aif1_dai->codec;
	int ret;

	snd_soc_dapm_ignore_suspend(&card->dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_sync(&codec->dapm);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       SYSCLK_RATE,
				       SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(largo_aif1_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret != 0) {
		dev_err(largo_aif1_dai->dev,
				"Failed to set AIF1 clock: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(largo_aif2_dai,
				ARIZONA_CLK_SYSCLK_2, 0, 0);
	if (ret != 0) {
		dev_err(largo_aif2_dai->dev,
				"Failed to set AIF2 clock: %d\n", ret);
		return ret;
	}
	dev_info(largo_aif2_dai->dev,
			"set AIF2 clock to %s\n", largo_aif2_dai->name);

	ret = snd_soc_codec_set_pll(codec, LARGO_FLL1_REFCLK,
			ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to clear FLL refclk: %d\n", ret);
		return ret;
	}

	arizona_set_ez2ctrl_cb(codec, universal7270_ez2c_trigger);
	/* largo-aif3 set to tristate */
	ret = arizona_set_tristate_direct(codec, 3, 1);
	if (ret < 0)
		dev_err(codec->dev, "largo-aif3: can't set tristate\n");
	/* largo-aif2 set to tristate */
	ret = snd_soc_dai_set_tristate(largo_aif2_dai, 1);
	if (ret < 0)
		dev_err(largo_aif2_dai->dev, "%s: can't set tristate %d\n",
						largo_aif2_dai->name, ret);
	/* largo-aif1 set to tristate */
	ret = snd_soc_dai_set_tristate(largo_aif1_dai, 1);
	if (ret < 0)
		dev_err(largo_aif1_dai->dev, "%s: can't set tristate %d\n",
						largo_aif1_dai->name, ret);

	return 0;
}

static int audmixer_init(struct snd_soc_component *cmp)
{
	dev_dbg(cmp->dev, "%s called\n", __func__);

	return 0;
}

static int universal7270_codec_mclk_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = codec_mclk_used;
	return 0;
}

static int universal7270_codec_mclk_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	codec_mclk_used = ucontrol->value.integer.value[0];
	pr_info("%s: set to %s\n", __func__, codec_mclk_text[codec_mclk_used]);
	return 1;
}

static const struct snd_kcontrol_new universal7270_controls[] = {
	SOC_ENUM_EXT("CODEC_MCLK", codec_mclk_enum,
		universal7270_codec_mclk_get, universal7270_codec_mclk_put),
};

const struct snd_soc_dapm_widget universal7270_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("DMIC1", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

const struct snd_soc_dapm_route universal7270_dapm_routes[] = {
	{ "DMIC1", NULL, "MICBIAS1" },
	{ "IN1L", NULL, "DMIC1" },
	{ "IN1R", NULL, "DMIC1" },
	{ "SPK", NULL, "SPKOUTN"},
	{ "SPK", NULL, "SPKOUTP"},
};

static struct snd_soc_ops universal7270_aif1_ops = {
	.hw_params = universal7270_ap_hw_params,
	.startup = universal7270_ap_startup,
	.shutdown = universal7270_ap_shutdown,
};

static struct snd_soc_ops universal7270_aif2_ops = {
	.hw_params = universal7270_cp_hw_params,
	.startup = universal7270_cp_startup,
	.shutdown = universal7270_cp_shutdown,
};

static struct snd_soc_ops external_bt_ops = {
	.hw_params = external_bt_hw_params,
	.startup = external_bt_startup,
	.shutdown = external_bt_shutdown,
};

static struct snd_soc_dai_driver universal7270_ext_dai[] = {
	{
		.name = "universal7270 voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
	{
		.name = "external BT",
		.playback = {
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 1,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link_component codecs_ap0[] = {{
		.name = "14880000.s1403x",
		.dai_name = "AP0",
	}, {
		.dai_name = "largo-aif1",
	},
};

static struct snd_soc_dai_link_component codecs_cp0[] = {{
		.name = "14880000.s1403x",
		.dai_name = "CP0",
	}, {
		.dai_name = "largo-aif1",
	},
};

static struct snd_soc_dai_link universal7270_largo_dai[] = {
	/* Playback and Recording */
	{
		.name = "universal7270-largo",
		.stream_name = "i2s0-pri",
		.codecs = codecs_ap0,
		.num_codecs = ARRAY_SIZE(codecs_ap0),
		.ops = &universal7270_aif1_ops,
		.ignore_pmdown_time = 1,
	},
	/* Voice Call */
	{
		.name = "cp",
		.stream_name = "voice call",
		.cpu_dai_name = "universal7270 voice call",
		.platform_name = "snd-soc-dummy",
		.codecs = codecs_cp0,
		.num_codecs = ARRAY_SIZE(codecs_cp0),
		.ops = &universal7270_aif2_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	/* BT */
	{
		.name = "bt",
		.stream_name = "bluetooth sco",
		.cpu_dai_name = "external BT",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "largo-aif2",
		.codec_name = "largo-codec",
		.ops = &external_bt_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	/* Deep buffer playback */
	{
		.name = "universal7270-largo-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.stream_name = "i2s0-sec",
		.platform_name = "samsung-i2s-sec",
		.codecs = codecs_ap0,
		.num_codecs = ARRAY_SIZE(codecs_ap0),
		.ops = &universal7270_aif1_ops,
	},
	/* Voice Control */
	{
		.name = "CPU-DSP Voice Control",
		.stream_name = "CPU-DSP Voice Control",
		.cpu_dai_name = "largo-cpu-voicectrl",
		.platform_name = "largo-codec",
		.codec_dai_name = "largo-dsp-voicectrl",
		.codec_name = "largo-codec",
	},
	/* Trace */
	{
		.name = "CPU-DSP Trace",
		.stream_name = "CPU-DSP Trace",
		.cpu_dai_name = "largo-cpu-trace",
		.platform_name = "largo-codec",
		.codec_dai_name = "largo-dsp-trace",
		.codec_name = "largo-codec",
	},
};

static struct snd_soc_aux_dev audmixer_aux_dev[] = {
	{
		.init = audmixer_init,
	},
};

static struct snd_soc_codec_conf audmixer_codec_conf[] = {
	{
		.name_prefix = "AudioMixer",
	},
};


static int universal7270_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_dai *cp0_dai = card->rtd[1].codec_dais[0];

	if (cp0_dai->active)
		dev_info(card->dev, "CP0 still alive\n");
	else
		dev_info(card->dev, "CP0 deactive\n");

	return 0;
}

static int universal7270_resume_pre(struct snd_soc_card *card)
{
	return 0;
}

static struct snd_soc_card universal7270_largo_card = {
	.name = "universal7270-largo",
	.owner = THIS_MODULE,

	.dai_link = universal7270_largo_dai,
	.num_links = ARRAY_SIZE(universal7270_largo_dai),

	.controls = universal7270_controls,
	.num_controls = ARRAY_SIZE(universal7270_controls),
	.dapm_widgets = universal7270_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal7270_dapm_widgets),
	.dapm_routes = universal7270_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal7270_dapm_routes),

	.late_probe = universal7270_late_probe,
	.set_bias_level = universal7270_set_bias_level,
	.set_bias_level_post = universal7270_set_bias_level_post,
	.aux_dev = audmixer_aux_dev,
	.num_aux_devs = ARRAY_SIZE(audmixer_aux_dev),
	.codec_conf = audmixer_codec_conf,
	.num_configs = ARRAY_SIZE(audmixer_codec_conf),

	.suspend_post = universal7270_suspend_post,
	.resume_pre = universal7270_resume_pre,
};

static int init_input_device(void)
{
	int ret = 0;

	voice_input = input_allocate_device();
	if (!voice_input) {
		ret = -ENOMEM;
		goto init_input_device_exit;
	}

	voice_input->name = "voice input";
	set_bit(EV_SYN, voice_input->evbit);
	set_bit(EV_KEY, voice_input->evbit);
	set_bit(KEY_VOICE_WAKEUP, voice_input->keybit);

	ret = input_register_device(voice_input);
	if (ret < 0) {
		pr_info("%s: input_device_register failed\n", __func__);
		input_free_device(voice_input);
	}

init_input_device_exit:
	return ret;
}

static int deinit_input_device(void)
{
	input_unregister_device(voice_input);
	return 0;
}

#ifdef CONFIG_SLEEP_MONITOR
int universal7270_audio_sm_read_cb_func(void *dev, unsigned int *val,
					int check_level, int caller_type)
{
	struct platform_device *pdev = dev;
	struct snd_soc_card *card = &universal7270_largo_card;
	int bias_level = 0;
	int adsp2 = 0;
	int adsp3 = 0;
	int ret = DEVICE_ERR_1;

	bias_level = card->rtd[0].codec_dais[1]->codec->dapm.bias_level;

	/* DSP2 */
	adsp2 = largo_get_adsp_status(card->rtd[4].codec_dai->codec, 1);
	/* DSP3 */
	adsp3 = largo_get_adsp_status(card->rtd[4].codec_dai->codec, 2);

	dev_dbg(&pdev->dev, "bias [%d], adsp [%d, %d]\n",
						bias_level, adsp2, adsp3);

	*val = ((adsp3 & 0x01) << 8) |
			((adsp2 & 0x01) << 4) | (bias_level & 0x0f);

	switch (bias_level) {
	case SND_SOC_BIAS_OFF:
		ret = DEVICE_POWER_OFF;
		break;
	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_ON:
		if (adsp2 && adsp3)
			ret = DEVICE_ON_ACTIVE2;
		else if (!adsp2 && adsp3)
			ret = DEVICE_ON_LOW_POWER;
		else
			ret = DEVICE_ON_ACTIVE1;
	}

	return ret;
}

static struct sleep_monitor_ops sm_ops = {
	.read_cb_func = universal7270_audio_sm_read_cb_func,
};
#endif

static int universal7270_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np, *auxdev_np;
	struct snd_soc_card *card = &universal7270_largo_card;

	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;
	card->num_links = 0;

	ret = snd_soc_register_component(card->dev, &universal7270_cmpnt,
			universal7270_ext_dai,
			ARRAY_SIZE(universal7270_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(universal7270_largo_dai); n++) {
		/* Skip parsing DT for fully formed dai links */
		if (universal7270_largo_dai[n].platform_name &&
				universal7270_largo_dai[n].codec_name) {
			dev_dbg(card->dev,
			"Skipping dt for populated dai link %s\n",
			universal7270_largo_dai[n].name);
			card->num_links++;
			continue;
		}

		cpu_np = of_parse_phandle(np, "samsung,audio-cpu", n);
		if (!cpu_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing\n");
			break;
		}

		codec_np = of_parse_phandle(np, "samsung,audio-codec", n);
		if (!codec_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-codec' missing\n");
			break;
		}

		if (universal7270_largo_dai[n].num_codecs > 1 &&
			universal7270_largo_dai[n].codecs)
			universal7270_largo_dai[n].codecs[1].of_node = codec_np;
		if (!universal7270_largo_dai[n].cpu_dai_name)
			universal7270_largo_dai[n].cpu_of_node = cpu_np;
		if (!universal7270_largo_dai[n].platform_name)
			universal7270_largo_dai[n].platform_of_node = cpu_np;

		card->num_links++;
	}

	for (n = 0; n < ARRAY_SIZE(audmixer_aux_dev); n++) {
		auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
		if (!auxdev_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,auxdev' missing\n");
			return -EINVAL;
		}

		audmixer_aux_dev[n].codec_of_node = auxdev_np;
		audmixer_codec_conf[n].of_node = auxdev_np;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		goto out_failed;
	}

	if (init_input_device() < 0)
		dev_err(&pdev->dev, "%s: input device init failed\n", __func__);

	mutex_init(&codec_mclk1_mutex);
	atomic_set(&codec_mclk1_enabled, false);

	codec_mclk1 = clk_get(&pdev->dev, "codec_26m_ap");
	if (IS_ERR(codec_mclk1)) {
		dev_err(&pdev->dev, "%s: can't get mclk1 of codec\n", __func__);
	} else {
		dev_info(&pdev->dev, "%s: get mclk1 of codec\n", __func__);
		if (clk_set_rate(codec_mclk1, MCLK1_RATE) < 0)
			dev_err(&pdev->dev,
				"%s: can't set rate mclk1\n", __func__);
	}

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops((void *)pdev, &sm_ops, SLEEP_MONITOR_AUDIO);
#endif
out_failed:

	return ret;
}

static int universal7270_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	if (codec_mclk1) {
		clk_put(codec_mclk1);
		codec_mclk1 = NULL;
	}
	mutex_destroy(&codec_mclk1_mutex);

	deinit_input_device();

	return 0;
}

static const struct of_device_id universal7270_largo_of_match[] = {
	{.compatible = "samsung,universal7270-wm1831",},
	{},
};
MODULE_DEVICE_TABLE(of, universal7270_largo_of_match);

static struct platform_driver universal7270_audio_driver = {
	.driver = {
		.name = "universal7270-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal7270_largo_of_match),
	},
	.probe = universal7270_audio_probe,
	.remove = universal7270_audio_remove,
};

module_platform_driver(universal7270_audio_driver);

MODULE_DESCRIPTION("ALSA SoC universal7270 largo");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:universal7270-audio");

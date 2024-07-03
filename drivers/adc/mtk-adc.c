// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 MediaTek Inc.
 * Author: Shu-Yun Wu <shu-yun.wu@mediatek.com>
 */
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <div64.h>
#include <errno.h>
#include <adc.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <mtk-adc.h>

static inline void auxadc_mod_reg(void __iomem *reg,
				  unsigned int or_mask,
				  unsigned int and_mask)
{
	unsigned int val;

	val = readl(reg);
	val |= or_mask;
	val &= ~and_mask;
	writel(val, reg);
}

int mtk_adc_channel_data(struct udevice *dev, int channel,
			 unsigned int *data)
{
	struct mtk_auxadc_device *adc_dev = dev_get_priv(dev);
	void __iomem *reg_channel;
	unsigned int val = 0;
	int ret;

	if (channel >= MTK_AUXADC_MAX_CHANNELS) {
		dev_err(dev, "channel[%d] exceed max_channel\n", channel);
		ret = -EINVAL;
		goto out;
	}

	reg_channel = adc_dev->base + MTK_AUXADC_DAT0 +
		      channel * MTK_AUXADC_DATA_N_OFFSET;

	mutex_lock(&adc_dev->lock);

	/* read channel and make sure ready bit == 1 */
	ret = readl_poll_timeout(reg_channel, val,
				 ((val & MTK_AUXADC_RDY0) != 0),
				 MTK_AUXADC_TIMEOUT_US);
	if (ret < 0) {
		dev_err(dev, "wait for channel[%d] data ready time out\n", channel);
		goto unlock;
	}

	/* read data */
	*data = readl(reg_channel) & MTK_AUXADC_DATA_MASK;

	/* convert data */
	*data = *data * VOLTAGE_FULL_RANGE / AUXADC_PRECISE;

unlock:
	mutex_unlock(&adc_dev->lock);

out:
	return ret;
}

int mtk_adc_start_channel(struct udevice *dev, int channel)
{
	struct mtk_auxadc_device *adc_dev = dev_get_priv(dev);
	void __iomem *reg_channel;
	unsigned int val = 0;
	int ret;

	if (channel >= MTK_AUXADC_MAX_CHANNELS) {
		dev_err(dev, "channel[%d] exceed max_channels\n", channel);
		ret = -EINVAL;
		goto out;
	}

	reg_channel = adc_dev->base + MTK_AUXADC_DAT0 +
		      channel * MTK_AUXADC_DATA_N_OFFSET;

	mutex_lock(&adc_dev->lock);

	writel(1 << channel, adc_dev->base + MTK_AUXADC_CON1_CLR);

	/* read channel and make sure old ready bit == 0 */
	ret = readl_poll_timeout(reg_channel, val,
				 ((val & MTK_AUXADC_RDY0) == 0),
				 MTK_AUXADC_TIMEOUT_US);
	if (ret < 0) {
		dev_err(dev, "wait for channel[%d] ready bit clear time out\n", channel);
		goto unlock;
	}

	/* set bit to trigger sample */
	writel(1 << channel, adc_dev->base + MTK_AUXADC_CON1_SET);

	/* we must delay here for hardware sample channel data */
	udelay(MTK_AUXADC_SAMPLE_READY_US);

unlock:
	mutex_unlock(&adc_dev->lock);

out:
	return ret;
}

int mtk_adc_stop(struct udevice *dev)
{
	struct mtk_auxadc_device *adc_dev = dev_get_priv(dev);

	/* Stop conversion, suspend ADC */
	auxadc_mod_reg(adc_dev->base + MTK_AUXADC_MISC,
		       0, MTK_AUXADC_PDN_EN);

	return 0;
}

static int mtk_adc_probe(struct udevice *dev)
{
	struct mtk_auxadc_device *adc_dev = dev_get_priv(dev);
	int ret = 0;
	unsigned int val = 0;

	adc_dev->base = dev_read_addr_ptr(dev);
	if (!adc_dev->base)
		return -EINVAL;

	ret = clk_get_by_name(dev, "main", &adc_dev->main_clk);
	if (ret < 0)
		return ret;

	ret = clk_enable(&adc_dev->main_clk);
	if (ret < 0)
		return ret;

	val = readl(adc_dev->base + MTK_AUXADC_MISC);
	if (!(val & MTK_AUXADC_PDN_EN)) {
		auxadc_mod_reg(adc_dev->base + MTK_AUXADC_MISC,
			       MTK_AUXADC_PDN_EN, 0);
		mdelay(MTK_AUXADC_POWER_READY_MS);
	}

	return ret;
}

int mtk_adc_ofdata_to_platdata(struct udevice *dev)
{
	struct adc_uclass_plat *uc_pdata = dev_get_uclass_plat(dev);

	uc_pdata->data_mask = MTK_AUXADC_DATA_MASK;
	uc_pdata->data_format = ADC_DATA_FORMAT_BIN;
	uc_pdata->data_timeout_us = MTK_AUXADC_TIMEOUT_US;
	uc_pdata->channel_mask = MTK_AUXADC_CHANNEL_MASK;

	return 0;
}

static const struct adc_ops mtk_adc_ops = {
	.start_channel = mtk_adc_start_channel,
	.channel_data = mtk_adc_channel_data,
	.stop = mtk_adc_stop,
};

static const struct udevice_id mtk_adc_ids[] = {
	{ .compatible = "mediatek,adc",},
	{ .compatible = "mediatek,mt8169-adc",},
	{ }
};

U_BOOT_DRIVER(mtk_adc) = {
	.name = "mtk-adc",
	.id = UCLASS_ADC,
	.of_match = mtk_adc_ids,
	.ops = &mtk_adc_ops,
	.probe = mtk_adc_probe,
	.of_to_plat = mtk_adc_ofdata_to_platdata,
	.priv_auto = sizeof(struct mtk_auxadc_device),
};

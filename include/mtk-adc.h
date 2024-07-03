/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 MediaTek Inc.
 * Author: Shu-Yun Wu <shu-yun.wu@mediatek.com>
 */

#ifndef _MTK_ADC_H_
#define _MTK_ADC_H_

struct mtk_auxadc_device {
	void __iomem *base;
	struct clk main_clk;
	struct mutex lock; /* Mutex for adc interface */
};

/* Register definitions */
#define MTK_AUXADC_CON0			0x00
#define MTK_AUXADC_CON1			0x04
#define MTK_AUXADC_CON1_SET		0x08
#define MTK_AUXADC_CON1_CLR		0x0C
#define MTK_AUXADC_CON2			0x10
#define MTK_AUXADC_STA			BIT(0)

#define MTK_AUXADC_DAT0			0x14
#define MTK_AUXADC_RDY0			BIT(12)

#define MTK_AUXADC_MISC			0x94
#define MTK_AUXADC_PDN_EN		BIT(14)

#define MTK_AUXADC_MAX_CHANNELS		16
#define MTK_AUXADC_CHANNEL_MASK		0xFF
#define MTK_AUXADC_TIMEOUT_US		2000
#define MTK_AUXADC_SAMPLE_READY_US	25
#define MTK_AUXADC_DATA_MASK		0xFFF /* 12-bits resolution */
#define MTK_AUXADC_DATA_N_OFFSET	0x04
#define MTK_AUXADC_POWER_READY_MS	1

/* For Voltage calculation */
#define VOLTAGE_FULL_RANGE		1500 /* VA voltage */
#define AUXADC_PRECISE			4096 /* 12 bits */

#endif

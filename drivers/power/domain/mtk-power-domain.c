// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

#include <dm/device_compat.h>
#include <dm/devres.h>
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <power-domain-uclass.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/iopoll.h>

#include <dt-bindings/power/mt7623-power.h>
#include <dt-bindings/power/mt7629-power.h>
#include <dt-bindings/power/mt8188-power.h>
#include <dt-bindings/power/mt8195-power.h>
#include <dt-bindings/power/mt8365-power.h>

#define SPM_EN			(0xb16 << 16 | 0x1)
#define SPM_VDE_PWR_CON		0x0210
#define SPM_MFG_PWR_CON		0x0214
#define SPM_ISP_PWR_CON		0x0238
#define SPM_DIS_PWR_CON		0x023c
#define SPM_CONN_PWR_CON	0x0280
#define SPM_BDP_PWR_CON		0x029c
#define SPM_ETH_PWR_CON		0x02a0
#define SPM_HIF_PWR_CON		0x02a4
#define SPM_IFR_MSC_PWR_CON	0x02a8
#define SPM_ETHSYS_PWR_CON	0x2e0
#define SPM_HIF0_PWR_CON	0x2e4
#define SPM_HIF1_PWR_CON	0x2e8
#define SPM_PWR_STATUS		0x60c
#define SPM_PWR_STATUS_2ND	0x610

#define PWR_RST_B_BIT		BIT(0)
#define PWR_ISO_BIT		BIT(1)
#define PWR_ON_BIT		BIT(2)
#define PWR_ON_2ND_BIT		BIT(3)
#define PWR_CLK_DIS_BIT		BIT(4)

#define PWR_STATUS_CONN		BIT(1)
#define PWR_STATUS_DISP		BIT(3)
#define PWR_STATUS_MFG		BIT(4)
#define PWR_STATUS_ISP		BIT(5)
#define PWR_STATUS_VDEC		BIT(7)
#define PWR_STATUS_BDP		BIT(14)
#define PWR_STATUS_ETH		BIT(15)
#define PWR_STATUS_HIF		BIT(16)
#define PWR_STATUS_IFR_MSC	BIT(17)
#define PWR_STATUS_ETHSYS	BIT(24)
#define PWR_STATUS_HIF0		BIT(25)
#define PWR_STATUS_HIF1		BIT(26)

/* MT8365 specific registers */
#define MT8365_SPM_PWR_STATUS	   0x180
#define MT8365_SPM_PWR_STATUS_2ND  0x184

/* Infrasys configuration */
#define INFRA_TOPDCM_CTRL	0x10
#define INFRA_TOPAXI_PROT_EN	0x220
#define INFRA_TOPAXI_PROT_STA1	0x228

#define DCM_TOP_EN		BIT(0)

#define SPM_MAX_BUS_PROT_DATA	6

#define MT8188_TOP_AXI_PROT_EN_SET				(0x2A0)
#define MT8188_TOP_AXI_PROT_EN_CLR				(0x2A4)
#define MT8188_TOP_AXI_PROT_EN_STA				(0x228)
#define MT8188_TOP_AXI_PROT_EN_1_SET				(0x2A8)
#define MT8188_TOP_AXI_PROT_EN_1_CLR				(0x2AC)
#define MT8188_TOP_AXI_PROT_EN_1_STA				(0x258)
#define MT8188_TOP_AXI_PROT_EN_2_SET				(0x714)
#define MT8188_TOP_AXI_PROT_EN_2_CLR				(0x718)
#define MT8188_TOP_AXI_PROT_EN_2_STA				(0x724)

#define MT8188_TOP_AXI_PROT_EN_MM_SET				(0x2D4)
#define MT8188_TOP_AXI_PROT_EN_MM_CLR				(0x2D8)
#define MT8188_TOP_AXI_PROT_EN_MM_STA				(0x2EC)
#define MT8188_TOP_AXI_PROT_EN_MM_2_SET				(0xDCC)
#define MT8188_TOP_AXI_PROT_EN_MM_2_CLR				(0xDD0)
#define MT8188_TOP_AXI_PROT_EN_MM_2_STA				(0xDD8)

#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET			(0xB84)
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR			(0xB88)
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA			(0xB90)
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET		(0xBCC)
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR		(0xBD0)
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA		(0xBD8)

#define MT8188_TOP_AXI_PROT_EN_MFG1_STEP1			(BIT(11))
#define MT8188_TOP_AXI_PROT_EN_2_MFG1_STEP2			(BIT(7))
#define MT8188_TOP_AXI_PROT_EN_1_MFG1_STEP3			(BIT(19))
#define MT8188_TOP_AXI_PROT_EN_2_MFG1_STEP4			(BIT(5))
#define MT8188_TOP_AXI_PROT_EN_MFG1_STEP5			(GENMASK(22, 21))
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MFG1_STEP6	(BIT(17))

#define MT8188_TOP_AXI_PROT_EN_PEXTP_MAC_P0_STEP1		(BIT(2))
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_PEXTP_MAC_P0_STEP2	(BIT(8) | BIT(18) | BIT(30))
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_ETHER_STEP1		(BIT(24))
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_HDMI_TX_STEP1		(BIT(20))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_AO_STEP1			(GENMASK(31, 29))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_AO_STEP2			(GENMASK(4, 3) | BIT(28))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_INFRA_STEP1		(GENMASK(16, 14) | BIT(23) | \
								BIT(27))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_INFRA_STEP2		(GENMASK(19, 17) | GENMASK(26, 25))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_STEP1			(GENMASK(11, 8))
#define MT8188_TOP_AXI_PROT_EN_2_ADSP_STEP2			(GENMASK(22, 21))
#define MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP1			(BIT(20))
#define MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP2			(BIT(12))
#define MT8188_TOP_AXI_PROT_EN_2_AUDIO_ASRC_STEP1		(BIT(24))
#define MT8188_TOP_AXI_PROT_EN_2_AUDIO_ASRC_STEP2		(BIT(13))

#define MT8188_TOP_AXI_PROT_EN_VPPSYS0_STEP1			(BIT(10))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS0_STEP2		(GENMASK(9, 8))
#define MT8188_TOP_AXI_PROT_EN_VPPSYS0_STEP3			(BIT(23))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS0_STEP4		(BIT(1) | BIT(4) | BIT(11))
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VPPSYS0_STEP5	(BIT(20))
#define MT8188_TOP_AXI_PROT_EN_MM_VDOSYS0_STEP1			(GENMASK(18, 17) | GENMASK(21, 20))
#define MT8188_TOP_AXI_PROT_EN_VDOSYS0_STEP2			(BIT(6))
#define MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VDOSYS0_STEP3	(BIT(21))
#define MT8188_TOP_AXI_PROT_EN_MM_VDOSYS1_STEP1			(GENMASK(31, 30))
#define MT8188_TOP_AXI_PROT_EN_MM_VDOSYS1_STEP2			(BIT(22))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VDOSYS1_STEP3		(BIT(10))
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_DP_TX_STEP1		(BIT(23))
#define MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_EDP_TX_STEP1		(BIT(22))

#define MT8188_TOP_AXI_PROT_EN_MM_VPPSYS1_STEP1			(GENMASK(6, 5))
#define MT8188_TOP_AXI_PROT_EN_MM_VPPSYS1_STEP2			(BIT(23))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS1_STEP3		(BIT(18))
#define MT8188_TOP_AXI_PROT_EN_MM_2_WPE_STEP1			(BIT(23))
#define MT8188_TOP_AXI_PROT_EN_MM_2_WPE_STEP2			(BIT(21))
#define MT8188_TOP_AXI_PROT_EN_MM_VDEC0_STEP1			(BIT(13))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VDEC0_STEP2			(BIT(13))
#define MT8188_TOP_AXI_PROT_EN_MM_VDEC1_STEP1			(BIT(14))
#define MT8188_TOP_AXI_PROT_EN_MM_VDEC1_STEP2			(BIT(29))
#define MT8188_TOP_AXI_PROT_EN_MM_VENC_STEP1			(BIT(9) | BIT(11))
#define MT8188_TOP_AXI_PROT_EN_MM_VENC_STEP2			(BIT(26))
#define MT8188_TOP_AXI_PROT_EN_MM_2_VENC_STEP3			(BIT(2))
#define MT8188_TOP_AXI_PROT_EN_MM_IMG_VCORE_STEP1		(BIT(1) | BIT(3))
#define MT8188_TOP_AXI_PROT_EN_MM_IMG_VCORE_STEP2		(BIT(25))
#define MT8188_TOP_AXI_PROT_EN_MM_2_IMG_VCORE_STEP3		(BIT(16))
#define MT8188_TOP_AXI_PROT_EN_MM_2_IMG_MAIN_STEP1		(GENMASK(27, 26))
#define MT8188_TOP_AXI_PROT_EN_MM_2_IMG_MAIN_STEP2		(GENMASK(25, 24))
#define MT8188_TOP_AXI_PROT_EN_MM_CAM_VCORE_STEP1		(BIT(2) | BIT(4))
#define MT8188_TOP_AXI_PROT_EN_2_CAM_VCORE_STEP2		(BIT(0))
#define MT8188_TOP_AXI_PROT_EN_1_CAM_VCORE_STEP3		(BIT(22))
#define MT8188_TOP_AXI_PROT_EN_MM_CAM_VCORE_STEP4		(BIT(24))
#define MT8188_TOP_AXI_PROT_EN_MM_2_CAM_VCORE_STEP5		(BIT(17))
#define MT8188_TOP_AXI_PROT_EN_MM_2_CAM_MAIN_STEP1		(GENMASK(31, 30))
#define MT8188_TOP_AXI_PROT_EN_2_CAM_MAIN_STEP2			(BIT(2))
#define MT8188_TOP_AXI_PROT_EN_MM_2_CAM_MAIN_STEP3		(GENMASK(29, 28))
#define MT8188_TOP_AXI_PROT_EN_2_CAM_MAIN_STEP4			(BIT(1))

#define MT8188_SMI_COMMON_CLAMP_EN_STA				(0x3C0)
#define MT8188_SMI_COMMON_CLAMP_EN_SET				(0x3C4)
#define MT8188_SMI_COMMON_CLAMP_EN_CLR				(0x3C8)

#define MT8188_SMI_COMMON_SMI_CLAMP_DIP_TO_VDO0			(GENMASK(3, 1))
#define MT8188_SMI_COMMON_SMI_CLAMP_DIP_TO_VPP1			(GENMASK(2, 1))
#define MT8188_SMI_COMMON_SMI_CLAMP_IPE_TO_VPP1			(BIT(0))

#define MT8188_SMI_COMMON_SMI_CLAMP_CAM_SUBA_TO_VPP0		(GENMASK(3, 2))
#define MT8188_SMI_COMMON_SMI_CLAMP_CAM_SUBB_TO_VDO0		(GENMASK(3, 2))

#define MT8188_SMI_LARB10_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB11A_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB11C_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB12_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB11B_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB15_RESET_ADDR				(0xC)
#define MT8188_SMI_LARB16B_RESET_ADDR				(0xA0)
#define MT8188_SMI_LARB17B_RESET_ADDR				(0xA0)
#define MT8188_SMI_LARB16A_RESET_ADDR				(0xA0)
#define MT8188_SMI_LARB17A_RESET_ADDR				(0xA0)

#define MT8188_SMI_LARB10_RESET					(BIT(0))
#define MT8188_SMI_LARB11A_RESET				(BIT(0))
#define MT8188_SMI_LARB11C_RESET				(BIT(0))
#define MT8188_SMI_LARB12_RESET					(BIT(8))
#define MT8188_SMI_LARB11B_RESET				(BIT(0))
#define MT8188_SMI_LARB15_RESET					(BIT(0))
#define MT8188_SMI_LARB16B_RESET				(BIT(4))
#define MT8188_SMI_LARB17B_RESET				(BIT(4))
#define MT8188_SMI_LARB16A_RESET				(BIT(4))
#define MT8188_SMI_LARB17A_RESET				(BIT(4))

#define MT8195_TOP_AXI_PROT_EN_STA1                     0x228
#define MT8195_TOP_AXI_PROT_EN_1_STA1                   0x258
#define MT8195_TOP_AXI_PROT_EN_SET			0x2a0
#define MT8195_TOP_AXI_PROT_EN_CLR                      0x2a4
#define MT8195_TOP_AXI_PROT_EN_1_SET                    0x2a8
#define MT8195_TOP_AXI_PROT_EN_1_CLR                    0x2ac
#define MT8195_TOP_AXI_PROT_EN_MM_SET                   0x2d4
#define MT8195_TOP_AXI_PROT_EN_MM_CLR                   0x2d8
#define MT8195_TOP_AXI_PROT_EN_MM_STA1                  0x2ec
#define MT8195_TOP_AXI_PROT_EN_2_SET                    0x714
#define MT8195_TOP_AXI_PROT_EN_2_CLR                    0x718
#define MT8195_TOP_AXI_PROT_EN_2_STA1                   0x724
#define MT8195_TOP_AXI_PROT_EN_VDNR_SET                 0xb84
#define MT8195_TOP_AXI_PROT_EN_VDNR_CLR                 0xb88
#define MT8195_TOP_AXI_PROT_EN_VDNR_STA1                0xb90
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_SET               0xba4
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_CLR               0xba8
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_STA1              0xbb0
#define MT8195_TOP_AXI_PROT_EN_VDNR_2_SET               0xbb8
#define MT8195_TOP_AXI_PROT_EN_VDNR_2_CLR               0xbbc
#define MT8195_TOP_AXI_PROT_EN_VDNR_2_STA1              0xbc4
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET       0xbcc
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR       0xbd0
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA1      0xbd8
#define MT8195_TOP_AXI_PROT_EN_MM_2_SET                 0xdcc
#define MT8195_TOP_AXI_PROT_EN_MM_2_CLR                 0xdd0
#define MT8195_TOP_AXI_PROT_EN_MM_2_STA1                0xdd8

#define MT8195_TOP_AXI_PROT_EN_VDOSYS0			BIT(6)
#define MT8195_TOP_AXI_PROT_EN_VPPSYS0			BIT(10)
#define MT8195_TOP_AXI_PROT_EN_MFG1			BIT(11)
#define MT8195_TOP_AXI_PROT_EN_MFG1_2ND			GENMASK(22, 21)
#define MT8195_TOP_AXI_PROT_EN_VPPSYS0_2ND		BIT(23)
#define MT8195_TOP_AXI_PROT_EN_1_MFG1			GENMASK(20, 19)
#define MT8195_TOP_AXI_PROT_EN_1_CAM			BIT(22)
#define MT8195_TOP_AXI_PROT_EN_2_CAM			BIT(0)
#define MT8195_TOP_AXI_PROT_EN_2_MFG1_2ND		GENMASK(6, 5)
#define MT8195_TOP_AXI_PROT_EN_2_MFG1			BIT(7)
#define MT8195_TOP_AXI_PROT_EN_2_AUDIO			(BIT(9) | BIT(11))
#define MT8195_TOP_AXI_PROT_EN_2_ADSP			(BIT(12) | GENMASK(16, 14))
#define MT8195_TOP_AXI_PROT_EN_MM_CAM			(BIT(0) | BIT(2) | BIT(4))
#define MT8195_TOP_AXI_PROT_EN_MM_IPE			BIT(1)
#define MT8195_TOP_AXI_PROT_EN_MM_IMG			BIT(3)
#define MT8195_TOP_AXI_PROT_EN_MM_VDOSYS0		GENMASK(21, 17)
#define MT8195_TOP_AXI_PROT_EN_MM_VPPSYS1		GENMASK(8, 5)
#define MT8195_TOP_AXI_PROT_EN_MM_VENC			(BIT(9) | BIT(11))
#define MT8195_TOP_AXI_PROT_EN_MM_VENC_CORE1		(BIT(10) | BIT(12))
#define MT8195_TOP_AXI_PROT_EN_MM_VDEC0			BIT(13)
#define MT8195_TOP_AXI_PROT_EN_MM_VDEC1			BIT(14)
#define MT8195_TOP_AXI_PROT_EN_MM_VDOSYS1_2ND		BIT(22)
#define MT8195_TOP_AXI_PROT_EN_MM_VPPSYS1_2ND		BIT(23)
#define MT8195_TOP_AXI_PROT_EN_MM_CAM_2ND		BIT(24)
#define MT8195_TOP_AXI_PROT_EN_MM_IMG_2ND		BIT(25)
#define MT8195_TOP_AXI_PROT_EN_MM_VENC_2ND		BIT(26)
#define MT8195_TOP_AXI_PROT_EN_MM_WPESYS		BIT(27)
#define MT8195_TOP_AXI_PROT_EN_MM_VDEC0_2ND		BIT(28)
#define MT8195_TOP_AXI_PROT_EN_MM_VDEC1_2ND		BIT(29)
#define MT8195_TOP_AXI_PROT_EN_MM_VDOSYS1		GENMASK(31, 30)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS0_2ND		(GENMASK(1, 0) | BIT(4) | BIT(11))
#define MT8195_TOP_AXI_PROT_EN_MM_2_VENC		BIT(2)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VENC_CORE1		(BIT(3) | BIT(15))
#define MT8195_TOP_AXI_PROT_EN_MM_2_CAM			(BIT(5) | BIT(17))
#define MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS1		(GENMASK(7, 6) | BIT(18))
#define MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS0		GENMASK(9, 8)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VDOSYS1		BIT(10)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VDEC2_2ND		BIT(12)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VDEC0_2ND		BIT(13)
#define MT8195_TOP_AXI_PROT_EN_MM_2_WPESYS_2ND		BIT(14)
#define MT8195_TOP_AXI_PROT_EN_MM_2_IPE			BIT(16)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VDEC2		BIT(21)
#define MT8195_TOP_AXI_PROT_EN_MM_2_VDEC0		BIT(22)
#define MT8195_TOP_AXI_PROT_EN_MM_2_WPESYS		GENMASK(24, 23)
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_EPD_TX		BIT(1)
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_DP_TX		BIT(2)
#define MT8195_TOP_AXI_PROT_EN_VDNR_PCIE_MAC_P0		(BIT(11) | BIT(28))
#define MT8195_TOP_AXI_PROT_EN_VDNR_PCIE_MAC_P1		(BIT(12) | BIT(29))
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_PCIE_MAC_P0	BIT(13)
#define MT8195_TOP_AXI_PROT_EN_VDNR_1_PCIE_MAC_P1	BIT(14)
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MFG1	(BIT(17) | BIT(19))
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VPPSYS0	BIT(20)
#define MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VDOSYS0	BIT(21)

#define _BUS_PROT(_mask, _sta_mask, _set, _clr, _sta, _update, _ignore, _wayen) {	\
		.bus_prot_mask = (_mask),				\
		.bus_prot_set = _set,					\
		.bus_prot_clr = _clr,					\
		.bus_prot_sta = _sta,					\
		.bus_prot_sta_mask = _sta_mask,				\
		.bus_prot_reg_update = _update,				\
		.ignore_clr_ack = _ignore,				\
		.wayen = _wayen,					\
	}

#define BUS_PROT_WR(_mask, _set, _clr, _sta)			\
		_BUS_PROT(_mask, _mask, _set, _clr, _sta, false, false, false)

enum scp_domain_type {
	SCPSYS_MT7622,
	SCPSYS_MT7623,
	SCPSYS_MT7629,
	SCPSYS_MT8188,
	SCPSYS_MT8195,
	SCPSYS_MT8365,
};

struct scp_domain;

struct scpsys_bus_prot_data {
	u32 bus_prot_mask;
	u32 bus_prot_set;
	u32 bus_prot_clr;
	u32 bus_prot_sta;
	u32 bus_prot_sta_mask;
	bool bus_prot_reg_update;
	bool ignore_clr_ack;
	bool wayen;
};

struct scp_domain_data {
	struct scp_domain *scpd;
	u32 sta_mask;
	int ctl_offs;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	u32 bus_prot_mask;
	const struct scpsys_bus_prot_data bp_infracfg[SPM_MAX_BUS_PROT_DATA];
	int pwr_sta_offs;
	int pwr_sta2nd_offs;
	int parent_id;
	struct clk_bulk clks;
	struct clk_bulk subsys_clks;
	int has_pd;
	struct power_domain parent_pd;
};

struct scp_domain {
	struct udevice *dev;
	void __iomem *base;
	void __iomem *infracfg;
	enum scp_domain_type type;
	struct scp_domain_data *data;
	int num_domains;
};

static struct scp_domain_data scp_domain_mt7623[] = {
	[MT7623_POWER_DOMAIN_CONN] = {
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = SPM_CONN_PWR_CON,
		.bus_prot_mask = BIT(8) | BIT(2),
	},
	[MT7623_POWER_DOMAIN_DISP] = {
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.bus_prot_mask = BIT(2),
	},
	[MT7623_POWER_DOMAIN_MFG] = {
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT7623_POWER_DOMAIN_VDEC] = {
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT7623_POWER_DOMAIN_ISP] = {
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT7623_POWER_DOMAIN_BDP] = {
		.sta_mask = PWR_STATUS_BDP,
		.ctl_offs = SPM_BDP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
	},
	[MT7623_POWER_DOMAIN_ETH] = {
		.sta_mask = PWR_STATUS_ETH,
		.ctl_offs = SPM_ETH_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT7623_POWER_DOMAIN_HIF] = {
		.sta_mask = PWR_STATUS_HIF,
		.ctl_offs = SPM_HIF_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT7623_POWER_DOMAIN_IFR_MSC] = {
		.sta_mask = PWR_STATUS_IFR_MSC,
		.ctl_offs = SPM_IFR_MSC_PWR_CON,
	},
};

static struct scp_domain_data scp_domain_mt7629[] = {
	[MT7629_POWER_DOMAIN_ETHSYS] = {
		.sta_mask = PWR_STATUS_ETHSYS,
		.ctl_offs = SPM_ETHSYS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = (BIT(3) | BIT(17)),
	},
	[MT7629_POWER_DOMAIN_HIF0] = {
		.sta_mask = PWR_STATUS_HIF0,
		.ctl_offs = SPM_HIF0_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = GENMASK(25, 24),
	},
	[MT7629_POWER_DOMAIN_HIF1] = {
		.sta_mask = PWR_STATUS_HIF1,
		.ctl_offs = SPM_HIF1_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = GENMASK(28, 26),
	},
};

static struct scp_domain_data scp_domain_mt8188[] = {
	[MT8188_POWER_DOMAIN_MFG0] = {
		.sta_mask = BIT(1),
		.ctl_offs = 0x300,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_MFG1] = {
		.sta_mask = BIT(2),
		.ctl_offs = 0x304,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MFG1_STEP1,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_MFG1_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_1_MFG1_STEP3,
				    MT8188_TOP_AXI_PROT_EN_1_SET,
				    MT8188_TOP_AXI_PROT_EN_1_CLR,
				    MT8188_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_MFG1_STEP4,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MFG1_STEP5,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MFG1_STEP6,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_MFG2] = {
		.sta_mask = BIT(3),
		.ctl_offs = 0x308,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_MFG3] = {
		.sta_mask = BIT(4),
		.ctl_offs = 0x30C,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_MFG4] = {
		.sta_mask = BIT(5),
		.ctl_offs = 0x310,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_PEXTP_MAC_P0] = {
		.sta_mask = BIT(10),
		.ctl_offs = 0x324,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_PEXTP_MAC_P0_STEP1,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_PEXTP_MAC_P0_STEP2,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_PEXTP_PHY_TOP] = {
		.sta_mask = BIT(12),
		.ctl_offs = 0x328,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
	},
	[MT8188_POWER_DOMAIN_CSIRX_TOP] = {
		.sta_mask = BIT(17),
		.ctl_offs = 0x3C4,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
	},
	[MT8188_POWER_DOMAIN_ETHER] = {
		.sta_mask = BIT(1),
		.ctl_offs = 0x338,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_ETHER_STEP1,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_HDMI_TX] = {
		.sta_mask = BIT(18),
		.ctl_offs = 0x37C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_HDMI_TX_STEP1,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_ADSP_AO] = {
		.sta_mask = BIT(10),
		.ctl_offs = 0x35C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_AO_STEP1,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_AO_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_ADSP_INFRA] = {
		.sta_mask = BIT(9),
		.ctl_offs = 0x358,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_INFRA_STEP1,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_INFRA_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_ADSP] = {
		.sta_mask = BIT(8),
		.ctl_offs = 0x354,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_STEP1,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_ADSP_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_AUDIO] = {
		.sta_mask = BIT(6),
		.ctl_offs = 0x34C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP1,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_AUDIO_ASRC] = {
		.sta_mask = BIT(7),
		.ctl_offs = 0x350,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_AUDIO_ASRC_STEP1,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_AUDIO_ASRC_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VPPSYS0] = {
		.sta_mask = BIT(11),
		.ctl_offs = 0x360,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_VPPSYS0_STEP1,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS0_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_VPPSYS0_STEP3,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS0_STEP4,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VPPSYS0_STEP5,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VDOSYS0] = {
		.sta_mask = BIT(13),
		.ctl_offs = 0x368,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDOSYS0_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_VDOSYS0_STEP2,
				    MT8188_TOP_AXI_PROT_EN_SET,
				    MT8188_TOP_AXI_PROT_EN_CLR,
				    MT8188_TOP_AXI_PROT_EN_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VDOSYS0_STEP3,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VDOSYS1] = {
		.sta_mask = BIT(14),
		.ctl_offs = 0x36C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDOSYS1_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDOSYS1_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VDOSYS1_STEP3,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_DP_TX] = {
		.sta_mask = BIT(16),
		.ctl_offs = 0x374,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_DP_TX_STEP1,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_EDP_TX] = {
		.sta_mask = BIT(17),
		.ctl_offs = 0x378,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_EDP_TX_STEP1,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_SET,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_CLR,
				    MT8188_TOP_AXI_PROT_EN_INFRA_VDNR_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VPPSYS1] = {
		.sta_mask = BIT(12),
		.ctl_offs = 0x364,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VPPSYS1_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VPPSYS1_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VPPSYS1_STEP3,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_WPE] = {
		.sta_mask = BIT(15),
		.ctl_offs = 0x370,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_WPE_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_WPE_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VDEC0] = {
		.sta_mask = BIT(19),
		.ctl_offs = 0x380,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDEC0_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VDEC0_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VDEC1] = {
		.sta_mask = BIT(20),
		.ctl_offs = 0x384,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDEC1_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VDEC1_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
		},
	},
	[MT8188_POWER_DOMAIN_VENC] = {
		.sta_mask = BIT(22),
		.ctl_offs = 0x38C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VENC_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_VENC_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_VENC_STEP3,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_IMG_VCORE] = {
		.sta_mask = BIT(28),
		.ctl_offs = 0x3A4,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_IMG_VCORE_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_IMG_VCORE_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_IMG_VCORE_STEP3,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_IMG_MAIN] = {
		.sta_mask = BIT(29),
		.ctl_offs = 0x3A8,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_IMG_MAIN_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_IMG_MAIN_STEP2,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_DIP] = {
		.sta_mask = BIT(30),
		.ctl_offs = 0x3AC,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_IPE] = {
		.sta_mask = BIT(31),
		.ctl_offs = 0x3B0,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_CAM_VCORE] = {
		.sta_mask = BIT(27),
		.ctl_offs = 0x3A0,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_CAM_VCORE_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_CAM_VCORE_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_1_CAM_VCORE_STEP3,
				    MT8188_TOP_AXI_PROT_EN_1_SET,
				    MT8188_TOP_AXI_PROT_EN_1_CLR,
				    MT8188_TOP_AXI_PROT_EN_1_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_CAM_VCORE_STEP4,
				    MT8188_TOP_AXI_PROT_EN_MM_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_CAM_VCORE_STEP5,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_CAM_MAIN] = {
		.sta_mask = BIT(24),
		.ctl_offs = 0x394,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_CAM_MAIN_STEP1,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_CAM_MAIN_STEP2,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_MM_2_CAM_MAIN_STEP3,
				    MT8188_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8188_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_MM_2_STA),
			BUS_PROT_WR(MT8188_TOP_AXI_PROT_EN_2_CAM_MAIN_STEP4,
				    MT8188_TOP_AXI_PROT_EN_2_SET,
				    MT8188_TOP_AXI_PROT_EN_2_CLR,
				    MT8188_TOP_AXI_PROT_EN_2_STA),
		},
	},
	[MT8188_POWER_DOMAIN_CAM_SUBA] = {
		.sta_mask = BIT(25),
		.ctl_offs = 0x398,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT8188_POWER_DOMAIN_CAM_SUBB] = {
		.sta_mask = BIT(26),
		.ctl_offs = 0x39C,
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
};

static struct scp_domain_data scp_domain_mt8195[] = {
	[MT8195_POWER_DOMAIN_PCIE_MAC_P0] = {
		.sta_mask = BIT(11),
		.ctl_offs = 0x328,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_PCIE_MAC_P0,
				    MT8195_TOP_AXI_PROT_EN_VDNR_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_1_PCIE_MAC_P0,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_PCIE_MAC_P1] = {
		.sta_mask = BIT(12),
		.ctl_offs = 0x32C,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_PCIE_MAC_P1,
				    MT8195_TOP_AXI_PROT_EN_VDNR_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_1_PCIE_MAC_P1,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_PCIE_PHY] = {
		.sta_mask = BIT(13),
		.ctl_offs = 0x330,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
	},
	[MT8195_POWER_DOMAIN_SSUSB_PCIE_PHY] = {
		.sta_mask = BIT(14),
		.ctl_offs = 0x334,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
	},
	[MT8195_POWER_DOMAIN_CSI_RX_TOP] = {
		.sta_mask = BIT(18),
		.ctl_offs = 0x3C4,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
	},
	[MT8195_POWER_DOMAIN_ETHER] = {
		.sta_mask = BIT(3),
		.ctl_offs = 0x344,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_ADSP] = {
		.sta_mask = BIT(10),
		.ctl_offs = 0x360,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_2_ADSP,
				    MT8195_TOP_AXI_PROT_EN_2_SET,
				    MT8195_TOP_AXI_PROT_EN_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_AUDIO] = {
		.sta_mask = BIT(8),
		.ctl_offs = 0x358,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_2_AUDIO,
				    MT8195_TOP_AXI_PROT_EN_2_SET,
				    MT8195_TOP_AXI_PROT_EN_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_MFG0] = {
		.sta_mask = BIT(1),
		.ctl_offs = 0x300,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_MFG1] = {
		.sta_mask = BIT(2),
		.ctl_offs = 0x304,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MFG1,
				    MT8195_TOP_AXI_PROT_EN_SET,
				    MT8195_TOP_AXI_PROT_EN_CLR,
				    MT8195_TOP_AXI_PROT_EN_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_2_MFG1,
				    MT8195_TOP_AXI_PROT_EN_2_SET,
				    MT8195_TOP_AXI_PROT_EN_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_1_MFG1,
				    MT8195_TOP_AXI_PROT_EN_1_SET,
				    MT8195_TOP_AXI_PROT_EN_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_1_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_2_MFG1_2ND,
				    MT8195_TOP_AXI_PROT_EN_2_SET,
				    MT8195_TOP_AXI_PROT_EN_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MFG1_2ND,
				    MT8195_TOP_AXI_PROT_EN_SET,
				    MT8195_TOP_AXI_PROT_EN_CLR,
				    MT8195_TOP_AXI_PROT_EN_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MFG1,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_MFG2] = {
		.sta_mask = BIT(3),
		.ctl_offs = 0x308,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_MFG3] = {
		.sta_mask = BIT(4),
		.ctl_offs = 0x30C,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_MFG4] = {
		.sta_mask = BIT(5),
		.ctl_offs = 0x310,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_MFG5] = {
		.sta_mask = BIT(6),
		.ctl_offs = 0x314,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_MFG6] = {
		.sta_mask = BIT(7),
		.ctl_offs = 0x318,
		.pwr_sta_offs = 0x174,
		.pwr_sta2nd_offs = 0x178,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_VPPSYS0] = {
		.sta_mask = BIT(11),
		.ctl_offs = 0x364,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VPPSYS0,
				    MT8195_TOP_AXI_PROT_EN_SET,
				    MT8195_TOP_AXI_PROT_EN_CLR,
				    MT8195_TOP_AXI_PROT_EN_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS0,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VPPSYS0_2ND,
				    MT8195_TOP_AXI_PROT_EN_SET,
				    MT8195_TOP_AXI_PROT_EN_CLR,
				    MT8195_TOP_AXI_PROT_EN_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS0_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VPPSYS0,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VDOSYS0] = {
		.sta_mask = BIT(13),
		.ctl_offs = 0x36C,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDOSYS0,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDOSYS0,
				    MT8195_TOP_AXI_PROT_EN_SET,
				    MT8195_TOP_AXI_PROT_EN_CLR,
				    MT8195_TOP_AXI_PROT_EN_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_VDOSYS0,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_SET,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_CLR,
				    MT8195_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VPPSYS1] = {
		.sta_mask = BIT(12),
		.ctl_offs = 0x368,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VPPSYS1,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VPPSYS1_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VPPSYS1,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VDOSYS1] = {
		.sta_mask = BIT(14),
		.ctl_offs = 0x370,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDOSYS1,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDOSYS1_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VDOSYS1,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_DP_TX] = {
		.sta_mask = BIT(16),
		.ctl_offs = 0x378,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_1_DP_TX,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_EPD_TX] = {
		.sta_mask = BIT(17),
		.ctl_offs = 0x37C,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_VDNR_1_EPD_TX,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_SET,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_VDNR_1_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_HDMI_TX] = {
		.sta_mask = BIT(18),
		.ctl_offs = 0x380,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_HDMI_RX] = {
		.sta_mask = BIT(19),
		.ctl_offs = 0x384,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_WPESYS] = {
		.sta_mask = BIT(15),
		.ctl_offs = 0x374,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_WPESYS,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_WPESYS,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_WPESYS_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VDEC0] = {
		.sta_mask = BIT(20),
		.ctl_offs = 0x388,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDEC0,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VDEC0,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDEC0_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VDEC0_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VDEC1] = {
		.sta_mask = BIT(21),
		.ctl_offs = 0x38C,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDEC1,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VDEC1_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VDEC2] = {
		.sta_mask = BIT(22),
		.ctl_offs = 0x390,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VDEC2,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VDEC2_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VENC] = {
		.sta_mask = BIT(23),
		.ctl_offs = 0x394,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VENC,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VENC_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VENC,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_VENC_CORE1] = {
		.sta_mask = BIT(24),
		.ctl_offs = 0x398,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_VENC_CORE1,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_VENC_CORE1,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_IMG] = {
		.sta_mask = BIT(29),
		.ctl_offs = 0x3AC,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_IMG,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_IMG_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_DIP] = {
		.sta_mask = BIT(30),
		.ctl_offs = 0x3B0,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_IPE] = {
		.sta_mask = BIT(31),
		.ctl_offs = 0x3B4,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_IPE,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_IPE,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_CAM] = {
		.sta_mask = BIT(25),
		.ctl_offs = 0x39C,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_infracfg = {
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_2_CAM,
				    MT8195_TOP_AXI_PROT_EN_2_SET,
				    MT8195_TOP_AXI_PROT_EN_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_2_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_CAM,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_1_CAM,
				    MT8195_TOP_AXI_PROT_EN_1_SET,
				    MT8195_TOP_AXI_PROT_EN_1_CLR,
				    MT8195_TOP_AXI_PROT_EN_1_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_CAM_2ND,
				    MT8195_TOP_AXI_PROT_EN_MM_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_STA1),
			BUS_PROT_WR(MT8195_TOP_AXI_PROT_EN_MM_2_CAM,
				    MT8195_TOP_AXI_PROT_EN_MM_2_SET,
				    MT8195_TOP_AXI_PROT_EN_MM_2_CLR,
				    MT8195_TOP_AXI_PROT_EN_MM_2_STA1),
		},
	},
	[MT8195_POWER_DOMAIN_CAM_RAWA] = {
		.sta_mask = BIT(26),
		.ctl_offs = 0x3A0,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_CAM_RAWB] = {
		.sta_mask = BIT(27),
		.ctl_offs = 0x3A4,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8195_POWER_DOMAIN_CAM_MRAW] = {
		.sta_mask = BIT(28),
		.ctl_offs = 0x3A8,
		.pwr_sta_offs = 0x16c,
		.pwr_sta2nd_offs = 0x170,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
};

static struct scp_domain_data scp_domain_mt8365[] = {
	[MT8365_POWER_DOMAIN_MM] = {
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = 0x30c,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8365_POWER_DOMAIN_CONN] = {
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = 0x032c,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
	},
	[MT8365_POWER_DOMAIN_MFG] = {
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = 0x0338,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT8365_POWER_DOMAIN_AUDIO] = {
		.sta_mask = BIT(24),
		.ctl_offs = 0x0314,
		.sram_pdn_bits = GENMASK(12, 8),
		.sram_pdn_ack_bits = GENMASK(17, 13),
	},
	[MT8365_POWER_DOMAIN_CAM] = {
		.sta_mask = BIT(25),
		.ctl_offs = 0x0344,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT8365_POWER_DOMAIN_DSP] = {
		.sta_mask = BIT(17),
		.ctl_offs = 0x037C,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT8365_POWER_DOMAIN_VDEC] = {
		.sta_mask = BIT(31),
		.ctl_offs = 0x0370,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8365_POWER_DOMAIN_VENC] = {
		.sta_mask = BIT(21),
		.ctl_offs = 0x0304,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT8365_POWER_DOMAIN_APU] = {
		.sta_mask = BIT(16),
		.ctl_offs = 0x0378,
		.sram_pdn_bits = GENMASK(14, 8),
		.sram_pdn_ack_bits = GENMASK(21, 15),
	},
};

/**
 * This function enables the bus protection bits for disabled power
 * domains so that the system does not hang when some unit accesses the
 * bus while in power down.
 */
static int mtk_infracfg_set_bus_protection(void __iomem *infracfg,
					   u32 mask)
{
	u32 val;

	clrsetbits_le32(infracfg + INFRA_TOPAXI_PROT_EN, mask, mask);

	return readl_poll_timeout(infracfg + INFRA_TOPAXI_PROT_STA1, val,
				  (val & mask) == mask, 100);
}

static int mtk_infracfg_clear_bus_protection(void __iomem *infracfg,
					     u32 mask)
{
	u32 val;

	clrbits_le32(infracfg + INFRA_TOPAXI_PROT_EN, mask);

	return readl_poll_timeout(infracfg + INFRA_TOPAXI_PROT_STA1, val,
				  !(val & mask), 100);
}

static int _scpsys_bus_protect_enable(const struct scpsys_bus_prot_data bpd,
				      void __iomem *reg)
{
	int ret;
	u32 val = 0, mask = bpd.bus_prot_mask;

	if (!mask)
		return 0;

	if (bpd.bus_prot_reg_update)
		clrsetbits_le32(reg + bpd.bus_prot_set, mask, mask);
	else
		writel(mask, reg + bpd.bus_prot_set);

	ret = readl_poll_timeout(reg + bpd.bus_prot_sta, val, (val & mask) == mask, 1000);
	if (ret)
		return ret;

	return 0;
}

static int _scpsys_bus_protect_disable(const struct scpsys_bus_prot_data bpd,
				       void __iomem *reg)
{
	int ret;
	u32 val = 0, mask = bpd.bus_prot_mask;

	if (!mask)
		return 0;

	if (bpd.bus_prot_reg_update)
		clrbits_le32(reg + bpd.bus_prot_clr, mask);
	else
		writel(mask, reg + bpd.bus_prot_clr);

	if (bpd.ignore_clr_ack)
		return 0;

	ret = readl_poll_timeout(reg + bpd.bus_prot_sta, val, !(val & mask), 1000);
	if (ret)
		return ret;

	return 0;
}

static int scpsys_bus_protect_enable(const struct scpsys_bus_prot_data *bpd, int bpd_size,
				     void __iomem *reg)
{
	int ret, i;

	for (i = 0; i < bpd_size; i++) {
		ret = _scpsys_bus_protect_enable(bpd[i], reg);
		if (ret)
			return ret;
	}

	return 0;
}

static int scpsys_bus_protect_disable(const struct scpsys_bus_prot_data *bpd, int bpd_size,
				      void __iomem *reg)
{
	int i, ret;

	for (i = bpd_size - 1; i >= 0; i--) {
		ret = _scpsys_bus_protect_disable(bpd[i], reg);
		if (ret)
			return ret;
	}

	return 0;
}

static int scpsys_domain_is_on(struct scp_domain_data *data)
{
	struct scp_domain *scpd = data->scpd;
	u32 spm_pwr_status, spm_pwr_status_2nd;
	u32 sta, sta2;

	if (data->pwr_sta_offs) {
		spm_pwr_status = data->pwr_sta_offs;
		spm_pwr_status_2nd = data->pwr_sta2nd_offs;
	} else if (data->scpd->type == SCPSYS_MT8365) {
		spm_pwr_status = MT8365_SPM_PWR_STATUS;
		spm_pwr_status_2nd = MT8365_SPM_PWR_STATUS_2ND;
	} else {
		spm_pwr_status = SPM_PWR_STATUS;
		spm_pwr_status_2nd = SPM_PWR_STATUS_2ND;
	}

	sta = readl(scpd->base + spm_pwr_status) & data->sta_mask;
	sta2 = readl(scpd->base + spm_pwr_status_2nd) & data->sta_mask;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */
	if (sta && sta2)
		return true;
	if (!sta && !sta2)
		return false;

	return -EINVAL;
}

static int scpsys_power_on(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data = &scpd->data[power_domain->id];
	void __iomem *ctl_addr = scpd->base + data->ctl_offs;
	u32 pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret, tmp;

	if (data->has_pd)
		power_domain_on(&data->parent_pd);

	ret = clk_enable_bulk(&data->clks);
	if (ret)
		return ret;

	writel(SPM_EN, scpd->base);

	val = readl(ctl_addr);
	val |= PWR_ON_BIT;
	writel(val, ctl_addr);

	val |= PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	ret = readx_poll_timeout(scpsys_domain_is_on, data, tmp, tmp > 0,
				 100);
	if (ret < 0)
		return ret;

	val &= ~PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ISO_BIT;
	writel(val, ctl_addr);

	val |= PWR_RST_B_BIT;
	writel(val, ctl_addr);

	ret = clk_enable_bulk(&data->subsys_clks);
	if (ret)
		return ret;

	val &= ~data->sram_pdn_bits;
	writel(val, ctl_addr);

	ret = readl_poll_timeout(ctl_addr, tmp, !(tmp & pdn_ack), 100);
	if (ret < 0)
		return ret;

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_clear_bus_protection(scpd->infracfg,
							data->bus_prot_mask);
		if (ret)
			return ret;
	}
	ret = scpsys_bus_protect_disable(data->bp_infracfg, SPM_MAX_BUS_PROT_DATA, scpd->infracfg);
	if (ret < 0)
		return ret;

	return 0;
}

static int scpsys_power_off(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data = &scpd->data[power_domain->id];
	void __iomem *ctl_addr = scpd->base + data->ctl_offs;
	u32 pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret, tmp;

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_set_bus_protection(scpd->infracfg,
						      data->bus_prot_mask);
		if (ret)
			return ret;
	}

	ret = scpsys_bus_protect_enable(data->bp_infracfg, SPM_MAX_BUS_PROT_DATA, scpd->infracfg);
	if (ret < 0)
		return ret;

	val = readl(ctl_addr);
	val |= data->sram_pdn_bits;
	writel(val, ctl_addr);

	ret = readl_poll_timeout(ctl_addr, tmp, (tmp & pdn_ack) == pdn_ack,
				 100);
	if (ret < 0)
		return ret;

	ret = clk_disable_bulk(&data->subsys_clks);
	if (ret)
		return ret;

	val |= PWR_ISO_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val |= PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	ret = readx_poll_timeout(scpsys_domain_is_on, data, tmp, !tmp, 100);
	if (ret < 0)
		return ret;

	ret = clk_disable_bulk(&data->clks);
	if (ret)
		return ret;

	return 0;
}

static int scpsys_power_request(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data;

	data = &scpd->data[power_domain->id];
	data->scpd = scpd;

	return 0;
}

static int scpsys_add_one_domain(struct scp_domain *scpsys, ofnode node, int parent_id)
{
	struct scp_domain_data *domain_data;
	const char *clk_name;
	int i, ret, num_clks;
	int clk_ind = 0;
	u32 id;

	ret = ofnode_read_u32(node, "reg", &id);
	if (ret) {
		dev_err(scpsys->dev, "%s: failed to retrieve domain id from reg: %d\n",
			ofnode_get_name(node), ret);
		return -EINVAL;
	}

	if (id >= scpsys->num_domains) {
		dev_err(scpsys->dev, "%s: invalid domain id %d\n", ofnode_get_name(node), id);
		return -EINVAL;
	}

	domain_data = &scpsys->data[id];
	domain_data->parent_id = parent_id;
	domain_data->scpd = scpsys;

	if (parent_id >= 0) {
		domain_data->has_pd = 1;
		domain_data->parent_pd.dev = scpsys->dev;
		domain_data->parent_pd.id = parent_id;
	}

	num_clks = 0;
	while (1) {
		char *subsys;

		ret = ofnode_read_string_index(node, "clock-names", num_clks, &clk_name);
		if (!clk_name)
			break;

		subsys = strchr(clk_name, '-');
		if (subsys)
			domain_data->subsys_clks.count++;
		else
			domain_data->clks.count++;

		num_clks++;
	}

	domain_data->clks.clks = devm_kcalloc(scpsys->dev, domain_data->clks.count,
					      sizeof(struct clk), GFP_KERNEL);
	if (!domain_data->clks.clks)
		return -ENOMEM;

	domain_data->subsys_clks.clks = devm_kcalloc(scpsys->dev, domain_data->subsys_clks.count,
						     sizeof(struct clk), GFP_KERNEL);
	if (!domain_data->subsys_clks.clks)
		return -ENOMEM;

	for (i = 0; i < domain_data->clks.count; i++) {
		ret = clk_get_by_index_nodev(node, i, &domain_data->clks.clks[i]);
		if (ret < 0) {
			dev_err(scpsys->dev, "%s: failed to get clk at index %d: %d\n",
				ofnode_get_name(node), i, ret);
			goto err_put_clocks;
		}
		clk_ind++;
	}

	for (i = 0; i < domain_data->subsys_clks.count; i++) {
		ret = clk_get_by_index_nodev(node, i + clk_ind,
					     &domain_data->subsys_clks.clks[i]);
		if (ret < 0) {
			dev_err(scpsys->dev, "%s: failed to get subsys clk at index %d: %d\n",
				ofnode_get_name(node), i + clk_ind, ret);
			goto err_put_subsys_clocks;
		}
	}

	return 0;

err_put_subsys_clocks:
	clk_release_all(domain_data->subsys_clks.clks, domain_data->subsys_clks.count);
err_put_clocks:
	clk_release_all(domain_data->clks.clks, domain_data->clks.count);
	return ret;
}

static int scpsys_add_subdomain(struct scp_domain *scpsys, ofnode node)
{
	ofnode subnode;
	int ret;
	u32 id;

	ret = ofnode_read_u32(node, "reg", &id);
	if (ret) {
		dev_err(scpsys->dev, "%s: failed to get domain id\n", ofnode_get_name(node));
		goto err_put_node;
	}

	ofnode_for_each_subnode(subnode, node) {
		ret = scpsys_add_one_domain(scpsys, subnode, id);
		if (ret) {
			dev_err(scpsys->dev, "failed to add child domain: %s\n",
				ofnode_get_name(subnode));
			continue;
		}

		/* recursive call to add all subdomains */
		ret = scpsys_add_subdomain(scpsys, subnode);
		if (ret)
			goto err_put_node;
	}

	return 0;

err_put_node:
	return ret;
}

static int mtk_power_domain_hook(struct udevice *dev)
{
	struct scp_domain *scpd = dev_get_priv(dev);

	scpd->type = (enum scp_domain_type)dev_get_driver_data(dev);

	switch (scpd->type) {
	case SCPSYS_MT7623:
		scpd->data = scp_domain_mt7623;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt7623);
		break;
	case SCPSYS_MT7622:
	case SCPSYS_MT7629:
		scpd->data = scp_domain_mt7629;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt7629);
		break;
	case SCPSYS_MT8188:
		scpd->data = scp_domain_mt8188;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt8188);
		break;
	case SCPSYS_MT8195:
		scpd->data = scp_domain_mt8195;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt8195);
		break;
	case SCPSYS_MT8365:
		scpd->data = scp_domain_mt8365;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt8365);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_power_domain_probe(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	struct scp_domain *scpd = dev_get_priv(dev);
	struct regmap *regmap;
	struct clk_bulk bulk;
	int err, ret;
	ofnode subnode;

	scpd->dev = dev;
	scpd->base = dev_read_addr_ptr(dev);
	if (!scpd->base)
		return -ENOENT;

	err = mtk_power_domain_hook(dev);
	if (err)
		return err;

	/* get corresponding syscon phandle */
	err = dev_read_phandle_with_args(dev, "infracfg", NULL, 0, 0, &args);
	if (!err) {
		regmap = syscon_node_to_regmap(args.node);
		if (IS_ERR(regmap))
			return PTR_ERR(regmap);

		scpd->infracfg = regmap_get_range(regmap, 0);
		if (!scpd->infracfg)
			return -ENOENT;
	}

	/* enable Infra DCM */
	setbits_le32(scpd->infracfg + INFRA_TOPDCM_CTRL, DCM_TOP_EN);

	err = clk_get_bulk(dev, &bulk);
	if (!err)
		clk_enable_bulk(&bulk);

	dev_for_each_subnode(subnode, dev) {
		ret = scpsys_add_one_domain(scpd, subnode, -1);
		if (ret) {
			dev_err(scpd->dev, "failed to add child domain: %s\n",
				ofnode_get_name(subnode));
			continue;
		}

		ret = scpsys_add_subdomain(scpd, subnode);
		if (ret) {
			dev_err(scpd->dev, "failed to add sub domain: %s\n",
				ofnode_get_name(subnode));
			return ret;
		}
	}

	return 0;
}

static const struct udevice_id mtk_power_domain_ids[] = {
	{
		.compatible = "mediatek,mt7622-scpsys",
		.data = SCPSYS_MT7622,
	},
	{
		.compatible = "mediatek,mt7623-scpsys",
		.data = SCPSYS_MT7623,
	},
	{
		.compatible = "mediatek,mt7629-scpsys",
		.data = SCPSYS_MT7629,
	},
	{
		.compatible = "mediatek,mt8188-scpsys",
		.data = SCPSYS_MT8188,
	},
	{
		.compatible = "mediatek,mt8195-scpsys",
		.data = SCPSYS_MT8195,
	},
	{
		.compatible = "mediatek,mt8365-scpsys",
		.data = SCPSYS_MT8365,
	},
	{ /* sentinel */ }
};

struct power_domain_ops mtk_power_domain_ops = {
	.off = scpsys_power_off,
	.on = scpsys_power_on,
	.request = scpsys_power_request,
};

U_BOOT_DRIVER(mtk_power_domain) = {
	.name = "mtk_power_domain",
	.id = UCLASS_POWER_DOMAIN,
	.ops = &mtk_power_domain_ops,
	.probe = mtk_power_domain_probe,
	.of_match = mtk_power_domain_ids,
	.priv_auto	= sizeof(struct scp_domain),
};

/* SPDX-License-Identifier: GPL-2.0
 *
 * MediaTek MT8390 EVK board header
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Tommy Chen <tommyyl.chen@mediatek.com>
 */

#ifndef _MT8390_EVK_BOARD_H_
#define _MT8390_EVK_BOARD_H_
#endif

#define MT8390_BOARD_ID_ADC_CHANNEL	1
#define MT8390_ADC_NAME	"adc@11002000"
#define MT8390_P1V4_DSI_DTS	"#conf-display-dsi-p1v4.dtbo"
#define MT8390_P1V4_DSI_DTS_EFI	"display-dsi-p1v4.dtbo"
#define MT8390_P1V4_THRESH	1100

enum MT8390_EVK_BOARD_ID {
	MT8390_EVK_BOARD_LEGACY,
	MT8390_EVK_BOARD_P1V4,
};

void panel_get_desc_kd070fhfid015(struct panel_description **panel_desc);
void panel_get_desc_kd070fhfid078(struct panel_description **panel_desc);

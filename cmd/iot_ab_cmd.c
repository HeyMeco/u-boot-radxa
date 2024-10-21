// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Howard Lin <howard-yh.lin@mediatek.com>
 */

#include <common.h>
#include <command.h>
#include <iot_ab.h>

static int do_iot_ab_cmd(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	if (argc != 1)
		return CMD_RET_USAGE;

	iot_ab_boot_select();

	printf("The slot is changed successfully!\n");
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(iot_ab, 1, 0, do_iot_ab_cmd,
	   "Change boot slot to A or B for MediaTek IoT Yocto.",
	   "\n"
	   "    - Change boot slot of boot_ctrl data from metadata on the device.\n"
);

// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <asm-generic/sections.h>
#include <common.h>
#include <command.h>
#include <linux/libfdt.h>

static int do_bootembedded(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	u8 *sp = (u8 *)&_start;
	u8 *ep = (u8 *)&_end;
	u8 *text_end = (u8 *)(CONFIG_SYS_TEXT_BASE + (ep - sp));
	u32 fdtsz = fdt_totalsize(text_end);
	u8 *script_start = text_end + fdtsz;
	s8 cmd[64];

	printf("start: 0x%x, end: 0x%x\n", sp, ep);
	printf("text_start: 0x%x, text_end: 0x%x\n", CONFIG_SYS_TEXT_BASE, text_end);
	printf("fdtsz: 0x%x (%d)\n", fdtsz, fdtsz);
	printf("script: 0x%x\n", script_start);

	sprintf(cmd, "source %p", script_start);
	printf("run '%s'\n", cmd);

	return run_command_list(cmd, -1, 0);
}

U_BOOT_CMD(bootembedded, 1, 1, do_bootembedded,
	   "Run embedded script appended to the end of u-boot binary",
	   ""
);

// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <common.h>
#include <command.h>
#include <part.h>
#include <linux/string.h>
#include <stdlib.h>

#if IS_ENABLED(CONFIG_PARTITION_TYPE_GUID)
#define MTK_FW_PART_GUID "384e979b-eb76-435a-a3a6-1a071dbad91d"

static int listfw(char *ifce, char *devnum, int *parts)
{
	int ret;
	struct blk_desc *desc;
	int p;
	struct disk_partition info;
	int idx = 0;

	ret = blk_get_device_by_str(ifce, devnum, &desc);
	if (ret < 0)
		return 1;

	for (p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
		int r = part_get_info(desc, p, &info);

		if (r != 0)
			continue;

		if (strncmp(info.type_guid, MTK_FW_PART_GUID, 36))
			continue;

		parts[idx] = p;
		idx++;
	}

	parts[idx] = 0;

	return 0;
}
#else
static int listfw(char *ifce, char *devnum, int *parts)
{
	parts[0] = 0;

	return 0;
}
#endif

static int getfw(int part)
{
	char *fw_dtb = env_get("boot_dtb");

	if (fw_dtb) {
		int i = simple_strtol(fw_dtb, NULL, 10);

		if (i == part)
			return CMD_RET_SUCCESS;
		else
			return 1;
	}
	return CMD_RET_SUCCESS;
}

static int exists(char *devtype, char *devnum, int part, char *path)
{
	char cmd[128];

	sprintf(cmd, "test -e %s %s:%d %s", devtype, devnum, part, path);
	if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS)
		return 1;

	return CMD_RET_SUCCESS;
}

static int loaddtb(char *devtype, char *devnum, int part, char *path, char *fdt_addr)
{
	char cmd[128];

	printf("Loading dtb '%s'\n", path);

	sprintf(cmd, "load %s %s:%d %s %s", devtype, devnum, part, fdt_addr, path);
	if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS)
		return 1;

#if IS_ENABLED(CONFIG_CMD_FDTPROBE)
	sprintf(cmd, "fdt authndtb %s", fdt_addr);
	if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS)
		return 1;
#endif

	sprintf(cmd, "fdt addr %s", fdt_addr);
	if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS)
		return 1;

	sprintf(cmd, "fdt resize 8192");
	if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS)
		return 1;

	return CMD_RET_SUCCESS;
}

static int loadoverlays(char *devtype, char *devnum, int part, char *base)
{
	char *overlay_addr = env_get("fdtoverlay_addr_r");
	char *list = env_get("list_dtbo");
	char cmd[128];
	int rcode = CMD_RET_SUCCESS;
	char *copy, *s;

	if (!overlay_addr) {
		printf("'fdtoverlay_addr_r' not set\n");
		return 1;
	}

	if (!list) {
		// Do nothing
		return CMD_RET_SUCCESS;
	}

	copy = strdup(list);
	s = copy;

	while (s) {
		char *o = strsep(&s, " ");

		if (!o || !strcmp(o, ""))
			continue;

		printf("Loading overlay '%s/%s'\n", base, o);
		sprintf(cmd, "load %s %s:%d %s %s/%s", devtype, devnum, part,
			overlay_addr, base, o);
		if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS) {
			rcode = 1;
			goto out;
		}

#if IS_ENABLED(CONFIG_CMD_FDTPROBE)
		sprintf(cmd, "fdt authndtb %s", overlay_addr);
		if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS) {
			rcode = 1;
			goto out;
		}
#endif

		sprintf(cmd, "fdt apply %s", overlay_addr);
		if (run_command_list(cmd, -1, 0) != CMD_RET_SUCCESS) {
			rcode = 1;
			goto out;
		}
	}

out:
	free(copy);

	return rcode;
}

static int do_dtbprobe(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	char *devtype;
	char *devnum;
	char *probe_base;
	char *dtb_file;
	char *fdt_addr;
	int i;
	int parts[MAX_SEARCH_PARTITIONS] = {0};
	char path[128];

	if (argc < 4)
		return CMD_RET_USAGE;

	devtype = argv[1];
	devnum = argv[2];
	probe_base = argv[3];
	dtb_file = env_get("fdtfile");
	if (!dtb_file) {
		printf("'fdtfile' is not set\n");
		return 1;
	}

	fdt_addr = env_get("fdt_addr_r");
	if (!fdt_addr) {
		printf("'fdt_addr_r' is not set\n");
		return 1;
	}

	printf("base: %s\n", probe_base);
	printf("fdtfile: %s\n", dtb_file);
	printf("fdtaddr: %s\n", fdt_addr);

	if (listfw(devtype, devnum, parts)) {
		printf("No dedicated firmware partition found\n");
		return 1;
	}

	sprintf(path, "%s/%s", probe_base, dtb_file);
	for (i = 0; i < MAX_SEARCH_PARTITIONS; i++) {
		int part = parts[i];

		if (part == 0)
			break;

		if (getfw(part) != CMD_RET_SUCCESS)
			continue;

		printf("Probing %s %s:%d for device tree...\n", devtype, devnum, part);

		if (exists(devtype, devnum, part, path) != CMD_RET_SUCCESS)
			continue;

		if (loaddtb(devtype, devnum, part, path, fdt_addr) != CMD_RET_SUCCESS)
			return 1;

		if (loadoverlays(devtype, devnum, part, probe_base) != CMD_RET_SUCCESS)
			return 1;

		return CMD_RET_SUCCESS;
	}

	return 1;
}

#if IS_ENABLED(CONFIG_CMD_FDTPROBE)
U_BOOT_CMD(fdtprobe, CONFIG_SYS_MAXARGS, 1, do_dtbprobe,
	   "Probe and load fdt from given partition list",
	   "<interface> <dev> <probe_base>\n"
	   "<probe_base>: The base directory for probing fdt files\n"
);
#else
U_BOOT_CMD(dtbprobe, CONFIG_SYS_MAXARGS, 1, do_dtbprobe,
	   "Probe and load dtb/dtbo from given partition list",
	   "<interface> <dev> <probe_base>\n"
	   "<probe_base>: The base directory for probing dtb files\n"
);
#endif

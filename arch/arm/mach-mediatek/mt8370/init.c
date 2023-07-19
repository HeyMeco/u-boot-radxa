// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Macpaul Lin <macpaul.lin@mediatek.com>
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/armv8/mmu.h>
#include <asm/sections.h>
#include <asm/system.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	int ret;

	ret = fdtdec_setup_memory_banksize();
	if (ret)
		return ret;

	fdtdec_setup_mem_size_base();

	/*
	 * Limit gd->ram_top not exceeding SZ_4G.
	 * Because some periphals like mmc requires DMA buffer
	 * allocaed below SZ_4G.
	 *
	 * Note: SZ_1M is for adjusting gd->relocaddr,
	 *       the reserved memory for u-boot itself.
	 */
	if (gd->ram_base + gd->ram_size >= SZ_4G)
		gd->mon_len = (gd->ram_base + gd->ram_size + SZ_1M) - SZ_4G;

	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = gd->ram_size;

	return 0;
}

int mtk_pll_early_init(void)
{
	return 0;
}

int mtk_soc_early_init(void)
{
	return 0;
}

#ifndef CONFIG_SYSRESET
void reset_cpu(ulong addr)
{
	psci_system_reset();
}
#endif

int print_cpuinfo(void)
{
	printf("CPU:   MediaTek MT8370\n");
	return 0;
}

static struct mm_region mt8370_mem_map[] = {
	{
		/* DDR */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 0x200000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE,
	}, {
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x20000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		0,
	}
};

struct mm_region *mem_map = mt8370_mem_map;

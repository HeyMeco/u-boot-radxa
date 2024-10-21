// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 BayLibre SAS
 * Author: Fabien Parent <fparent@baylibre.com>
 */

#include <common.h>
#include <dm.h>
#include <efi_loader.h>
#include <iot_ab.h>
#include <net.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/arm-smccc.h>

#define MT8195_UPDATABLE_IMAGES	5

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
static struct efi_fw_image fw_images[MT8195_UPDATABLE_IMAGES] = {0};

struct efi_capsule_update_info update_info = {
#if IS_ENABLED(CONFIG_MEDIATEK_IOT_AB_BOOT_SUPPORT)
#if IS_ENABLED(CONFIG_UFS_MEDIATEK)
	.dfu_string = "ufs 0=bl2.img raw 0x0 0x400 dev 0;"
			"fip.bin part 2 %d;firmware.vfat part 2 %d;u-boot-env.bin raw 0x0 0x400 dev 1",
#else
	.dfu_string = "mmc 0=bl2.img raw 0x0 0x2000 mmcpart 1;"
			"fip.bin part 0 %d;firmware.vfat part 0 %d;u-boot-env.bin raw 0x0 0x2000 mmcpart 2",
#endif
#else
#if IS_ENABLED(CONFIG_DFU_MTD)
	.dfu_string = "mtd nor0=bl2.img part 1;"
			"fip.bin part 2;firmware.vfat part 4;u-boot-env.bin part 9",
#elif defined(CONFIG_UFS_MEDIATEK)
	.dfu_string = "ufs 0=bl2.img raw 0x0 0x400 dev 0;"
			"fip.bin part 2 1;firmware.vfat part 2 3;u-boot-env.bin raw 0x0 0x400 dev 1",
#else
	.dfu_string = "mmc 0=bl2.img raw 0x0 0x2000 mmcpart 1;"
			"fip.bin part 0 1;firmware.vfat part 0 3;u-boot-env.bin raw 0x0 0x2000 mmcpart 2",
#endif
#endif
	.images = fw_images,
};

u8 num_image_type_guids = MT8195_UPDATABLE_IMAGES;
#endif

#if defined(CONFIG_EFI_HAVE_CAPSULE_SUPPORT) && defined(CONFIG_EFI_PARTITION)
enum mt8195_updatable_images {
	MT8195_BL2_IMAGE = 1,
	MT8195_FIP_IMAGE,
	MT8195_FW_IMAGE,
	MT8195_ENV_IMAGE,
	MT8195_FIT_IMAGE,
};

static bool board_is_mt8195_demo(void)
{
	return CONFIG_IS_ENABLED(TARGET_MT8195) &&
		of_machine_is_compatible("mediatek,mt8195-demo");
}

static bool board_is_genio_1200_evk(void)
{
	return CONFIG_IS_ENABLED(TARGET_MT8195) &&
		of_machine_is_compatible("mediatek,genio-1200-evk");
}

static bool board_is_genio_1200_evk_ufs(void)
{
	return CONFIG_IS_ENABLED(TARGET_MT8195) &&
		of_machine_is_compatible("mediatek,genio-1200-evk-ufs");
}

static bool board_is_genio_1200_evk_ufs_qspi(void)
{
	return CONFIG_IS_ENABLED(TARGET_MT8195) &&
		of_machine_is_compatible("mediatek,genio-1200-evk-ufs-qspi");
}

void mediatek_capsule_update_board_setup(void)
{
	fw_images[0].image_index = MT8195_FIT_IMAGE;
	fw_images[1].image_index = MT8195_FIP_IMAGE;
	fw_images[2].image_index = MT8195_BL2_IMAGE;
	fw_images[3].image_index = MT8195_FW_IMAGE;
	fw_images[4].image_index = MT8195_ENV_IMAGE;

	if (board_is_mt8195_demo()) {
		efi_guid_t image_type_guid = MT8195_DEMO_FIT_IMAGE_GUID;
		efi_guid_t uboot_image_type_guid = MT8195_DEMO_FIP_IMAGE_GUID;
		efi_guid_t bl2_image_type_guid = MT8195_DEMO_BL2_IMAGE_GUID;
		efi_guid_t fw_image_type_guid = MT8195_DEMO_FW_IMAGE_GUID;
		efi_guid_t env_image_type_guid = MT8195_DEMO_ENV_IMAGE_GUID;

		guidcpy(&fw_images[0].image_type_id, &image_type_guid);
		guidcpy(&fw_images[1].image_type_id, &uboot_image_type_guid);
		guidcpy(&fw_images[2].image_type_id, &bl2_image_type_guid);
		guidcpy(&fw_images[3].image_type_id, &fw_image_type_guid);
		guidcpy(&fw_images[4].image_type_id, &env_image_type_guid);

		fw_images[0].fw_name = u"MT8195-DEMO-FIT";
		fw_images[1].fw_name = u"MT8195-DEMO-FIP";
		fw_images[2].fw_name = u"MT8195-DEMO-BL2";
		fw_images[3].fw_name = u"MT8195-DEMO-FW";
		fw_images[4].fw_name = u"MT8195-DEMO-ENV";
	} else if (board_is_genio_1200_evk()) {
		efi_guid_t image_type_guid = GENIO_1200_EVK_FIT_IMAGE_GUID;
		efi_guid_t uboot_image_type_guid = GENIO_1200_EVK_FIP_IMAGE_GUID;
		efi_guid_t bl2_image_type_guid = GENIO_1200_EVK_BL2_IMAGE_GUID;
		efi_guid_t fw_image_type_guid = GENIO_1200_EVK_FW_IMAGE_GUID;
		efi_guid_t env_image_type_guid = GENIO_1200_EVK_ENV_IMAGE_GUID;

		guidcpy(&fw_images[0].image_type_id, &image_type_guid);
		guidcpy(&fw_images[1].image_type_id, &uboot_image_type_guid);
		guidcpy(&fw_images[2].image_type_id, &bl2_image_type_guid);
		guidcpy(&fw_images[3].image_type_id, &fw_image_type_guid);
		guidcpy(&fw_images[4].image_type_id, &env_image_type_guid);

		fw_images[0].fw_name = u"GENIO-1200-EVK-FIT";
		fw_images[1].fw_name = u"GENIO-1200-EVK-FIP";
		fw_images[2].fw_name = u"GENIO-1200-EVK-BL2";
		fw_images[3].fw_name = u"GENIO-1200-EVK-FW";
		fw_images[4].fw_name = u"GENIO-1200-EVK-ENV";
	} else if (board_is_genio_1200_evk_ufs()) {
		efi_guid_t image_type_guid = GENIO_1200_EVK_UFS_FIT_IMAGE_GUID;
		efi_guid_t uboot_image_type_guid = GENIO_1200_EVK_UFS_FIP_IMAGE_GUID;
		efi_guid_t bl2_image_type_guid = GENIO_1200_EVK_UFS_BL2_IMAGE_GUID;
		efi_guid_t fw_image_type_guid = GENIO_1200_EVK_UFS_FW_IMAGE_GUID;
		efi_guid_t env_image_type_guid = GENIO_1200_EVK_UFS_ENV_IMAGE_GUID;

		guidcpy(&fw_images[0].image_type_id, &image_type_guid);
		guidcpy(&fw_images[1].image_type_id, &uboot_image_type_guid);
		guidcpy(&fw_images[2].image_type_id, &bl2_image_type_guid);
		guidcpy(&fw_images[3].image_type_id, &fw_image_type_guid);
		guidcpy(&fw_images[4].image_type_id, &env_image_type_guid);

		fw_images[0].fw_name = u"GENIO-1200-EVK-UFS-FIT";
		fw_images[1].fw_name = u"GENIO-1200-EVK-UFS-FIP";
		fw_images[2].fw_name = u"GENIO-1200-EVK-UFS-BL2";
		fw_images[3].fw_name = u"GENIO-1200-EVK-UFS-FW";
		fw_images[4].fw_name = u"GENIO-1200-EVK-UFS-ENV";
	} else if (board_is_genio_1200_evk_ufs_qspi()) {
		efi_guid_t image_type_guid = GENIO_1200_EVK_UFS_QSPI_FIT_IMAGE_GUID;
		efi_guid_t uboot_image_type_guid = GENIO_1200_EVK_UFS_QSPI_FIP_IMAGE_GUID;
		efi_guid_t bl2_image_type_guid = GENIO_1200_EVK_UFS_QSPI_BL2_IMAGE_GUID;
		efi_guid_t fw_image_type_guid = GENIO_1200_EVK_UFS_QSPI_FW_IMAGE_GUID;
		efi_guid_t env_image_type_guid = GENIO_1200_EVK_UFS_QSPI_ENV_IMAGE_GUID;

		guidcpy(&fw_images[0].image_type_id, &image_type_guid);
		guidcpy(&fw_images[1].image_type_id, &uboot_image_type_guid);
		guidcpy(&fw_images[2].image_type_id, &bl2_image_type_guid);
		guidcpy(&fw_images[3].image_type_id, &fw_image_type_guid);
		guidcpy(&fw_images[4].image_type_id, &env_image_type_guid);

		fw_images[0].fw_name = u"GENIO-1200-EVK-UFS-QSPI-FIT";
		fw_images[1].fw_name = u"GENIO-1200-EVK-UFS-QSPI-FIP";
		fw_images[2].fw_name = u"GENIO-1200-EVK-UFS-QSPI-BL2";
		fw_images[3].fw_name = u"GENIO-1200-EVK-UFS-QSPI-FW";
		fw_images[4].fw_name = u"GENIO-1200-EVK-UFS-QSPI-ENV";
	}
}

#if IS_ENABLED(CONFIG_MEDIATEK_IOT_AB_BOOT_SUPPORT)
void set_dfu_alt_info(char *interface, char *devstr)
{
	char alt[BOOTCTRL_DFU_ALT_LEN] = {0};
	const char *s = env_get(BOOTCTRL_ENV);

	if (s) {
		if (!strcmp(s, "a")) {
			if (sprintf(alt, update_info.dfu_string, BOOTCTRL_FIP_NUM + PART_BOOT_B,
				    BOOTCTRL_FW_NUM + PART_BOOT_B) < 0)
				return;
		} else if (!strcmp(s, "b")) {
			if (sprintf(alt, update_info.dfu_string,
				    BOOTCTRL_FIP_NUM, BOOTCTRL_FW_NUM) < 0)
				return;
		}
		env_set("dfu_alt_info", alt);
	}
}
#endif
#endif /* CONFIG_EFI_HAVE_CAPSULE_SUPPORT && CONFIG_EFI_PARTITION */

int board_init(void)
{
	struct udevice *dev;
	int ret;

	if (CONFIG_IS_ENABLED(USB_GADGET)) {
		ret = uclass_get_device(UCLASS_USB_GADGET_GENERIC, 0, &dev);
		if (ret) {
			pr_err("%s: Cannot find USB device\n", __func__);
			return ret;
		}
	}

	if (CONFIG_IS_ENABLED(USB_ETHER))
		usb_ether_init();

	printf("Enabling SCP SRAM\n");
	for (unsigned int val = 0xFFFFFFFF; val != 0U;) {
		val = val >> 1;
		writel(val, 0x1072102C);
	}

	/* Run scsi_scan() only when the board support NOR and UFS storage */
	if (CONFIG_IS_ENABLED(UFS_MEDIATEK) && CONFIG_IS_ENABLED(MTK_SNOR)) {
		ret = scsi_scan(true);
		if (ret) {
			printf("scsi scan failed\n");
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT) &&
	    IS_ENABLED(CONFIG_EFI_PARTITION))
		mediatek_capsule_update_board_setup();

	return 0;
}

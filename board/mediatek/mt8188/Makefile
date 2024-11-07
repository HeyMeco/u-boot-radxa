# SPDX-License-Identifier:	GPL-2.0

obj-y += mt8390_evk.o

ifeq ($(CONFIG_VIDEO_MEDIATEK),y)
	obj-y += \
		mt8390_evk_panel.o \
		mt8390_evk_panel_p1v4.o \
		mt8390_evk_edp_panel.o
	ccflags-y += -I$(srctree)/drivers/video/mediatek/
endif

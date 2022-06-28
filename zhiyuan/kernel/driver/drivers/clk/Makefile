# SPDX-License-Identifier: GPL-2.0

# Common objects
ccu-sunxi-ng-y					+= ccu_common.o
ccu-sunxi-ng-y					+= ccu_reset.o

# Base clock types
ccu-sunxi-ng-y					+= ccu_div.o
ccu-sunxi-ng-y					+= ccu_frac.o
ccu-sunxi-ng-y					+= ccu_gate.o
ccu-sunxi-ng-y					+= ccu_mux.o
ccu-sunxi-ng-y					+= ccu_mult.o
ccu-sunxi-ng-y					+= ccu_phase.o
ccu-sunxi-ng-y					+= ccu_sdm.o

# Multi-factor clocks
ccu-sunxi-ng-y					+= ccu_nk.o
ccu-sunxi-ng-y					+= ccu_nkm.o
ccu-sunxi-ng-y					+= ccu_nkmp.o
ccu-sunxi-ng-y					+= ccu_nm.o
ccu-sunxi-ng-y					+= ccu_mp.o

obj-$(CONFIG_AW_CCU)				+= ccu-sunxi-ng.o
obj-$(CONFIG_AW_RTC_CCU)			+= ccu-sunxi-rtc.o

# SoC support
obj-$(CONFIG_AW_SUN8IW20_CCU)		+= ccu-sun8iw20.o
obj-$(CONFIG_AW_SUN8IW20_CCU)		+= ccu-sun8iw20-rtc.o
obj-$(CONFIG_AW_SUN8IW20_R_CCU)		+= ccu-sun8iw20-r.o
obj-$(CONFIG_AW_SUN50IW12_CCU)		+= ccu-sun50iw12.o
obj-$(CONFIG_AW_SUN50IW12_R_CCU)	+= ccu-sun50iw12-r.o
obj-$(CONFIG_AW_SUN50IW10_CCU)		+= ccu-sun50iw10.o
obj-$(CONFIG_AW_SUN50IW10_R_CCU)	+= ccu-sun50iw10-r.o
obj-$(CONFIG_AW_SUN50IW9_CCU)		+= ccu-sun50iw9.o
obj-$(CONFIG_AW_SUN50IW9_CCU)		+= ccu-sun50iw9-rtc.o
obj-$(CONFIG_AW_SUN50IW9_R_CCU)		+= ccu-sun50iw9-r.o


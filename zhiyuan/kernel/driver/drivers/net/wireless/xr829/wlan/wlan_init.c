/*
 * Entry code of XRadio drivers
 *
 * Copyright (c) 2013
 * Xradio Technology Co., Ltd. <www.xradiotech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

#ifdef CONFIG_DRIVERS_HDF_XR829
#include <linux/netdevice.h>
#endif

#ifndef CONFIG_DRIVERS_HDF_XR829
MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xradio_wlan");
#endif

/* external interfaces */
extern int  xradio_core_init(void);
extern void xradio_core_deinit(void);

#ifndef CONFIG_DRIVERS_HDF_XR829
extern int  __init ieee80211_init(void);
#else
extern int  ieee80211_init(void);
#endif

extern void ieee80211_exit(void);
#ifndef CONFIG_DRIVERS_HDF_XR829
extern int  __init xradio_core_entry(void);
#else
extern int  xradio_core_entry(void);
#endif

extern void xradio_core_exit(void);

#ifdef CONFIG_XRADIO_ETF
void xradio_etf_to_wlan(u32 change);
#endif

static int etf_enable = -1;

#ifndef CONFIG_DRIVERS_HDF_XR829
module_param(etf_enable, int, S_IRUSR);
#endif

/* Init Module function -> Called by insmod */
#ifndef CONFIG_DRIVERS_HDF_XR829
static int __init xradio_init(void)
#else
int xradio_init(void)
#endif
{
	int ret = 0;
	printk(KERN_ERR "======== XRADIO WIFI OPEN ========\n");
	ret = ieee80211_init();
	if (ret) {
		printk(KERN_ERR "ieee80211_init failed (%d)!\n", ret);
		goto ieee80211_fail;
	}

	ret = xradio_core_entry();
	if (ret) {
		printk(KERN_ERR "xradio_core_entry failed (%d)!\n", ret);
		goto core_entry_fail;
	}

	if (etf_enable == 1)
		goto ieee80211_fail;

#ifdef CONFIG_XRADIO_ETF
	xradio_etf_to_wlan(1);
#endif
	ret = xradio_core_init();  /* wlan driver init */
	if (ret) {
		printk(KERN_ERR "xradio_core_init failed (%d)!\n", ret);
#ifdef CONFIG_XRADIO_ETF
		xradio_etf_to_wlan(0);
#endif
		goto core_init_fail;
	}
	return ret;

core_init_fail:
	xradio_core_exit();
core_entry_fail:
	ieee80211_exit();
ieee80211_fail:
	return ret;
}

/* Called at Driver Unloading */
#ifndef CONFIG_DRIVERS_HDF_XR829
static void __exit xradio_exit(void)
#else
void xradio_exit(void)
#endif
{
	if (etf_enable == 1)
		goto exit_end;

	xradio_core_deinit();
#ifdef CONFIG_XRADIO_ETF
	xradio_etf_to_wlan(0);
#endif
exit_end:
	xradio_core_exit();
	ieee80211_exit();
	printk(KERN_ERR "======== XRADIO WIFI CLOSE ========\n");
}

#ifndef CONFIG_DRIVERS_HDF_XR829
module_init(xradio_init);
module_exit(xradio_exit);
#else
EXPORT_SYMBOL(xradio_init);
EXPORT_SYMBOL(xradio_exit);
#endif

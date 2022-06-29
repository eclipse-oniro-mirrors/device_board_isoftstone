/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

/* ****************************************************************************
  1 头文件包含
**************************************************************************** */
#include "net_adpater.h"
#include "hdf_base.h"
#include "net_device.h"
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include "eapol.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* ****************************************************************************
  2 全局变量声明
**************************************************************************** */
NetDevice *gp_hdf_netDev;
extern struct net_device_ops ieee80211_dataif_ops;
extern struct wiphy* wrap_get_wiphy(void);
extern struct net_device *get_krn_netdev(void);
extern struct NetDeviceInterFace *wal_get_net_dev_ops(void);
extern void xradio_get_mac_addrs(uint8_t *macaddr);
/* ****************************************************************************
  2 全局变量定义
**************************************************************************** */

/* ****************************************************************************
  3 函数实现
**************************************************************************** */
int32_t hdf_netdev_init(struct NetDevice *netDev)
{
    HDF_LOGE("%s: start...", __func__);
    if (NULL == netDev) {
        HDF_LOGE("%s: netDev null!", __func__);
        return HDF_FAILURE;
    }

    HDF_LOGE("%s: netDev->name:%s\n", __func__, netDev->name);
    netDev->netDeviceIf = wal_get_net_dev_ops();
    CreateEapolData(netDev);

    xradio_get_mac_addrs(netDev->macAddr);
    HDF_LOGE("%s: %02x:%02x:%02x:%02x:%02x:%02x", __func__, 
        netDev->macAddr[0],
        netDev->macAddr[1],
        netDev->macAddr[2],
        netDev->macAddr[3],
        netDev->macAddr[4],
        netDev->macAddr[5]
        );

    return HDF_SUCCESS;
}

void hdf_netdev_deinit(struct NetDevice *netDev)
{
    HDF_LOGE("%s: start...", __func__);
    (void)netDev;
}

int32_t hdf_netdev_open(struct NetDevice *netDev)
{
    int32_t retVal = 0;
    //struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device * netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    if (NULL == netdev) {
        HDF_LOGE("%s: netDev null!", __func__);
        return HDF_FAILURE;
    }

   /* HDF_LOGE("%s: ndo_stop...", __func__);
    retVal = (int32_t)ieee80211_dataif_ops.ndo_stop(netdev);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device stop failed! ret = %d", __func__, retVal);
    }*/

    retVal = (int32_t)ieee80211_dataif_ops.ndo_open(netdev);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device open failed! ret = %d", __func__, retVal);
    }

    netDev->ieee80211Ptr = netdev->ieee80211_ptr;
    if (NULL == netDev->ieee80211Ptr) {
        HDF_LOGE("%s: NULL == netDev->ieee80211Ptr", __func__);
    }

    //struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);
    gp_hdf_netDev = netDev;

	HDF_LOGE("%s: ndo_open...", __func__);
    return retVal;
}

int32_t hdf_netdev_stop(struct NetDevice *netDev)
{
    int32_t retVal = 0;
    struct net_device * netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    if (NULL == netdev) {
        HDF_LOGE("%s: netDev null!", __func__);
        return HDF_FAILURE;
    }

    retVal = (int32_t)ieee80211_dataif_ops.ndo_stop(netdev);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device stop failed! ret = %d", __func__, retVal);
    }

	HDF_LOGE("%s: ndo_stop...", __func__);
    return retVal;
}

int32_t hdf_netdev_xmit(struct NetDevice *netDev, NetBuf *netBuff)
{
    int32_t retVal = 0;
    struct net_device * netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGI("%s: start...", __func__);

    if (NULL == netdev || NULL == netBuff) {
        HDF_LOGE("%s: netdev or netBuff null!", __func__);
        return HDF_FAILURE;
    }

    retVal = (int32_t)ieee80211_dataif_ops.ndo_start_xmit((struct sk_buff *)netBuff, netdev);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device xmit failed! ret = %d", __func__, retVal);
    }

    return retVal;
}

int32_t hdf_netdev_setmacaddr(struct NetDevice *netDev, void *addr)
{
    int32_t retVal = 0;
    struct net_device * netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    if (NULL == netdev || NULL == addr) {
        HDF_LOGE("%s: netDev or addr null!", __func__);
        return HDF_FAILURE;
    }

    retVal = (int32_t)ieee80211_dataif_ops.ndo_set_mac_address(netdev, addr);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device setmacaddr failed! ret = %d", __func__, retVal);
    }

    return retVal;
}

void hdf_netdev_setnetifstats(struct NetDevice *netDev, NetIfStatus status)
{
    HDF_LOGE("%s: start...", __func__);
    (void)netDev;
    (void)status;
}

uint16_t hdf_netdev_selectqueue(struct NetDevice *netDev, NetBuf *netBuff)
{
    HDF_LOGE("%s: start...", __func__);
    (void)netDev;
    (void)netBuff;
    return HDF_SUCCESS;
}

uint32_t hdf_netdev_netifnotify(struct NetDevice *netDev, NetDevNotify *notify)
{
    HDF_LOGE("%s: start...", __func__);
    (void)netDev;
    (void)notify;
    return HDF_SUCCESS;
}

int32_t hdf_netdev_changemtu(struct NetDevice *netDev, int32_t mtu)
{   
    int32_t retVal = 0;
    struct net_device * netdev = get_krn_netdev();
    HDF_LOGE("%s: start...", __func__);

    (void)netDev;
    if (NULL == netdev) {
        HDF_LOGE("%s: netdev null!", __func__);
        return HDF_FAILURE;
    }
    HDF_LOGE("%s: change mtu to %d\n", __FUNCTION__, mtu);

    retVal = (int32_t)ieee80211_dataif_ops.ndo_change_mtu(netdev, mtu);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf net device chg mtu failed! ret = %d", __func__, retVal);
    }

    return retVal;
}

void hdf_netdev_linkstatuschanged(struct NetDevice *netDev)
{
    HDF_LOGE("%s: start...", __func__);
    (void)netDev;
}

#define WIFI_SHIFT_BIT 8

ProcessingResult hdf_netdev_specialethertypeprocess(const struct NetDevice *netDev, NetBuf *buff)
{
    //struct EtherHeader *header = NULL;
    //uint16_t etherType;
    const struct Eapol *eapolInstance = NULL;
    int ret;
    uint16_t protocol;
    const int pidx0 = 12, pidx1 = 13;

    HDF_LOGE("%s: start...", __func__);

    if (netDev == NULL || buff == NULL) {
        return PROCESSING_ERROR;
    }

    //header = (struct EtherHeader *)NetBufGetAddress(buff, E_DATA_BUF);

    protocol = (buff->data[pidx0] << WIFI_SHIFT_BIT) | buff->data[pidx1];
    if (protocol != ETHER_TYPE_PAE) {
        HDF_LOGE("%s: return PROCESSING_CONTINUE", __func__);
        return PROCESSING_CONTINUE;
    }
    if (netDev->specialProcPriv == NULL) {
        HDF_LOGE("%s: return PROCESSING_ERROR", __func__);
        return PROCESSING_ERROR;
    }

    eapolInstance = EapolGetInstance();
    ret = eapolInstance->eapolOp->writeEapolToQueue(netDev, buff);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: writeEapolToQueue failed", __func__);
        NetBufFree(buff);
    }
    return PROCESSING_COMPLETE;
}

/*****************************************************************************
  net_device上挂接的net_device_ops函数
**************************************************************************** */
struct NetDeviceInterFace g_wal_net_dev_ops = 
{
    .init       = hdf_netdev_init,
    .deInit     = hdf_netdev_deinit,
    .open       = hdf_netdev_open,
    .stop       = hdf_netdev_stop,
    .xmit       = hdf_netdev_xmit,
    .setMacAddr = hdf_netdev_setmacaddr,
    .setNetIfStatus     = hdf_netdev_setnetifstats,
    .selectQueue        = hdf_netdev_selectqueue,
    .netifNotify        = hdf_netdev_netifnotify,
    .changeMtu          = hdf_netdev_changemtu,
    .linkStatusChanged  = hdf_netdev_linkstatuschanged,
    .specialEtherTypeProcess  = hdf_netdev_specialethertypeprocess,
};

struct NetDeviceInterFace *wal_get_net_dev_ops(void)
{
    return &g_wal_net_dev_ops;
}

void wal_netif_rx_ni(struct sk_buff *skb)
{
    NetIfRxNi(gp_hdf_netDev, skb);
}

void wal_netif_rx(struct sk_buff *skb)
{
    NetIfRx(gp_hdf_netDev, skb);
}

void wal_netif_receive_skb(struct sk_buff *skb)
{
    NetIfRxReceive(gp_hdf_netDev, skb);
}

NetDevice *get_netDev(void)
{
    return gp_hdf_netDev;
}

EXPORT_SYMBOL(get_netDev);
EXPORT_SYMBOL(wal_netif_rx);
EXPORT_SYMBOL(wal_netif_rx_ni);
EXPORT_SYMBOL(wal_netif_receive_skb);


// #ifdef CHG_FOR_HDF
EXPORT_SYMBOL(hdf_netdev_open);
EXPORT_SYMBOL(hdf_netdev_stop);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

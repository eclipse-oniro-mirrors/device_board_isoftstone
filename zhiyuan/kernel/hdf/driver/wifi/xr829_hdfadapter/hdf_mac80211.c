/*
 * Copyright (C) 2022 VYAGOO TECHNOLOGY Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wifi_mac80211_ops.h"
#include "net_adpater.h"
#include "hdf_wlan_utils.h"
#include "wifi_module.h"
#include <net/cfg80211.h>
#include <net/regulatory.h>
#include "osal_mem.h"
#include "hdf_wifi_event.h"

#define HDF_LOG_TAG Xr829Driver
#ifndef errno_t
typedef int errno_t;
#endif

extern struct cfg80211_ops xrmac_config_ops;

extern NetDevice *get_netDev(void);
extern struct wiphy* wrap_get_wiphy(void);
extern struct net_device *get_krn_netdev(void);
extern struct wireless_dev* wrap_get_widev(void);
extern int32_t HdfWifiEventInformBssFrame(const struct NetDevice *netDev, const struct WlanChannel *channel, const struct ScannedBssInfo *bssInfo);
extern void xradio_get_mac_addrs(uint8_t *macaddr);
extern int set_wifi_custom_mac_address(char *addr_str, uint8_t len);
extern int ieee80211_cancel_scan(struct wiphy *wiphy, struct wireless_dev *wdev);

extern errno_t memset_s(void * dest, size_t dest_max, int ch, size_t count);
extern errno_t memcpy_s(void *dest, size_t dest_max, const void *src, size_t count);
extern int snprintf_s(char *dest, size_t dest_max, size_t count, const char *format, ...);

typedef enum {
    WLAN_BAND_2G,
    WLAN_BAND_BUTT
} wlan_channel_band_enum;

#define WIFI_24G_CHANNEL_NUMS   (14)
#define WAL_MIN_CHANNEL_2G      (1)
#define WAL_MAX_CHANNEL_2G      (14)
#define WAL_MIN_FREQ_2G         (2412 + 5*(WAL_MIN_CHANNEL_2G - 1))
#define WAL_MAX_FREQ_2G         (2484)
#define WAL_FREQ_2G_INTERVAL    (5)
#define WLAN_WPS_IE_MAX_SIZE    (352) // (WLAN_MEM_EVENT_SIZE2 - 32)   /* 32表示事件自身占用的空间 */
/* Driver supports AP mode */
#define HISI_DRIVER_FLAGS_AP                         0x00000040
/* Driver supports concurrent P2P operations */
#define HISI_DRIVER_FLAGS_P2P_CONCURRENT             0x00000200

#define HISI_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE    0x00000400
/* P2P capable (P2P GO or P2P Client) */
#define HISI_DRIVER_FLAGS_P2P_CAPABLE                0x00000800
/* Driver supports a dedicated interface for P2P Device */
#define HISI_DRIVER_FLAGS_DEDICATED_P2P_DEVICE       0x20000000

struct WlanAssocParams *global_param = NULL;

typedef struct WlanAssocParams {
    uint32_t centerFreq;    /**< Connection channel. If this parameter is not specified, the connection channel
                             * is automatically obtained from the scan result.
                             */
    uint8_t bssid[ETH_ADDR_LEN];         /**< AP BSSID. If this parameter is not specified, the AP BSSID is automatically
                             * obtained from the scan result.
                            */
    uint8_t ssid[OAL_IEEE80211_MAX_SSID_LEN];      /**< SSID */
    uint32_t ssidLen;       /**< SSID length */
	u8 *ie;
	size_t ie_len;
	struct cfg80211_crypto_settings crypto;
} WlanAssocParams;


/*--------------------------------------------------------------------------------------------------*/
/* public */
/*--------------------------------------------------------------------------------------------------*/
static int32_t GetIfName(enum nl80211_iftype type, char *ifName, uint32_t len)
{
    if (ifName == NULL || len == 0) {
        HDF_LOGE("%s:para is null!", __func__);
        return HDF_FAILURE;
    }
    switch (type) {
        case NL80211_IFTYPE_P2P_DEVICE:
            if (snprintf_s(ifName, len, len -1, "p2p%d", 0) < 0) {
                HDF_LOGE("%s:format ifName failed!", __func__);
                return HDF_FAILURE;
            }
            break;
        case NL80211_IFTYPE_P2P_CLIENT:
            /*  fall-through */
        case NL80211_IFTYPE_P2P_GO:
            if (snprintf_s(ifName, len, len -1, "p2p-p2p0-%d", 0) < 0) {
                HDF_LOGE("%s:format ifName failed!", __func__);
                return HDF_FAILURE;
            }
            break;
        default:
            HDF_LOGE("%s:GetIfName::not supported dev type!", __func__);
            return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

void WalReleaseHwCapability(struct WlanHwCapability *self)
{
    uint8_t i;
    if (self == NULL) {
        return;
    }
    for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
        if (self->bands[i] != NULL) {
            OsalMemFree(self->bands[i]);
            self->bands[i] = NULL;
        }
    }
    if (self->supportedRates != NULL) {
        OsalMemFree(self->supportedRates);
        self->supportedRates = NULL;
    }
    OsalMemFree(self);
}

static struct ieee80211_channel *GetChannelByFreq(struct wiphy* wiphy, uint16_t center_freq)
{
    enum Ieee80211Band band;
    struct ieee80211_supported_band *currentBand = NULL;
    int32_t loop;
    for (band = (enum Ieee80211Band)0; band < IEEE80211_NUM_BANDS; band++) {
        currentBand = wiphy->bands[band];
        if (currentBand == NULL) {
            continue;
        }
        for (loop = 0; loop < currentBand->n_channels; loop++) {
            if (currentBand->channels[loop].center_freq == center_freq) {
                return &currentBand->channels[loop];
            }
        }
    }
    return NULL;
}

static struct ieee80211_channel *WalGetChannel(struct wiphy *wiphy, int32_t freq)
{
    int32_t loop = 0;
    enum Ieee80211Band band = IEEE80211_BAND_2GHZ;
    struct ieee80211_supported_band *currentBand = NULL;

    if (wiphy == NULL) {
        HDF_LOGE("%s: capality is NULL!", __func__);
        return NULL;
    }

    for (band = (enum Ieee80211Band)0; band < IEEE80211_NUM_BANDS; band++) {
        currentBand = wiphy->bands[band];
        if (currentBand == NULL) {
            continue;
        }

        for (loop = 0; loop < currentBand->n_channels; loop++) {
            if (currentBand->channels[loop].center_freq == freq) {
                return &currentBand->channels[loop];
            }
        }
    }

    return NULL;
}

/*--------------------------------------------------------------------------------------------------*/
/* HdfMac80211STAOps */
/*--------------------------------------------------------------------------------------------------*/
static int32_t WalAssociate(void)
{
    int ret = 0;
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
    struct ieee80211_channel *channel;
	struct cfg80211_assoc_request assoc_params = { 0 };

    HDF_LOGE("%s: start ...!", __func__);

    /*
      HDF_LOGV("%s: centerFreq:%u ssidLen:%u ssid:%s", __func__, global_param->centerFreq, global_param->ssidLen, global_param->ssid);
      HDF_LOGV("%s: bssid:%02x:%02x:%02x:%02x:%02x:%02x!", __func__,
        global_param->bssid[0], global_param->bssid[1], global_param->bssid[2],
        global_param->bssid[3], global_param->bssid[4], global_param->bssid[5]);
      HDF_LOGV("%s: global_param->ie:%s,  global_param->ie_len %d", __func__, global_param->ie, global_param->ie_len);
     */

      if (global_param->centerFreq != WLAN_FREQ_NOT_SPECFIED) {
		channel = WalGetChannel(wiphy, global_param->centerFreq);
		if ((channel == NULL) || (channel->flags & WIFI_CHAN_DISABLED)) {
			HDF_LOGE("%s:illegal channel.flags=%u", __func__,
				(channel == NULL) ? 0 : channel->flags);
			retVal = HDF_FAILURE;
			goto end;
		}
	}
	assoc_params.bss = cfg80211_get_bss(wiphy, channel, global_param->bssid, global_param->ssid, global_param->ssidLen,
				   IEEE80211_BSS_TYPE_ESS,
				   IEEE80211_PRIVACY_ANY);

    assoc_params.ie = global_param->ie;
    assoc_params.ie_len = global_param->ie_len;
    ret = memcpy_s(&assoc_params.crypto, sizeof(assoc_params.crypto), &global_param->crypto, sizeof(global_param->crypto));
    if (ret != 0) {
        HDF_LOGE("%s:Copy crypto info failed!ret=%d", __func__, ret);
        retVal = HDF_FAILURE;
        goto end;
    }

    retVal = xrmac_config_ops.assoc(wiphy, netdev, &assoc_params);
    if (retVal < 0) {
        HDF_LOGE("%s: connect failed!\n", __func__);
    }

end:
    if (global_param->ie != NULL) {
        kfree(global_param->ie);
        global_param->ie = NULL;
    }
    return retVal;
}


void inform_bss_frame(struct ieee80211_channel *channel, int32_t signal, int16_t freq, struct ieee80211_mgmt *mgmt, uint32_t mgmtLen)
{
    int32_t retVal = 0;
    NetDevice *netDev = get_netDev();
    struct ScannedBssInfo bssInfo;
    struct WlanChannel hdfchannel;

    if (channel == NULL || netDev == NULL || mgmt == NULL) {
        HDF_LOGE("%s: inform_bss_frame channel = null or netDev = null!", __func__);
        return;
    }

    bssInfo.signal = signal;
    bssInfo.freq = freq;
    bssInfo.mgmtLen = mgmtLen;
    bssInfo.mgmt = (struct Ieee80211Mgmt *)mgmt;

    hdfchannel.flags = channel->flags;
    hdfchannel.channelId = channel->hw_value;
    hdfchannel.centerFreq = channel->center_freq;

    /*
      HDF_LOGV("%s: hdfchannel signal:%d flags:%d--channelId:%d--centerFreq:%d--dstMac:%02x:%02x:%02x:%02x:%02x:%02x!",
        __func__, bssInfo.signal, hdfchannel.flags, hdfchannel.channelId, hdfchannel.centerFreq,
        bssInfo.mgmt->bssid[0], bssInfo.mgmt->bssid[1], bssInfo.mgmt->bssid[2],
        bssInfo.mgmt->bssid[3], bssInfo.mgmt->bssid[4], bssInfo.mgmt->bssid[5]);
     */

    retVal = HdfWifiEventInformBssFrame(netDev, &hdfchannel, &bssInfo);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf wifi event inform bss frame failed!", __func__);
    }
}

#define HDF_ETHER_ADDR_LEN (6)
void inform_connect_result(uint8_t *bssid, uint8_t *rspIe, uint8_t *reqIe, uint32_t reqIeLen, uint32_t rspIeLen, uint16_t connectStatus)
{
    int32_t retVal = 0;
    NetDevice *netDev = get_netDev();
    struct ConnetResult connResult;

   /* if (netDev == NULL || bssid == NULL || rspIe == NULL || reqIe == NULL) {
         HDF_LOGE("%s: netDev / bssid / rspIe / reqIe null!", __func__);
        return;
     }
    */

    if (netDev == NULL || bssid == NULL || rspIe == NULL) {
        HDF_LOGE("%s: netDev / bssid / rspIe null!", __func__);
        return;
    }

    memcpy_s(&connResult.bssid[0], HDF_ETHER_ADDR_LEN, bssid, HDF_ETHER_ADDR_LEN);
    HDF_LOGE("%s: connResult:%02x:%02x:%02x:%02x:%02x:%02x\n", __func__, 
        connResult.bssid[0], connResult.bssid[1], connResult.bssid[2], connResult.bssid[3], connResult.bssid[4], connResult.bssid[5]);

    connResult.rspIe = rspIe;
    connResult.rspIeLen = rspIeLen;

    connResult.reqIe = reqIe;
    connResult.reqIeLen = reqIeLen;

    connResult.connectStatus = connectStatus;

    // TODO: modify freq & statusCode
    connResult.freq = 0;
    connResult.statusCode = connectStatus;

    retVal = HdfWifiEventConnectResult(netDev, &connResult);
    if (retVal < 0) {
        HDF_LOGE("%s: hdf wifi event inform connect result failed!", __func__);
    }
}

void inform_auth_result(struct net_device *dev, const u8 *buf, size_t len)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)buf;
	u16 status_code = le16_to_cpu(mgmt->u.auth.status_code);

	if (status_code != WLAN_STATUS_SUCCESS) {
		HDF_LOGE("%s: status_code %d!", __func__, status_code);
	 } else {
		HDF_LOGE("%s: sucess!", __func__);
		WalAssociate();
	 }
}

struct wireless_dev ap_wireless_dev;
struct ieee80211_channel ap_ieee80211_channel;
#define GET_DEV_CFG80211_WIRELESS(dev) ((struct wireless_dev*)((dev)->ieee80211_ptr))
static int32_t SetupWireLessDev(struct net_device *netDev, struct WlanAPConf *apSettings)
{

    if (netDev->ieee80211_ptr == NULL) {
        netDev->ieee80211_ptr = &ap_wireless_dev;
    }

    if (GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.chan == NULL) {
        GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.chan = &ap_ieee80211_channel;
    }
    GET_DEV_CFG80211_WIRELESS(netDev)->iftype = NL80211_IFTYPE_AP;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.width = (enum nl80211_channel_type)apSettings->width;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.center_freq1 = apSettings->centerFreq1;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.center_freq2 = apSettings->centerFreq2;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.chan->hw_value = apSettings->channel;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.chan->band = IEEE80211_BAND_2GHZ;
    GET_DEV_CFG80211_WIRELESS(netDev)->preset_chandef.chan->center_freq = apSettings->centerFreq1;

    return HDF_SUCCESS;
}

/*--------------------------------------------------------------------------------------------------*/
/* HdfMac80211BaseOps */
/*--------------------------------------------------------------------------------------------------*/
// OK
int32_t WalSetMode(NetDevice *netDev, enum WlanWorkMode iftype)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    HDF_LOGE("%s: start... iftype=%d ", __func__, iftype);
    retVal = (int32_t)xrmac_config_ops.change_virtual_intf(wiphy, netdev, (enum nl80211_iftype)iftype, NULL);
    if (retVal < 0) {
        HDF_LOGE("%s: set mode failed!", __func__);
    }

    return retVal;
}


int32_t WalAddKey(struct NetDevice *netDev, uint8_t keyIndex, bool pairwise, const uint8_t *macAddr,
    struct KeyParams *params)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    HDF_LOGE("%s: start...", __func__);

    (void)netDev;
    retVal = (int32_t)xrmac_config_ops.add_key(wiphy, netdev, keyIndex, pairwise, macAddr, (struct key_params *)params);
    if (retVal < 0) {
        HDF_LOGE("%s: add key failed!", __func__);
    }

    return retVal;
}

int32_t WalDelKey(struct NetDevice *netDev, uint8_t keyIndex, bool pairwise, const uint8_t *macAddr)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    if(macAddr != NULL) {
        if ((macAddr[0] == 0) && (macAddr[1] == 0) && (macAddr[2] == 0) && (macAddr[3] == 0) && (macAddr[4] == 0) && (macAddr[5] == 0)) {
            HDF_LOGE("%s: macAddr value is 0 ", __func__);
	    macAddr = NULL;
	}
    }

    retVal = (int32_t)xrmac_config_ops.del_key(wiphy, netdev, keyIndex, pairwise, macAddr);
    if (retVal < 0) {
        HDF_LOGE("%s: delete key failed!", __func__);
    }

    return retVal;
}

int32_t WalSetDefaultKey(struct NetDevice *netDev, uint8_t keyIndex, bool unicast, bool multicas)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    HDF_LOGE("%s: start...", __func__);

    retVal = (int32_t)xrmac_config_ops.set_default_key(wiphy, netdev, keyIndex, unicast, multicas);
    if (retVal < 0) {
        HDF_LOGE("%s: set default key failed!", __func__);
    }

    return retVal;
}

int32_t WalGetDeviceMacAddr(NetDevice *netDev, int32_t type, uint8_t *mac, uint8_t len)
{
    (void)len;
    (void)type;
    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    xradio_get_mac_addrs(mac);
    return HDF_SUCCESS;
}

int32_t WalSetMacAddr(NetDevice *netDev, uint8_t *mac, uint8_t len)
{
    int32_t retVal = 0;

    (void)len;
    (void)netDev;
    HDF_LOGE("%s: start... len = %d", __func__, len);

   if (len <= ETH_ADDR_LEN) {
        char addr_mac[ETH_ADDR_LEN] = {0};
        HDF_LOGE("ADDR=%02x:%02x:%02x:%02x:%02x:%02x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        sprintf(addr_mac, "%02x:%02x:%02x:%02x:%02x:%02x\n",
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        retVal = set_wifi_custom_mac_address(addr_mac, len);
	} else {
            HDF_LOGE("%s: lenght(%u) is err", __func__, len);
	}

	if (retVal <= 0)
	    return -1;

    return HDF_SUCCESS;
}

int32_t WalSetTxPower(NetDevice *netDev, int32_t power)
{
    int retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);

    HDF_LOGE("%s: start...", __func__);

    retVal = (int32_t)xrmac_config_ops.set_tx_power(wiphy, wdev, NL80211_TX_POWER_FIXED ,power);

    if (retVal < 0) {
        HDF_LOGE("%s: set_tx_power failed!", __func__);
    }

    return HDF_SUCCESS;
}

int32_t WalGetValidFreqsWithBand(NetDevice *netDev, int32_t band, int32_t *freqs, uint32_t *num)
{
    uint32_t freqIndex = 0;
    uint32_t channelNumber;
    uint32_t freqTmp;
    uint32_t minFreq;
    uint32_t maxFreq;
    (void)band;
    (void)freqs;
    (void)num;
    HDF_LOGE("%s: start...", __func__);
	if (freqs == NULL) {
		HDF_LOGE("%s: freq is null, error!", __func__);
		return -1;
	}

    /*const struct ieee80211_regdomain *regdom = wrp_get_regdomain();
    NOT SUPPORT
    if (regdom == NULL) {
        HDF_LOGE("%s: wal_get_cfg_regdb failed!", __func__);
        return HDF_FAILURE;
    }*/

	// for pass verfy
    minFreq = 2412;
    maxFreq = 2472;
    switch (band) {
        case WLAN_BAND_2G:
            for (channelNumber = 1; channelNumber <= WIFI_24G_CHANNEL_NUMS; channelNumber++) {
                if (channelNumber < WAL_MAX_CHANNEL_2G) {
                    freqTmp = WAL_MIN_FREQ_2G + (channelNumber - 1) * WAL_FREQ_2G_INTERVAL;
                } else if (channelNumber == WAL_MAX_CHANNEL_2G) {
                    freqTmp = WAL_MAX_FREQ_2G;
                }
                if (freqTmp < minFreq || freqTmp > maxFreq) {
                    continue;
                }
                freqs[freqIndex] = freqTmp;
                freqIndex++;
            }
            *num = freqIndex;
            break;
        default:
            HDF_LOGE("%s: no support band!", __func__);
            return HDF_ERR_NOT_SUPPORT;
    }
	return HDF_SUCCESS;
}


int32_t WalGetHwCapability(struct NetDevice *netDev, struct WlanHwCapability **capability)
{
    uint8_t loop = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct ieee80211_supported_band *band = wiphy->bands[IEEE80211_BAND_2GHZ];
    struct WlanHwCapability *hwCapability = (struct WlanHwCapability *)OsalMemCalloc(sizeof(struct WlanHwCapability));

    (void)netDev;
    if (hwCapability == NULL) {
        HDF_LOGE("%s: oom!\n", __func__);
        return HDF_FAILURE;
    }
    hwCapability->Release = WalReleaseHwCapability;
    if (hwCapability->bands[IEEE80211_BAND_2GHZ] == NULL) {
        hwCapability->bands[IEEE80211_BAND_2GHZ] =
            OsalMemCalloc(sizeof(struct WlanBand) + (sizeof(struct WlanChannel) * band->n_channels));
        if (hwCapability->bands[IEEE80211_BAND_2GHZ] == NULL) {
            HDF_LOGE("%s: oom!\n", __func__);
            WalReleaseHwCapability(hwCapability);
            return HDF_FAILURE;
        }
    }
    hwCapability->htCapability = band->ht_cap.cap;
    hwCapability->bands[IEEE80211_BAND_2GHZ]->channelCount = band->n_channels;
    for (loop = 0; loop < band->n_channels; loop++) {
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].centerFreq = band->channels[loop].center_freq;
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].flags = band->channels[loop].flags;
        hwCapability->bands[IEEE80211_BAND_2GHZ]->channels[loop].channelId = band->channels[loop].hw_value;
    }
    hwCapability->supportedRateCount = band->n_bitrates;
    hwCapability->supportedRates = OsalMemCalloc(sizeof(uint16_t) * band->n_bitrates);
    if (hwCapability->supportedRates == NULL) {
        HDF_LOGE("%s: oom!\n", __func__);
        WalReleaseHwCapability(hwCapability);
        return HDF_FAILURE;
    }
    for (loop = 0; loop < band->n_bitrates; loop++) {
        hwCapability->supportedRates[loop] = band->bitrates[loop].bitrate;
    }
    *capability = hwCapability;
	HDF_LOGE("%s: start...", __func__);
    return HDF_SUCCESS;
}

#ifdef XR829_WLAN_FEATURE_P2P
int32_t WalRemainOnChannel(struct NetDevice *netDev, WifiOnChannel *onChannel)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
	struct ieee80211_channel *channel;
	u64 cookie;
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);	

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);
    if (netdev == NULL || onChannel == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
	if (onChannel->freq != WLAN_FREQ_NOT_SPECFIED) {
		channel = WalGetChannel(wiphy, onChannel->freq);
		if ((channel == NULL) || (channel->flags & WIFI_CHAN_DISABLED)) {
			HDF_LOGE("%s:illegal channel.flags=%u", __func__,
				(channel == NULL) ? 0 : channel->flags);
			return HDF_FAILURE;
		}
	}

	retVal = xrmac_config_ops.remain_on_channel(wiphy, wdev, channel, onChannel->duration, &cookie);
    if (retVal) {
        HDF_LOGE("%s: remain on channel failed! ret = %d", __func__, retVal);
    }
    return retVal;
}

int32_t WalCancelRemainOnChannel(struct NetDevice *netDev)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);
    if (netdev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }

    retVal = xrmac_config_ops.cancel_remain_on_channel(wiphy, wdev, 0);
    if (retVal) {
        HDF_LOGE("%s: cancel remain on channel failed!", __func__);
    }
	HDF_LOGE("%s: start...", __func__);
    return retVal;
}

int32_t WalProbeReqReport(struct NetDevice *netDev, int32_t report)
{
    // NO SUPPORT
    (void)report;
    HDF_LOGE("%s: start...", __func__);
    if (netDev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t WalAddIf(struct NetDevice *netDev, WifiIfAdd *ifAdd)
{
    char ifName[16] = {0};
    struct wiphy* wiphy = wrap_get_wiphy();

    HDF_LOGE("%s: start...", __func__);

    GetIfName(ifAdd->type, ifName, 16);
    xrmac_config_ops.add_virtual_intf(wiphy, ifName, 0, ifAdd->type, NULL);

    return HDF_SUCCESS; 
}


int32_t WalRemoveIf(struct NetDevice *netDev, WifiIfRemove *ifRemove)
{
    struct NetDevice *removeNetdev = NULL;
    struct wireless_dev *wdev = NULL;
    struct wiphy* wiphy = wrap_get_wiphy();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);
    if (ifRemove == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }

    removeNetdev = NetDeviceGetInstByName((const char*)(ifRemove->ifname));
    if (removeNetdev == NULL) {
        return HDF_FAILURE;
    }

    wdev = GET_NET_DEV_CFG80211_WIRELESS(removeNetdev);
    xrmac_config_ops.del_virtual_intf(wiphy, wdev);

    return HDF_SUCCESS;
}

int32_t WalSetApWpsP2pIe(struct NetDevice *netDev, WifiAppIe *appIe)
{
    // NO SUPPORT
    (void)appIe;
    HDF_LOGE("%s: start...", __func__);
    if (netDev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t WalGetDriverFlag(struct NetDevice *netDev, WifiGetDrvFlags **params)
{
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);
    WifiGetDrvFlags *getDrvFlag = (WifiGetDrvFlags *)OsalMemCalloc(sizeof(WifiGetDrvFlags));

    if (netDev == NULL || params == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }

    HDF_LOGE("%s: start...", __func__);

    if (NULL == wdev) {
        HDF_LOGE("%s: wdev NULL", __func__);
        return HDF_FAILURE;
    } else {
        HDF_LOGE("%s: p_wdev:%p", __func__, wdev);
    }

    if (NULL == getDrvFlag) {
        HDF_LOGE("%s: getDrvFlag NULL", __func__);
    }
    switch (wdev->iftype) {
        case NL80211_IFTYPE_P2P_CLIENT:
             /* fall-through */
        case NL80211_IFTYPE_P2P_GO:
            HDF_LOGE("%s: NL80211_IFTYPE_P2P_GO case!", __func__);
            getDrvFlag->drvFlags = HISI_DRIVER_FLAGS_AP;
            break;
        case NL80211_IFTYPE_P2P_DEVICE:
            HDF_LOGE("%s: NL80211_IFTYPE_P2P_DEVICE case!", __func__);
            getDrvFlag->drvFlags = (HISI_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE |
                                            HISI_DRIVER_FLAGS_P2P_CONCURRENT |
                                            HISI_DRIVER_FLAGS_P2P_CAPABLE);
            break;
        default:
            HDF_LOGE("%s: getDrvFlag->drvFlags default to 0 case!", __func__);
            getDrvFlag->drvFlags = 0;
    }
    *params = getDrvFlag;
    return HDF_SUCCESS;
}
#endif

int32_t WalSendAction(struct NetDevice *netDev, WifiActionData *actionData)
{
    (void)netDev;
    (void)actionData;
    HDF_LOGE("%s: start...", __func__);
    return HDF_ERR_NOT_SUPPORT;
}

int32_t WalGetIftype(struct NetDevice *netDev, uint8_t *iftype)
{
    iftype = (uint8_t *)(&(GET_NET_DEV_CFG80211_WIRELESS(netDev)->iftype));
    if (iftype != NULL)
        HDF_LOGE("%s: start...", __func__);
    return HDF_SUCCESS;
}


/*--------------------------------------------------------------------------------------------------*/
/* HdfMac80211STAOps */
/*--------------------------------------------------------------------------------------------------*/
static int32_t WalAuthenticate(NetDevice *netDev, WlanConnectParams *param)
{
    int ret = 0;
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
    struct ieee80211_channel *channel;
	struct cfg80211_auth_request auth_params = { 0 };

    HDF_LOGE("%s: start ...!", __func__);

    (void)netDev;
    if (netdev == NULL || param == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }

    auth_params.ie = param->ie;
    auth_params.ie_len = param->ieLen;
    auth_params.key = param->key;
    auth_params.auth_type = (u_int8_t)param->authType;
    auth_params.key_len = param->keyLen;
    auth_params.key_idx = param->keyIdx;
    auth_params.auth_data = NULL;
    auth_params.auth_data_len = 0;

	if (param->centerFreq != WLAN_FREQ_NOT_SPECFIED) {
		channel = WalGetChannel(wiphy, param->centerFreq);
		if ((channel == NULL) || (channel->flags & WIFI_CHAN_DISABLED)) {
			HDF_LOGE("%s:illegal channel.flags=%u", __func__,
				(channel == NULL) ? 0 : channel->flags);
			return HDF_FAILURE;
		}
	}
	auth_params.bss = cfg80211_get_bss(wiphy, channel, param->bssid, param->ssid, param->ssidLen,
				   IEEE80211_BSS_TYPE_ESS,
				   IEEE80211_PRIVACY_ANY);

	global_param->centerFreq = param->centerFreq;
	global_param->ssidLen = param->ssidLen;
	global_param->ie_len = param->ieLen;
	memcpy_s(global_param->bssid, ETH_ADDR_LEN, param->bssid, ETH_ADDR_LEN);
	memcpy_s(global_param->ssid, OAL_IEEE80211_MAX_SSID_LEN, param->ssid, param->ssidLen);
	if (global_param->ie != NULL) {
	    HDF_LOGE("%s: global_param ie is not NULL, kfree!", __func__);
	    kfree(global_param->ie);
	    global_param->ie = NULL;
	}
	global_param->ie = NULL;
	global_param->ie = kzalloc(global_param->ie_len, GFP_KERNEL);
	if (global_param->ie == NULL) {
		HDF_LOGE("%s: global_param ie is NULL", __func__);
		return HDF_FAILURE;
	}
    ret = memcpy_s(&global_param->crypto, sizeof(global_param->crypto), &param->crypto, sizeof(param->crypto));
    if (ret != 0) {
        HDF_LOGE("%s:Copy crypto info failed!ret=%d", __func__, ret);
        return HDF_FAILURE;
    }
	//global_param->ie = param->ie;
    memcpy_s(global_param->ie, global_param->ie_len, param->ie, param->ieLen);
    //HDF_LOGV("%s: global_param->ie:%s,  global_param->ie_len %d", __func__, global_param->ie, global_param->ie_len);

	retVal = xrmac_config_ops.auth(wiphy, netdev, &auth_params);
    if (retVal < 0) {
        kfree(global_param->ie);
        global_param->ie = NULL;
        HDF_LOGE("%s: connect failed!\n", __func__);
    }

    return retVal;
}

int32_t WalDisconnect(NetDevice *netDev, uint16_t reasonCode)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
    struct cfg80211_deauth_request req;

    (void)netDev;
    req.bssid = global_param->bssid;
    req.reason_code = reasonCode;
    HDF_LOGE("%s: start...reasonCode:%d", __func__, reasonCode);

    retVal = (int32_t)xrmac_config_ops.deauth(wiphy, netdev, &req);
    if (retVal < 0) {
        HDF_LOGE("%s: sta disconnect failed!", __func__);
    }

    return retVal;
}

int32_t WalStartScan(NetDevice *netDev, struct WlanScanRequest *scanParam)
{
   int32_t loop;
    int32_t count = 0;
    int32_t retVal = 0;
    enum Ieee80211Band band = IEEE80211_BAND_2GHZ;
    struct ieee80211_channel *chan = NULL;
    struct wiphy* wiphy = wrap_get_wiphy();

    struct cfg80211_scan_request *request = 
            (struct cfg80211_scan_request *)OsalMemCalloc(sizeof(struct cfg80211_scan_request));

	HDF_LOGE("%s: start ...", __func__);

    if (request == NULL) {
        HDF_LOGE("%s: calloc request null!\n", __func__);
        return HDF_FAILURE;
    }

    // basic info
    request->wiphy = wiphy;
    // request->dev = netdev;  // for sched scan 
    request->wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);
    request->n_ssids = scanParam->ssidCount;
    // request->prefix_ssid_scan_flag = 0;  // for sched scan 

    // channel info
    if ((scanParam->freqs == NULL) || (scanParam->freqsCount == 0)) {
        if (wiphy->bands[band] == NULL) {
            HDF_LOGE("%s: wiphy->bands[band] = NULL!\n", __func__);
            return HDF_FAILURE;
        }

        for (loop = 0; loop < (int32_t)wiphy->bands[band]->n_channels; loop++) {
            chan = &wiphy->bands[band]->channels[loop];
            if ((chan->flags & WIFI_CHAN_DISABLED) != 0) {
                continue;
            }
            request->channels[count++] = chan;
        }
    } else {
        for (loop = 0; loop < scanParam->freqsCount; loop++) {
            chan = GetChannelByFreq(wiphy, (uint16_t)(scanParam->freqs[loop]));
            if (chan == NULL) {
                HDF_LOGE("%s: freq not found!freq=%d!\n", __func__, scanParam->freqs[loop]);
                continue;
            }

            request->channels[count++] = chan;
        }
    }

    if (count == 0) {
        HDF_LOGE("%s: invalid freq info!\n", __func__);
        return HDF_FAILURE;
    }
    request->n_channels = count;

    // ssid info
    count = 0;
    loop = 0;
    if (scanParam->ssidCount > WPAS_MAX_SCAN_SSIDS) {
        HDF_LOGE("%s:unexpected numSsids!numSsids=%u", __func__, scanParam->ssidCount);
        return HDF_FAILURE;
    }

    if (scanParam->ssidCount == 0) {
        HDF_LOGE("%s:ssid number is 0!", __func__);
		return HDF_SUCCESS;
    }

    request->ssids = (struct cfg80211_ssid *)OsalMemCalloc(scanParam->ssidCount * sizeof(struct cfg80211_ssid));
    if (request->ssids == NULL) {
        HDF_LOGE("%s: calloc request->ssids null", __func__);
        return HDF_FAILURE;
    }

    for (loop = 0; loop < scanParam->ssidCount; loop++) {
        if (count >= DRIVER_MAX_SCAN_SSIDS) {
            break;
        }

        if (scanParam->ssids[loop].ssidLen > IEEE80211_MAX_SSID_LEN) {
            continue;
        }

        request->ssids[count].ssid_len = scanParam->ssids[loop].ssidLen;
        if (memcpy_s(request->ssids[count].ssid, OAL_IEEE80211_MAX_SSID_LEN, scanParam->ssids[loop].ssid,
            scanParam->ssids[loop].ssidLen) != 0) {
            continue;
        }
        count++;
    }
    request->n_ssids = count;

    // User Ie Info
    if (scanParam->extraIEsLen > 512) {
        HDF_LOGE("%s:unexpected extra len!extraIesLen=%d", __func__, scanParam->extraIEsLen);
        return HDF_FAILURE;
    }

    if ((scanParam->extraIEs != NULL) && (scanParam->extraIEsLen != 0)) {
        request->ie = (uint8_t *)OsalMemCalloc(scanParam->extraIEsLen);
        if (request->ie == NULL) {
            HDF_LOGE("%s: calloc request->ie null", __func__);
            goto fail;
        }
        (void)memcpy_s((void*)request->ie, scanParam->extraIEsLen, scanParam->extraIEs, scanParam->extraIEsLen);
        request->ie_len = scanParam->extraIEsLen;
    }

    retVal = (int32_t)xrmac_config_ops.scan(wiphy, request);
    if (retVal < 0) {
        HDF_LOGE("%s: scan Failed!", __func__);
        if (request != NULL) {
            if ((request)->ie != NULL) {
                OsalMemFree((void*)(request->ie));
                request->ie = NULL;
            }
            if ((request)->ssids != NULL) {
                OsalMemFree(request->ssids);
                (request)->ssids = NULL;
            }
            OsalMemFree(request);
            request = NULL;
        }
    } else {
        HDF_LOGE("%s: scan OK!", __func__);
    }

    return retVal;

fail:
    if (request->ie != NULL) {
        OsalMemFree((void*)request->ie);
        request->ie = NULL;
    }

    return HDF_FAILURE;
}

int32_t WalAbortScan(NetDevice *netDev)
{
    int retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct wireless_dev *wdev = GET_NET_DEV_CFG80211_WIRELESS(netDev);

    HDF_LOGE("%s: start...", __func__);
    retVal = ieee80211_cancel_scan(wiphy, wdev);

    return retVal;
}

int32_t WalSetScanningMacAddress(NetDevice *netDev, unsigned char *mac, uint32_t len)
{
    (void)netDev;
    (void)mac;
    (void)len;
    return HDF_ERR_NOT_SUPPORT;
}

/*--------------------------------------------------------------------------------------------------*/
/* HdfMac80211APOps */
/*--------------------------------------------------------------------------------------------------*/
struct cfg80211_ap_settings ap_setting_info;
u8 ap_ssid[IEEE80211_MAX_SSID_LEN];
struct ieee80211_channel ap_chan;
int32_t WalConfigAp(NetDevice *netDev, struct WlanAPConf *apConf)
{
    int32_t ret = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    (void)netDev;
    ret = SetupWireLessDev(netdev, apConf);
    if (0 != ret) {
        HDF_LOGE("%s: set up wireless device failed!", __func__);
        return HDF_FAILURE;
    }

    HDF_LOGE("%s: before iftype = %d!", __func__, netdev->ieee80211_ptr->iftype);
    netdev->ieee80211_ptr->iftype = NL80211_IFTYPE_AP;
    HDF_LOGE("%s: after  iftype = %d!", __func__, netdev->ieee80211_ptr->iftype);

    ap_setting_info.ssid_len = apConf->ssidConf.ssidLen;
    memcpy_s(&ap_ssid[0], IEEE80211_MAX_SSID_LEN, &apConf->ssidConf.ssid[0], apConf->ssidConf.ssidLen);

    ap_setting_info.ssid = &ap_ssid[0];
    ap_setting_info.chandef.center_freq1 = apConf->centerFreq1;
    ap_setting_info.chandef.center_freq2 = apConf->centerFreq2;
    ap_setting_info.chandef.width = apConf->width;

    ap_chan.center_freq = apConf->centerFreq1;
    ap_chan.hw_value= apConf->channel;
    ap_chan.band = apConf->band;
    ap_chan.band = IEEE80211_BAND_2GHZ;
    ap_setting_info.chandef.chan = &ap_chan;

    ret = (int32_t)xrmac_config_ops.change_virtual_intf(wiphy, netdev, (enum nl80211_iftype)netdev->ieee80211_ptr->iftype, NULL);
    if (ret < 0) {
        HDF_LOGE("%s: set mode failed!", __func__);
    }
	HDF_LOGE("%s: start ...!", __func__);

    return HDF_SUCCESS;
}

int32_t WalStartAp(NetDevice *netDev)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);
    GET_DEV_CFG80211_WIRELESS(netdev)->preset_chandef.chan->center_freq = netdev->ieee80211_ptr->preset_chandef.center_freq1;
    retVal = (int32_t)xrmac_config_ops.start_ap(wiphy, netdev, &ap_setting_info);
    if (retVal < 0) {
        HDF_LOGE("%s: start_ap failed!", __func__);
    }
	HDF_LOGE("%s: start ...!", __func__);
    return retVal;
}

int32_t WalStopAp(NetDevice *netDev)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    retVal = (int32_t)xrmac_config_ops.stop_ap(wiphy, netdev);
    if (retVal < 0) {
        HDF_LOGE("%s: stop_ap failed!", __func__);
    }

	HDF_LOGE("%s: start ...!", __func__);
    return retVal;
}

int32_t WalChangeBeacon(NetDevice *netDev, struct WlanBeaconConf *param)
{
    int32_t retVal = 0;
    struct cfg80211_beacon_data info;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();

    (void)netDev;
    HDF_LOGE("%s: start...", __func__);

    memset_s(&info, sizeof(info), 0x00, sizeof(info));
    info.head = param->headIEs;
    info.head_len = (size_t)param->headIEsLength;
    info.tail = param->tailIEs;
    info.tail_len = (size_t)param->tailIEsLength;

    info.beacon_ies = NULL;
    info.proberesp_ies = NULL;
    info.assocresp_ies = NULL;
    info.probe_resp = NULL;
    info.beacon_ies_len = 0X00;
    info.proberesp_ies_len = 0X00;
    info.assocresp_ies_len = 0X00;
    info.probe_resp_len = 0X00;

    // add beacon data for start ap
    ap_setting_info.dtim_period = param->DTIMPeriod;
    ap_setting_info.hidden_ssid = param->hiddenSSID;
    ap_setting_info.beacon_interval = param->interval;
    HDF_LOGE("%s: dtim_period:%d---hidden_ssid:%d---beacon_interval:%d!",
        __func__, ap_setting_info.dtim_period, ap_setting_info.hidden_ssid, ap_setting_info.beacon_interval);

    ap_setting_info.beacon.head = param->headIEs;
    ap_setting_info.beacon.head_len = param->headIEsLength;
    ap_setting_info.beacon.tail = param->tailIEs;
    ap_setting_info.beacon.tail_len = param->tailIEsLength;

    ap_setting_info.beacon.beacon_ies = NULL;
    ap_setting_info.beacon.proberesp_ies = NULL;
    ap_setting_info.beacon.assocresp_ies = NULL;
    ap_setting_info.beacon.probe_resp = NULL;
    ap_setting_info.beacon.beacon_ies_len = 0X00;
    ap_setting_info.beacon.proberesp_ies_len = 0X00;
    ap_setting_info.beacon.assocresp_ies_len = 0X00;
    ap_setting_info.beacon.probe_resp_len = 0X00;

    HDF_LOGE("%s: headIEsLen:%d---tailIEsLen:%d!", __func__, param->headIEsLength, param->tailIEsLength);

    retVal = (int32_t)xrmac_config_ops.change_beacon(wiphy, netdev, &info);
    if (retVal < 0) {
        HDF_LOGE("%s: change_beacon failed!", __func__);
    }
	HDF_LOGE("%s: start ...!", __func__);
    return HDF_SUCCESS;
}

int32_t WalDelStation(NetDevice *netDev, const uint8_t *macAddr)
{
    int32_t retVal = 0;
    struct wiphy* wiphy = wrap_get_wiphy();
    struct net_device *netdev = get_krn_netdev();
    struct station_del_parameters del_param = {macAddr, 10, 0};

    (void)netDev;
    (void)macAddr;
    HDF_LOGE("%s: start...", __func__);

    retVal = (int32_t)xrmac_config_ops.del_station(wiphy, netdev, &del_param);
    if (retVal < 0) {
        HDF_LOGE("%s: del_station failed!", __func__);
    }

	HDF_LOGE("%s: start ...!", __func__);
    return retVal;
}

int32_t WalSetCountryCode(NetDevice *netDev, const char *code, uint32_t len)
{
    // NO SUPPORT
    (void)code;
    HDF_LOGE("%s: start...", __func__);
    if (netDev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;

}

int32_t WalGetAssociatedStasCount(NetDevice *netDev, uint32_t *num)
{
    // NO SUPPORT
    (void)num;
    HDF_LOGE("%s: start...", __func__);
    if (netDev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;

}

int32_t WalGetAssociatedStasInfo(NetDevice *netDev, WifiStaInfo *staInfo, uint32_t num) 
{
    // NO SUPPORT
    (void)staInfo;
    (void)num;
    HDF_LOGE("%s: start...", __func__);
    if (netDev == NULL) {
        HDF_LOGE("%s:NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;

}

/*--------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------*/
static struct HdfMac80211BaseOps g_baseOps =
{
    .SetMode = WalSetMode,
    .AddKey = WalAddKey,
    .DelKey = WalDelKey,
    .SetDefaultKey = WalSetDefaultKey,
    .GetDeviceMacAddr = WalGetDeviceMacAddr,
    .SetMacAddr = WalSetMacAddr,
    .SetTxPower = WalSetTxPower,
    .GetValidFreqsWithBand = WalGetValidFreqsWithBand,
    .GetHwCapability = WalGetHwCapability,
    .SendAction = WalSendAction,
    .GetIftype = WalGetIftype,
};

static struct HdfMac80211STAOps g_staOps =
{
    .Connect = WalAuthenticate,
    .Disconnect = WalDisconnect,
    .StartScan = WalStartScan,
    .AbortScan = WalAbortScan,
    .SetScanningMacAddress = WalSetScanningMacAddress,
};

static struct HdfMac80211APOps g_apOps =
{
    .ConfigAp = WalConfigAp,
    .StartAp = WalStartAp,
    .StopAp = WalStopAp,
    .ConfigBeacon = WalChangeBeacon,
    .DelStation = WalDelStation,
    .SetCountryCode = WalSetCountryCode,
    .GetAssociatedStasCount = WalGetAssociatedStasCount,
    .GetAssociatedStasInfo = WalGetAssociatedStasInfo
};

#ifdef XR829_WLAN_FEATURE_P2P
static struct HdfMac80211P2POps g_p2pOps = {
    .RemainOnChannel = WalRemainOnChannel,
    .CancelRemainOnChannel = WalCancelRemainOnChannel,
    .ProbeReqReport = WalProbeReqReport,
    .AddIf = WalAddIf,
    .RemoveIf = WalRemoveIf,
    .SetApWpsP2pIe = WalSetApWpsP2pIe,
    .GetDriverFlag = WalGetDriverFlag,
};
#endif

void XrMac80211Init(struct HdfChipDriver *chipDriver)
{
    HDF_LOGE("%s: start...", __func__);

    if (chipDriver == NULL) {
        HDF_LOGE("%s: input is NULL", __func__);
        return;
    }

	if (global_param == NULL) {
		global_param = kzalloc(sizeof(struct WlanConnectParams), GFP_KERNEL);
	}
	if (global_param == NULL) {
		HDF_LOGE("%s: global_param is NULL", __func__);
		return;
	}

    chipDriver->ops = &g_baseOps;
    chipDriver->staOps = &g_staOps;
    chipDriver->apOps = &g_apOps;
#ifdef XR829_WLAN_FEATURE_P2P
	chipDriver->p2pOps = g_p2pOps;
#else
	chipDriver->p2pOps = NULL;
#endif
};

EXPORT_SYMBOL(inform_bss_frame);
EXPORT_SYMBOL(inform_connect_result);
EXPORT_SYMBOL(inform_auth_result);

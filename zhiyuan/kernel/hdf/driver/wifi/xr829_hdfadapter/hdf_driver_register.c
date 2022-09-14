/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "hdf_device_desc.h"
#include "hdf_wifi_product.h"
#include "hdf_log.h"
#include "osal_mem.h"
#include "hdf_wlan_chipdriver_manager.h"
#include "securec.h"
#include "wifi_module.h"


#define HDF_LOG_TAG Xr829Driver
static const char * const XR829_DRIVER_NAME = "xr829";

int32_t InitXr829Chip(struct HdfWlanDevice *device);
int32_t DeinitXr829Chip(struct HdfWlanDevice *device);
int32_t Xr829Deinit(struct HdfChipDriver *chipDriver, struct NetDevice *netDevice);
int32_t Xr829Init(struct HdfChipDriver *chipDriver, struct NetDevice *netDevice);
void XrMac80211Init(struct HdfChipDriver *chipDriver);

extern errno_t memset_s(void * dest, size_t dest_max, int ch, size_t count);

static struct HdfChipDriver *BuildXr829Driver(struct HdfWlanDevice *device, uint8_t ifIndex)
{
    struct HdfChipDriver *specificDriver = NULL;

    HDF_LOGE("%s: Enter ", __func__);
	
    if (device == NULL) {
        HDF_LOGE("%s fail : channel is NULL!", __func__);
        return NULL;
    }
    (void)device;
    (void)ifIndex;
    specificDriver = (struct HdfChipDriver *)OsalMemCalloc(sizeof(struct HdfChipDriver));
    if (specificDriver == NULL) {
        HDF_LOGE("%s fail: OsalMemCalloc fail!", __func__);
        return NULL;
    }
    if (memset_s(specificDriver, sizeof(struct HdfChipDriver), 0, sizeof(struct HdfChipDriver)) != EOK) {
        HDF_LOGE("%s fail: memset_s fail!", __func__);
        OsalMemFree(specificDriver);
        return NULL;
    }

    if (strcpy_s(specificDriver->name, MAX_WIFI_COMPONENT_NAME_LEN, XR829_DRIVER_NAME) != EOK) {
        HDF_LOGE("%s fail : strcpy_s fail!", __func__);
        OsalMemFree(specificDriver);
        return NULL;
    }
    specificDriver->init = Xr829Init;
    specificDriver->deinit = Xr829Deinit;

    XrMac80211Init(specificDriver);

    return specificDriver;
}

static void ReleaseXr829Driver(struct HdfChipDriver *chipDriver)
{
    HDF_LOGE("%s: Enter ", __func__);
    return;
	
    if (chipDriver == NULL) {
        return;
    }
    if (strcmp(chipDriver->name, XR829_DRIVER_NAME) != 0) {
        HDF_LOGE("%s:Not my driver!", __func__);
        return;
    }
    OsalMemFree(chipDriver);
}

static uint8_t GetXr829GetMaxIFCount(struct HdfChipDriverFactory *factory)
{
    (void)factory;
    HDF_LOGE("%s: Enter ", __func__);

    return 1;
}

/* xr829's register */
static int32_t HDFWlanRegXrDriverFactory(void)
{
    static struct HdfChipDriverFactory tmpFactory = { 0 };
    struct HdfChipDriverManager *driverMgr;

    HDF_LOGE("%s: Enter ", __func__);
    driverMgr = HdfWlanGetChipDriverMgr();
    if (driverMgr == NULL) {
        HDF_LOGE("%s fail: driverMgr is NULL!", __func__);
        return HDF_FAILURE;
    }
    tmpFactory.driverName = XR829_DRIVER_NAME;
    tmpFactory.GetMaxIFCount = GetXr829GetMaxIFCount;
    tmpFactory.InitChip = InitXr829Chip;
    tmpFactory.DeinitChip = DeinitXr829Chip;
    tmpFactory.Build = BuildXr829Driver;
    tmpFactory.Release = ReleaseXr829Driver;
    tmpFactory.ReleaseFactory = NULL;
    if (driverMgr->RegChipDriver(&tmpFactory) != HDF_SUCCESS) {
        HDF_LOGE("%s fail: driverMgr is NULL!", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t HdfWlanXrChipDriverInit(struct HdfDeviceObject *device)
{
    (void)device;
    HDF_LOGE("%s: Enter ", __func__);
    return HDFWlanRegXrDriverFactory();
}

static int HdfWlanXrDriverBind(struct HdfDeviceObject *dev)
{
    (void)dev;
    HDF_LOGE("%s: Enter ", __func__);
    return HDF_SUCCESS;
}

static void HdfWlanXrChipRelease(struct HdfDeviceObject *object)
{
    (void)object;
    HDF_LOGE("%s: Enter ", __func__);
}

struct HdfDriverEntry g_hdfXrChipEntry = {
    .moduleVersion = 1,
    .Bind = HdfWlanXrDriverBind,
    .Init = HdfWlanXrChipDriverInit,
    .Release = HdfWlanXrChipRelease,
    .moduleName = "HDF_WLAN_CHIPS"
};

HDF_INIT(g_hdfXrChipEntry);

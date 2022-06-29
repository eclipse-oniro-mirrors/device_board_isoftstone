/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_IVI_OBJECT_H
#define HOS_CAMERA_IVI_OBJECT_H

#include "mpi_adapter.h"
#include "camera_metadata_info.h"

namespace OHOS::Camera {
class IViObject {
public:
    static std::shared_ptr<IViObject> CreateViObject();
    virtual ~IViObject() {};
    virtual void ConfigVi(std::vector<DeviceFormat>& format) = 0;
    virtual RetCode StartVi() = 0;
    virtual RetCode StopVi() = 0;
    virtual RetCode SetFlashlight(FlashMode mode, bool enable) = 0;
    virtual RetCode UpdateSetting(const camera_device_metadata_tag_t command, const void* args) = 0;
    virtual RetCode QuerySetting(const camera_device_metadata_tag_t command, void* args) = 0;
};
}
#endif // namespace OHOS::Camera


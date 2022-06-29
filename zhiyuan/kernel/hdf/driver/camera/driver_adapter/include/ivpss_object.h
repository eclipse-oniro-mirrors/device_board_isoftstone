/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_IVPSS_OBJECT_H
#define HOS_CAMERA_IVPSS_OBJECT_H

#include "mpi_adapter.h"

namespace OHOS::Camera {
class IVpssObject {
public:
    static std::shared_ptr<IVpssObject> CreateVpssObject();
    virtual ~IVpssObject() {};
    virtual void ConfigVpss(std::vector<DeviceFormat>& format) = 0;
    virtual RetCode StartVpss() = 0;
    virtual RetCode StopVpss() = 0;
};
}
#endif // namespace OHOS::Camera

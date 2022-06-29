/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_IVO_OBJECT_H
#define HOS_CAMERA_IVO_OBJECT_H

#include "mpi_adapter.h"

namespace OHOS::Camera {
class IVoObject {
public:
    static std::shared_ptr<IVoObject> CreateVoObject();
    virtual ~IVoObject() {};
    virtual void ConfigVo(std::vector<DeviceFormat>& format) = 0;
    virtual void StartVo() = 0;
    virtual void StopVo() = 0;
};
}

#endif // OHOS::Camera


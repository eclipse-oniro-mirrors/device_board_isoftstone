/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_IMX600_H
#define HOS_CAMERA_IMX600_H

#include "isensor.h"
#include "create_sensor_factory.h"
#include "device_manager_adapter.h"

namespace OHOS::Camera {
class Imx600 : public ISensor {
    DECLARE_SENSOR(Imx600)
public:
    Imx600();
    virtual ~Imx600();
    void Init(CameraStandard::CameraMetadata& camera_meta_data);
    void InitPhysicalSize(CameraStandard::CameraMetadata& camera_meta_data);
    void InitAntiBandingModes(CameraStandard::CameraMetadata& camera_meta_data);
    void InitAeFpsTarget(CameraStandard::CameraMetadata& camera_meta_data);
    void InitCompensationRange(CameraStandard::CameraMetadata& camera_meta_data);
    void InitSensitivityRange(CameraStandard::CameraMetadata& camera_meta_data);
};
} // namespace OHOS::Camera
#endif

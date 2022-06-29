/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "imx335.h"
#include <vector>

namespace OHOS::Camera {
IMPLEMENT_SENSOR(Imx335)
Imx335::Imx335() : ISensor("imx335") {}

Imx335::~Imx335() {}
void Imx335::InitPhysicalSize(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitPhysicalSize(camera_meta_data);
}
void Imx335::InitAntiBandingModes(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitAntiBandingModes(camera_meta_data);
}
void Imx335::InitAvailableModes(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitAvailableModes(camera_meta_data);
}
void Imx335::InitFpsTarget(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitFpsTarget(camera_meta_data);
}
void Imx335::InitCompensationRange(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitCompensationRange(camera_meta_data);
}

void Imx335::InitAwbModes(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitAwbModes(camera_meta_data);
}

void Imx335::InitSensitivityRange(CameraStandard::CameraMetadata& camera_meta_data)
{
    ISensor::InitSensitivityRange(camera_meta_data);
}

void Imx335::Init(CameraStandard::CameraMetadata& camera_metaData)
{
    InitPhysicalSize(camera_metaData);
    InitAntiBandingModes(camera_metaData);
    InitAvailableModes(camera_metaData);
    InitFpsTarget(camera_metaData);
    InitCompensationRange(camera_metaData);

    const camera_rational_t aeCompensationStep[] = {{0, 1}};
    camera_metaData.addEntry(OHOS_CONTROL_AE_COMPENSATION_STEP, aeCompensationStep, 1);

    InitAwbModes(camera_metaData);
    InitSensitivityRange(camera_metaData);

    uint8_t faceDetectMode = OHOS_CAMERA_FACE_DETECT_MODE_OFF;
    camera_metaData.addEntry(OHOS_STATISTICS_FACE_DETECT_MODE, &faceDetectMode,
        1);
}
} // namespace OHOS::Camera

/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_PROJET_HARDWARE_H
#define HOS_CAMERA_PROJET_HARDWARE_H

namespace OHOS::Camera {
std::vector<HardwareConfiguration> hardware = {
    {CAMERA_FIRST, DM_M_VI, DM_C_VI, (std::string) "vi"},
    {CAMERA_FIRST, DM_M_VO, DM_C_VO, (std::string) "vo"},
    {CAMERA_FIRST, DM_M_VI, DM_C_SENSOR, (std::string) "Imx335"},
    {CAMERA_FIRST, DM_M_VPSS, DM_C_VPSS, (std::string) "vpss"},
    {CAMERA_SECOND, DM_M_VI, DM_C_SENSOR, (std::string) "Imx600"},
    {CAMERA_SECOND, DM_M_VO, DM_C_VO, (std::string) "vo"},
    {CAMERA_SECOND, DM_M_VI, DM_C_VI, (std::string) "vi"}
};
} // namespace OHOS::Camera
#endif

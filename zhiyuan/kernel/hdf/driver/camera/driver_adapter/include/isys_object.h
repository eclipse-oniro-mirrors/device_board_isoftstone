/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_ISYS_OBJECT_H
#define HOS_CAMERA_ISYS_OBJECT_H

#include "ibuffer.h"
#include "mpi_adapter.h"
#include "camera_metadata_info.h"

namespace OHOS::Camera {
class ISysObject {
public:
    static std::shared_ptr<ISysObject> CreateSysObject();
    virtual ~ISysObject() {};
    virtual void StartSys() = 0;
    virtual void UnInitSys() = 0;
    virtual void SetCallback(BufCallback cb) = 0;
    virtual void SetDevStatusCallback(DeviceStatusCb cb) = 0;
    virtual void VpssBindVenc(int32_t vpssChn, int32_t vencChn) = 0;
    virtual void VpssUnBindVenc(int32_t vpssChn, int32_t vencChn) = 0;
    virtual void ViBindVpss(int32_t viPipe, int32_t viChn, int32_t vpssGrp, int32_t vpssChn = 0) = 0;
    virtual void VpssBindVo(int32_t vpssGrp, int32_t vpssChn, int32_t voLayer, int32_t voChn) = 0;
    virtual void ViUnBindVpss(int32_t viPipe, int32_t viChn, int32_t vpssGrp, int32_t vpssChn = 0) = 0;
    virtual void VpssUnBindVo(int32_t vpssGrp, int32_t vpssChn, int32_t voLayer, int32_t voChn) = 0;
    virtual RetCode StopSys() = 0;
    virtual RetCode InitSys() = 0;
    virtual RetCode Flush(int32_t streamId) = 0;
    virtual RetCode Prepare() = 0;
    virtual RetCode RequestBuffer(std::shared_ptr<FrameSpec> &frameSpec) = 0;
    virtual RetCode PreConfig(const std::shared_ptr<CameraStandard::CameraMetadata>& meta,
        const std::vector<DeviceStreamSetting>& settings) = 0;
    virtual RetCode StartRecvFrame(int32_t streamId) = 0;
    virtual RetCode StopRecvFrame(int32_t streamId) = 0;
};
}
#endif // OHOS::Camera

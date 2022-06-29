/*
 * Copyright (c) 2021 iSoftStone Device Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef HOS_CAMERA_IVENC_OBJECT_H
#define HOS_CAMERA_IVENC_OBJECT_H

#include <string>
#include "mpi_adapter.h"

namespace OHOS::Camera {
class IVencObject {
public:
    static std::shared_ptr<IVencObject> CreateVencObject();
    virtual ~IVencObject() {};
    virtual void ConfigVenc(uint32_t width, uint32_t height) = 0;
    virtual void StartVenc() = 0;
    virtual void StopVenc() = 0;
    virtual void StartEncoder(uint32_t mode, uint32_t w, uint32_t h) = 0;
    virtual void EncoderProc(const void *buffer, std::string path) = 0;
    virtual void StopEncoder() = 0;
    virtual void dump() = 0;
};
}
#endif // OHOS::Camera

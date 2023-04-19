/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rk_codec_node.h"
#include <securec.h>

namespace OHOS::Camera {
uint32_t RKCodecNode::previewWidth_ = 0;
uint32_t RKCodecNode::previewHeight_ = 0;

RKCodecNode::RKCodecNode(const std::string& name, const std::string& type) : NodeBase(name, type)
{
    CAMERA_LOGV("%{public}s enter, type(%{public}s)\n", name_.c_str(), type_.c_str());
}

RKCodecNode::~RKCodecNode()
{
    CAMERA_LOGI("~RKCodecNode Node exit.");
}

RetCode RKCodecNode::Start(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Start streamId = %{public}d\n", streamId);
    return RC_OK;
}

RetCode RKCodecNode::Stop(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Stop streamId = %{public}d\n", streamId);

    if (halCtx_ != nullptr) {
        hal_mpp_ctx_delete(halCtx_);
        halCtx_ = nullptr;
        mppStatus_ = 0;
    }

    return RC_OK;
}

RetCode RKCodecNode::Flush(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::Flush streamId = %{public}d\n", streamId);
    return RC_OK;
}

void RKCodecNode::encodeJpegToMemory(unsigned char* image, int width, int height,
    const char* comment, size_t* jpegSize, unsigned char** jpegBuf)
{
    struct jpeg_compress_struct cInfo;
    struct jpeg_error_mgr jErr;
    JSAMPROW row_pointer[1];
    int row_stride = 0;
    constexpr uint32_t colorMap = 3;
    constexpr uint32_t compressionRatio = 100;
    constexpr uint32_t pixelsThick = 3;

    cInfo.err = jpeg_std_error(&jErr);

    jpeg_create_compress(&cInfo);
    cInfo.image_width = width;
    cInfo.image_height = height;
    cInfo.input_components = colorMap;
    cInfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cInfo);
    jpeg_set_quality(&cInfo, compressionRatio, TRUE);
    jpeg_mem_dest(&cInfo, jpegBuf, (unsigned long *)jpegSize);
    jpeg_start_compress(&cInfo, TRUE);

    if (comment) {
        jpeg_write_marker(&cInfo, JPEG_COM, (const JOCTET*)comment, strlen(comment));
    }

    row_stride = width;
    while (cInfo.next_scanline < cInfo.image_height) {
        row_pointer[0] = &image[cInfo.next_scanline * row_stride * pixelsThick];
        jpeg_write_scanlines(&cInfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cInfo);
    jpeg_destroy_compress(&cInfo);
}

int RKCodecNode::findStartCode(unsigned char *data, size_t dataSz)
{
    constexpr uint32_t dataSize = 4;
    constexpr uint32_t dataBit2 = 2;
    constexpr uint32_t dataBit3 = 3;

    if (data == nullptr) {
        CAMERA_LOGI("RKCodecNode::findStartCode paramater == nullptr");
        return -1;
    }

    if ((dataSz > dataSize) && (data[0] == 0) && (data[1] == 0) && \
        (data[dataBit2] == 0) && (data[dataBit3] == 1)) {
        return 4;
    }

    return -1;
}

static constexpr uint32_t nalBit = 0x1F;
#define NAL_TYPE(value)             ((value) & nalBit)
void RKCodecNode::SerchIFps(unsigned char* buf, size_t bufSize, std::shared_ptr<IBuffer>& buffer)
{
    size_t nalType = 0;
    size_t idx = 0;
    size_t size = bufSize;
    constexpr uint32_t nalTypeValue = 0x05;

    if (buffer == nullptr || buf == nullptr) {
        CAMERA_LOGI("RKCodecNode::SerchIFps paramater == nullptr");
        return;
    }

    for (int i = 0; i < bufSize; i++) {
        int ret = findStartCode(buf + idx, size);
        if (ret == -1) {
            idx += 1;
            size -= 1;
        } else {
            nalType = NAL_TYPE(buf[idx + ret]);
            CAMERA_LOGI("ForkNode::ForkBuffers nalu == 0x%{public}x buf == 0x%{public}x \n", nalType, buf[idx + ret]);
            if (nalType == nalTypeValue) {
                buffer->SetEsKeyFrame(1);
                CAMERA_LOGI("ForkNode::ForkBuffers SetEsKeyFrame == 1 nalu == 0x%{public}x\n", nalType);
                break;
            } else {
                idx += ret;
                size -= ret;
            }
        }

        if (idx >= bufSize) {
            break;
        }
    }

    if (idx >= bufSize) {
        buffer->SetEsKeyFrame(0);
        CAMERA_LOGI("ForkNode::ForkBuffers SetEsKeyFrame == 0 nalu == 0x%{public}x idx = %{public}d\n",
            nalType, idx);
    }
}

void  RKCodecNode::xYUV422ToRGBA(uint8_t *yuv422, uint8_t *rgba, int width, int height)
{
    int R, G, B, Y, U, V;
    int ynum = width * height;
    int i;

    for (i=0; i<ynum; i++) {
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average
        Y = *(yuv422 + (i * 2));
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average, U is offset by 1 bits
        U = *(yuv422 + (i / 2 * 4) + 1);
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average, V is offset by 3 bits
        V = *(yuv422 + (i / 2 * 4) + 3);

        // 1164/1000 equals 1.164, 2018/1000 equals 2.018, Y is offset by 16 bits, U is offset by 128 bits
        B = (1164 * (Y - 16) + 2018 * (U - 128)) / 1000;
        // 1164/1000 = 1.164, 391/1000 = 0.391, 813/1000 = 0.813, Y is offset by 16 bits, U is offset by 128 bits
        G = (1164 * (Y - 16) - 391 * (U - 128) - 813 * (V - 128)) / 1000;
        // 1164/1000 equals 1.164, 1596/1000 equals 1.596, Y is offset by 16 bits, V is offset by 128 bits
        R = (1164 * (Y - 16) + 1596 * (V - 128)) / 1000;

        if (R > 255) { // RGB colors are stored up to 255
            R = 255;   // RGB colors are stored up to 255
        }
        if (R < 0) {
            R = 0;
        }
        if (G > 255) { // RGB colors are stored up to 255
            G = 255;   // RGB colors are stored up to 255
        }
        if (G < 0) {
            G = 0;
        }
        if (B > 255) { // RGB colors are stored up to 255
            B = 255;   // RGB colors are stored up to 255
        }
        if (B < 0) {
            B = 0;
        }

        *(rgba + (i * 4)) = R;       // RGBA occupies 4 bits
        *(rgba + (i * 4) + 1) = G;   // RGBA occupies 4 bits, G is offset by 1 bits
        *(rgba + (i * 4) + 2) = B;   // RGBA occupies 4 bits, B is offset by 2 bits
        *(rgba + (i * 4) + 3) = 255; // RGBA occupies 4 bits, A is offset by 3 bits, and is filled with 255
    }
}

void  RKCodecNode::xYUV422ToRGB(uint8_t *yuv422, uint8_t *rgb, int width, int height)
{
    int R, G, B, Y, U, V;
    int ynum = width * height;
    int i;

    for (i=0; i<ynum; i++) {
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average
        Y = *(yuv422 + (i * 2));
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average, U is offset by 1 bits
        U = *(yuv422 + (i / 2 * 4) + 1);
        // Two pixels occupy 4 bytes of storage space, and one pixel occupies 2 bytes on average, V is offset by 3 bits
        V = *(yuv422 + (i / 2 * 4) + 3);

        // 1164/1000 equals 1.164, 2018/1000 equals 2.018, Y is offset by 16 bits, U is offset by 128 bits
        B = (1164 * (Y - 16) + 2018 * (U - 128)) / 1000;
        // 1164/1000 = 1.164, 391/1000 = 0.391, 813/1000 = 0.813, Y is offset by 16 bits, U is offset by 128 bits
        G = (1164 * (Y - 16) - 391 * (U - 128) - 813 * (V - 128)) / 1000;
        // 1164/1000 equals 1.164, 1596/1000 equals 1.596, Y is offset by 16 bits, V is offset by 128 bits
        R = (1164 * (Y - 16) + 1596 * (V - 128)) / 1000;

        if (R > 255) { // RGB colors are stored up to 255
            R = 255;   // RGB colors are stored up to 255
        }
        if (R < 0) {
            R = 0;
        }
        if (G > 255) { // RGB colors are stored up to 255
            G = 255;   // RGB colors are stored up to 255
        }
        if (G < 0) {
            G = 0;
        }
        if (B > 255) { // RGB colors are stored up to 255
            B = 255;   // RGB colors are stored up to 255
        }
        if (B < 0) {
            B = 0;
        }

        *(rgb + (i * 3)) = R;      // RGB occupies 3 bits
        *(rgb + (i * 3) + 1) = G;  // RGB occupies 3 bits, G is offset by 1 bits
        *(rgb + (i * 3) + 2) = B;  // RGB occupies 3 bits, B is offset by 2 bits
    }  
}

void  RKCodecNode::xRGBAToRGB(uint8_t *rgba, uint8_t *rgb, int width, int height)
{
    int ynum = width * height;
    int i;

    for(i=0; i<ynum; i++){
        *(rgb + (i * 3)) = *(rgba + (i * 4));
        *(rgb + (i * 3) + 1) = *(rgba + (i * 4) + 1);
        *(rgb + (i * 3) + 2) = *(rgba + (i * 4) + 2);
    }
}

void RKCodecNode::Yuv422ToRGBA8888(std::shared_ptr<IBuffer>& buffer)
{
    if (buffer == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToRGBA8888 buffer == nullptr");
        return;
    }

    previewWidth_ = buffer->GetWidth();
    previewHeight_ = buffer->GetHeight();

    int temp_src_size = previewWidth_ * previewHeight_ * 2;
    int temp_dst_size = previewWidth_ * previewHeight_ * 4;

    if (buffer->GetSize() < temp_dst_size) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToRGBA8888 buffer too small");
        return;
    }

    void* temp_src = malloc(temp_src_size);
    if (temp_src == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToRGBA8888 malloc buffer == nullptr");
        return;
    }

    void* temp_dst = malloc(temp_dst_size);
    if (!temp_dst_size) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToRGBA8888 malloc buffer == nullptr");
        return;
    }

    int ret = memcpy_s(temp_src, temp_src_size, (const void *)buffer->GetVirAddress(), temp_src_size);
    if (ret != 0) {
        printf("memcpy_s failed!\n");
    }

    xYUV422ToRGBA((uint8_t *)temp_src, (uint8_t *)temp_dst,previewWidth_,previewHeight_);

    ret = memcpy_s((void *)buffer->GetVirAddress(), temp_dst_size,temp_dst , temp_dst_size);
    if (ret != 0) {
        printf("memcpy_s failed!\n");
    }

    free(temp_src);
    free(temp_dst);
}

void RKCodecNode::Yuv422ToJpeg(std::shared_ptr<IBuffer>& buffer)
{
    if (buffer == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg buffer == nullptr");
        return;
    }

    int temp_src_size = previewWidth_ * previewHeight_ * 2;
    int temp_dst_size = previewWidth_ * previewHeight_ * 3;

    if (buffer->GetSize() < temp_dst_size) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg buffer too small");
        return;
    }
    
    void* temp_src = malloc(temp_src_size);
    if (temp_src == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg malloc buffer == nullptr");
        return;
    }

    void* temp_dst = malloc(temp_dst_size);
    if (!temp_dst_size) {
        CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg malloc buffer == nullptr");
        return;
    }

    int ret = memcpy_s(temp_src, temp_src_size, (const void *)buffer->GetVirAddress(), temp_src_size);
    if (ret != 0) {
        printf("memcpy_s failed!\n");
    }

    xYUV422ToRGB((uint8_t *)temp_src, (uint8_t *)temp_dst,previewWidth_,previewHeight_);

    unsigned char* jBuf = nullptr;
    size_t jpegSize = 0;

    encodeJpegToMemory((unsigned char *)temp_dst, previewWidth_, previewHeight_, nullptr, &jpegSize, &jBuf);
    if (jBuf == nullptr){
        CAMERA_LOGI("RKCodecNode::Yuv422ToJpeg buffer == nullptr");
        free(temp_dst);
        return;
    }

    ret = memcpy_s((unsigned char*)buffer->GetVirAddress(), jpegSize, jBuf, jpegSize);
    if (ret != 0) {
        printf("memcpy_s failed!\n");
    }

    buffer->SetEsFrameSize(jpegSize);

    free(jBuf);
    free(temp_src);
    free(temp_dst);

    CAMERA_LOGE("RKCodecNode::Yuv422ToJpeg jpegSize = %{public}d\n", jpegSize);
}

void RKCodecNode::Yuv420ToH264(std::shared_ptr<IBuffer>& buffer)
{
    if (buffer == nullptr) {
        CAMERA_LOGI("RKCodecNode::Yuv420ToH264 buffer == nullptr");
        return;
    }

    int ret = 0;
    size_t buf_size = 0;
    struct timespec ts = {};
    int64_t timestamp = 0;
    int dma_fd = buffer->GetFileDescriptor();

    if (mppStatus_ == 0) {
        MpiEncTestArgs args = {};
        args.width       = previewWidth_;
        args.height      = previewHeight_;
        args.format      = MPP_FMT_YUV420P;
        args.type        = MPP_VIDEO_CodingAVC;
        halCtx_ = hal_mpp_ctx_create(&args);
        mppStatus_ = 1;
        buf_size = ((MpiEncTestData *)halCtx_)->frame_size;

        ret = hal_mpp_encode(halCtx_, dma_fd, (unsigned char *)buffer->GetVirAddress(), &buf_size);
        SerchIFps((unsigned char *)buffer->GetVirAddress(), buf_size, buffer);

        buffer->SetEsFrameSize(buf_size);
        clock_gettime(CLOCK_REALTIME, &ts);
        timestamp = ts.tv_sec & 0xFFFFFF;
        timestamp *= 1000000000;
        timestamp += ts.tv_nsec;
        buffer->SetEsTimestamp(timestamp);
        CAMERA_LOGI("RKCodecNode::Yuv420ToH264 video capture on\n");
    } else {
        buf_size = ((MpiEncTestData *)halCtx_)->frame_size;
        ret = hal_mpp_encode(halCtx_, dma_fd, (unsigned char *)buffer->GetVirAddress(), &buf_size);
        SerchIFps((unsigned char *)buffer->GetVirAddress(), buf_size, buffer);
        buffer->SetEsFrameSize(buf_size);
        clock_gettime(CLOCK_REALTIME, &ts);
        timestamp = ts.tv_sec & 0xFFFFFF;
        timestamp *= 1000000000;
        timestamp += ts.tv_nsec;
        buffer->SetEsTimestamp(timestamp);
    }

    CAMERA_LOGI("ForkNode::ForkBuffers H264 size = %{public}d ret = %{public}d timestamp = %{public}lld\n",
        buf_size, ret, timestamp);
}

void RKCodecNode::DeliverBuffer(std::shared_ptr<IBuffer>& buffer)
{
    if (buffer == nullptr) {
        CAMERA_LOGE("RKCodecNode::DeliverBuffer frameSpec is null");
        return;
    }

    int32_t id = buffer->GetStreamId();
    CAMERA_LOGE("RKCodecNode::DeliverBuffer StreamId %{public}d", id);
    if (buffer->GetEncodeType() == ENCODE_TYPE_JPEG) {
        Yuv422ToJpeg(buffer);
    } else if (buffer->GetEncodeType() == ENCODE_TYPE_H264) {
        //Yuv420ToH264(buffer);
    } else {
        Yuv422ToRGBA8888(buffer);
    }

    outPutPorts_ = GetOutPorts();
    for (auto& it : outPutPorts_) {
        if (it->format_.streamId_ == id) {
            it->DeliverBuffer(buffer);
            CAMERA_LOGI("RKCodecNode deliver buffer streamid = %{public}d", it->format_.streamId_);
            return;
        }
    }
}

RetCode RKCodecNode::Capture(const int32_t streamId, const int32_t captureId)
{
    CAMERA_LOGV("RKCodecNode::Capture");
    return RC_OK;
}

RetCode RKCodecNode::CancelCapture(const int32_t streamId)
{
    CAMERA_LOGI("RKCodecNode::CancelCapture streamid = %{public}d", streamId);
    if (halCtx_ != nullptr) {
        hal_mpp_ctx_delete(halCtx_);
        halCtx_ = nullptr;
        mppStatus_ = 0;
    }

    return RC_OK;
}

REGISTERNODE(RKCodecNode, {"RKCodec"})
} // namespace OHOS::Camera

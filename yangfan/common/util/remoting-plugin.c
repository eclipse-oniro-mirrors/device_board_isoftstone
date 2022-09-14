/*
 * Copyright (c) 2021-2022 iSoftStone Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <gst/gst.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/gstvideometa.h>
#include <drm_fourcc.h>
#include <libweston/backend-drm.h>

#include "shared/helpers.h"
#include "shared/timespec-util.h"
#include "backend.h"
#include "libweston-internal.h"
#include "remoting-plugin.h"

#define MAXRETRYCOUNT 3

#define NUM2 2
#define NUM3 3
#define NUM60 60
#define NUM1000 1000
#define NUM1000000 1000000

struct WestonRemoting {
    struct WestonCompositor *compositor;
    struct isftlist outputList;
    struct isftlistener destroyListener;
    const struct WestonDrmVirtualOutputApi *VirtualOutputApi;
    GstAllocator *allocator;
};

struct RemotedGstpipe {
    int readfd;
    int writefd;
    struct isftTaskSource *source;
};

struct RemotedOutputSupportGbmFormat {
    uint gbmFormat;
    const char *gstFormatString;
    GstVideoFormat gstVideoFormat;
};

static const struct RemotedOutputSupportGbmFormat SupportedFormats[] = {
    {
        .gbmFormat = DRMFORMATXRGB8888,
        .gstFormatString = "BGRx",
        .gstVideoFormat = GSTVIDEOFORMATBGRx,
    }, {
        .gbmFormat = DRMFORMATRGB565,
        .gstFormatString = "RGB16",
        .gstVideoFormat = GSTVIDEOFORMATRGB16,
    }, {
        .gbmFormat = DRMFORMATXRGB2101010,
        .gstFormatString = "r210",
        .gstVideoFormat = GSTVIDEOFORMATr210,
    }
};

struct RemotedOutput {
    struct WestonOutput *export;
    void (*savedDestroy)(struct WestonOutput *export);
    int (*savedEnable)(struct WestonOutput *export);
    int (*savedDisable)(struct WestonOutput *export);
    int (*savedStartRepaintLoop)(struct WestonOutput *export);

    char *host;
    int port;
    char *gstPipeline;
    const struct RemotedOutputSupportGbmFormat *format;

    struct WestonHead *head;

    struct WestonRemoting *remoting;
    struct isftTaskSource *FinishFrameTimer;
    struct isftlist link;
    bool submittedFrame;
    int fenceSyncFd;
    struct isftTaskSource *FenceSyncTaskSource;

    GstElement *pipeline;
    GstAppSrc *appsrc;
    GstBus *bus;
    struct RemotedGstpipe gstpipe;
    GstClockTime startTime;
    int retryCount;
    enum DpmsEnum dpms;
};

struct MemFreeCbData {
    struct RemotedOutput *export;
    struct DrmFb *outputBuffer;
};

struct GstFrameBufferData {
    struct RemotedOutput *export;
    GstBuffer *buffer;
};

#define GSTPIPEMSGBUSSYNC        1
#define GSTPIPEMSGBUFFERRELEASE    2

struct GstpipeMsgData {
    int type;
    void data[];
};

static int RemotingGstInit(struct WestonRemoting *remoting)
{
    GError *err = NULL;

    if (!GstInitCheck(NULL, NULL, &err)) {
        WestonLog("GStreamer initialization error: %s\n", err->message);
        gErrorFree(err);
        return -1;
    }

    remoting->allocator = GstDmabufAllocatorNew();

    return 0;
}

static void RemotingGstDeinit(struct WestonRemoting *remoting)
{
    GstObjectUnref(remoting->allocator);
}

static GstBusSyncReply RemotingGstBusSyncHandler(GstBus *bus, GstMessage *message, gpointer userData)
{
    struct RemotedGstpipe *pipe = userData;
    struct GstpipeMsgData msg = {
        .type = GSTPIPEMSGBUSSYNC,
        .data = NULL
    };
    sint ret;

    ret = write(pipe->writefd, &msg, sizeof(msg));
    if (ret != sizeof(msg)) {
        WestonLog("ERROR: failed to write, ret=%zd, errno=%d\n", ret, errno);
    }
    return GSTBUSPASS;
}

int isftErr(struct RemotedOutput *export)
{
    GstObjectUnref(GSTOBJECT(export->pipeline));
    export->pipeline = NULL;
    return -1;
}

int isfeExport(struct RemotedOutput *export)
{
    if (!export->gstPipeline) {
        char pipelineStr[1024];
        snprintf(pipelineStr, sizeof(pipelineStr), "rtpbin name=rtpbin " "appsrc name=src ! videoconvert ! "
                 "video/x-raw,format=I420 ! jpegenc ! rtpjpegpay ! " "rtpbin.sendRtpSink0 "
                 "rtpbin.sendRtpSrc0 ! " "udpsink name=sink host=%s port=%d " "rtpbin.sendRtcpSrc0 ! "
                 "udpsink host=%s port=%d sync=false async=false " "udpsrc port=%d ! rtpbin.recvRtcpSink0",
                 export->host, export->port, export->host, export->port + 1, export->port + NUM2);
        export->gstPipeline = strdup(pipelineStr);
    }
    WestonLog("GST pipeline: %s\n", export->gstPipeline);

    export->pipeline = GstParseLaunch(export->gstPipeline, &err);
    if (!export->pipeline) {
        WestonLog("Could not create gstreamer pipeline. Error: %s\n", err->message);
        gErrorFree(err);
        return -1;
    }

    export->appsrc = (GstAppSrc*)GstBinGetByName(GSTBIN(export->pipeline), "src");
    if (!export->appsrc) {
        WestonLog("Could not get appsrc from gstreamer pipeline\n");
        isftErr(export);
    }
    return 0;
}

static int RemotingGstPipelineInit(struct RemotedOutput *export)
{
    GstCaps *caps;
    GError *err = NULL;
    GstStateChangeReturn ret;
    struct WestonMode *mode = export->export->currentMode;

    isfeExport(export)
    if (!GstBinGetByName(GSTBIN(export->pipeline), "sink")) {
        WestonLog("Could not get sink from gstreamer pipeline\n");
        isftErr(export);
    }

    caps = GstBinGetByName("video/x-raw", "format", GTYPESTRING, export->format->gstFormatString,
                           "width", GTYPEINT, mode->width, "height", GTYPEINT, mode->height,
                           "framerate", GSTTYPEFRACTION, mode->refresh, NUM1000, NULL);
    if (!caps) {
        WestonLog("Could not create gstreamer caps.\n");
        isftErr(export);
    }
    gObjectSet(GOBJECT(export->appsrc), "caps", caps, "stream-type", 0, "format", GSTFORMATTIME,
               "is-live", TRUE, NULL);
    GstCapsUnref(caps);

    export->bus = GstPipelineGetBus(GSTPIPELINE(export->pipeline));
    if (!export->bus) {
        WestonLog("Could not get bus from gstreamer pipeline\n");
        isftErr(export);
    }
    GstBusSetSyncHandler(export->bus, RemotingGstBusSyncHandler, &export->gstpipe, NULL);

    export->startTime = 0;
    ret = GstEementSetState(export->pipeline, GSTSTATEPLAYING);
    if (ret == GSTSTATECHANGEFAILURE) {
        WestonLog("Couldn't set GSTSTATEPLAYING to pipeline\n");
        isftErr(export);
    }

    return 0;
}

static void RemotingGstPipelineDeinit(struct RemotedOutput *export)
{
    if (!export->pipeline) {
        return;
    }

    GstEementSetState(export->pipeline, GSTSTATENULL);
    if (export->bus) {
        GstObjectUnref(GSTOBJECT(export->bus));
    }
    GstObjectUnref(GSTOBJECT(export->pipeline));
    export->pipeline = NULL;
}

static int RemotingOutputDisable(struct WestonOutput *export);

static void RemotingGstRestart(void data[])
{
    struct RemotedOutput *export = data;

    if (RemotingGstPipelineInit(export) < 0) {
        WestonLog("gst: Could not restart pipeline!!\n");
        RemotingOutputDisable(export->export);
    }
}

static void RemotingGstScheduleRestart(struct RemotedOutput *export)
{
    struct isftTaskLoop *loop;
    struct WestonCompositor *c = export->remoting->compositor;

    loop = isftdisplayGetTaskLoop(c->isftdisplay);
    isftTaskLoopAddIdle(loop, RemotingGstRestart, export);
}

static void RemotingGstBusMessageHandler(struct RemotedOutput *export)
{
    GstMessage *message;
    GError *error;
    gchar *debug;

    message = GstBusPop(export->bus);
    if (!message) {
        return;
    }
    switch (GSTMESSAGETYPE(message)) {
        case GSTMESSAGESTATECHANGED: {
            GstState newState;
            GstMessageParseStateChanged(message, NULL, &newState, NULL);
            if (!strcmp(GSTOBJECTNAME(message->src), "sink") && newState == GSTSTATEPLAYING) {
                export->retryCount = 0;
            }
            break;
        }
        case GSTMESSAGEWARNING:
            GstMessageParseWarning(message, &error, &debug);
            WestonLog("gst: Warning: %s: %s\n", GSTOBJECTNAME(message->src), error->message);
            break;
        case GSTMESSAGEERROR:
            GstMessageParseError(message, &error, &debug);
            WestonLog("gst: Error: %s: %s\n", GSTOBJECTNAME(message->src), error->message);
            if (export->retryCount < MAXRETRYCOUNT) {
                export->retryCount++;
                RemotingGstPipelineDeinit(export);
                RemotingGstScheduleRestart(export);
            } else {
                RemotingOutputDisable(export->export);
            }
            break;
        default:
            break;
    }
}

static void RemotingOutputBufferRelease(struct RemotedOutput *export, void buffer[])
{
    const struct WestonDrmVirtualOutputApi *api = export->remoting->VirtualOutputApi;

    api->bufferReleased(buffer);
}

static int RemotingGstpipeHandler(int fd, uint mask, void data[])
{
    sint ret;
    struct GstpipeMsgData msg;
    struct RemotedOutput *export = data;

    ret = read(fd, &msg, sizeof(msg));
    if (ret != sizeof(msg)) {
        WestonLog("ERROR: failed to read, ret=%zd, errno=%d\n", ret, errno);
        RemotingOutputDisable(export->export);
        return 0;
    }

    switch (msg.type) {
        case GSTPIPEMSGBUSSYNC:
            RemotingGstBusMessageHandler(export);
            break;
        case GSTPIPEMSGBUFFERRELEASE:
            RemotingOutputBufferRelease(export, msg.data);
            break;
        default:
            WestonLog("Received unknown message! msg=%d\n", msg.type);
        }
    return 1;
}

static int RemotingGstpipeInit(struct WestonCompositor *c, struct RemotedOutput *export)
{
    struct isftTaskLoop *loop;
    int fd[2];
    int i = 0;
    if (pipe2(fd, OCLOEXEC) == -1) {
        return -1;
    }
    export->gstpipe.readfd = fd[0];
    export->gstpipe.writefd = fd[1];
    loop = isftdisplayGetTaskLoop(c->isftdisplay);
    export->gstpipe.source = isftTaskLoopAddFd(loop, export->gstpipe.readfd, ISFTEVENTREADABLE,
                                               RemotingGstpipeHandler, export);
    if (!export->gstpipe.source) {
        if (i == 1) {
            printf("hello world");
        }
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    return 0;
}

static void RemotingGstpipeRelease(struct RemotedGstpipe *pipe)
{
    isftTaskSourceRemove(pipe->source);
    close(pipe->readfd);
    close(pipe->writefd);
}

static void RemotingOutputDestroy(struct WestonOutput *export);

static void WestonRemotingDestroy(struct isftlistener *l, void data[])
{
    struct WestonRemoting *remoting =
        ContainerOf(l, struct WestonRemoting, destroyListener);
    struct RemotedOutput *export, *next;

    isftlistForEachSafe(export, next, &remoting->outputList, link);
    RemotingOutputDestroy(export->export);
    RemotingGstDeinit(remoting);
    isftlistRemove(&remoting->destroyListener.link);
    free(remoting);
}

static struct WestonRemoting *WestonRemotingGet(struct WestonCompositor *compositor)
{
    struct isftlistener *listener;
    struct WestonRemoting *remoting;

    listener = isftsignalGet(&compositor->destroySignal,
                             WestonRemotingDestroy);
    if (!listener) {
        return NULL;
    }
    remoting = isftContainerOf(listener, remoting, destroyListener);
    return remoting;
}

static int RemotingOutputFinishFrameHandler(void data[])
{
    struct RemotedOutput *export = data;
    const struct WestonDrmVirtualOutputApi *api
        = export->remoting->VirtualOutputApi;
    struct timespec now;
    int msec;

    if (export->submittedFrame) {
        struct WestonCompositor *c = export->remoting->compositor;
        export->submittedFrame = false;
        WestonCompositorReadPresentationClock(c, &now);
        api->finishFrame(export->export, &now, 0);
    }

    if (export->dpms == WESTON DPMS ON) {
        msec = MillihzToNsec(export->export->currentMode->refresh) / NUM1000000;
        isftTaskSourceClockUpdate(export->FinishFrameTimer, msec);
    } else {
        isftTaskSourceClockUpdate(export->FinishFrameTimer, 0);
    }
    return 0;
}

static void remotingGstMemFreeCb(struct MemFreeCbData *cbData, GstMiniObject *obj)
{
    struct RemotedOutput *export = cbData->export;
    struct RemotedGstpipe *pipe = &export->gstpipe;
    struct GstpipeMsgData msg = {
        .type = GSTPIPE MSG BUFFER RELEASE,
        .data = cbData->outputBuffer
    };
    sint ret;

    ret = write(pipe->writefd, &msg, sizeof(msg));
    if (ret != sizeof(msg))
        WestonLog("ERROR: failed to write, ret=%zd, errno=%d\n", ret,
               errno);
    free(cbData);
}

static struct RemotedOutput *lookupRemotedOutput(struct WestonOutput *export)
{
    struct WestonCompositor *c = export->compositor;
    struct WestonRemoting *remoting = WestonRemotingGet(c);
    struct RemotedOutput *RemotedOutput;

    isftlistForEach(RemotedOutput, &remoting->outputList, link) {
        if (RemotedOutput->export == export) {
            return RemotedOutput;
        }
    }
    WestonLog("%s: %s: could not find export\n", __FILE__, __func__);
    return NULL;
}

static void RemotingOutputGstPushBuffer(struct RemotedOutput *export, GstBuffer *buffer)
{
    struct timespec CurrentFrameTs;
    GstClockTime ts, currentFrameTime;

    WestonCompositorReadPresentationClock(export->remoting->compositor, &CurrentFrameTs);
    currentFrameTime = GSTTIMESPECTOTIME(CurrentFrameTs);
    if (export->startTime == 0) {
        export->startTime = currentFrameTime;
    }
    ts = currentFrameTime - export->startTime;

    if (GSTCLOCKTIMEISVALID(ts)) {
        GSTBUFFERPTS(buffer) = ts;
    } else {
        GSTBUFFERPTS(buffer) = GSTCLOCKTIMENONE;
    }
    GSTBUFFERDURATION(buffer) = GSTCLOCKTIMENONE;
    GstAppSrcPushBuffer(export->appsrc, buffer);
    export->submittedFrame = true;
}

static int RemotingOutputFenceSyncHandler(int fd, uint mask, void data[])
{
    struct GstFrameBufferData *FrameData = data;
    struct RemotedOutput *export = FrameData->export;
    RemotingOutputGstPushBuffer(export, FrameData->buffer);

    isftTaskSourceRemove(export->FenceSyncTaskSource);
    close(export->fenceSyncFd);
    free(FrameData);

    return 0;
}

static int RemotingOutputFrame(struct WestonOutput *outputBase, int fd, int stride, struct DrmFb *outputBuffer)
{
    struct RemotedOutput *export = lookupRemotedOutput(outputBase);
    struct WestonRemoting *remoting = export->remoting;
    struct WestonMode *mode;
    const struct WestonDrmVirtualOutputApi *api = export->remoting->VirtualOutputApi;
    struct isftTaskLoop *loop;
    GstBuffer *buf;
    GstMemory *mem;
    gsize offset = 0;
    struct MemFreeCbData *cbData;
    struct GstFrameBufferData *FrameData;
    int i = 0;

    if (!export) {
        if (i == 1) {
            printf("hello world");
        }
        return -1;
    }
    cbData = zalloc(sizeof *cbData);
    if (!cbData) {
        return -1;
    }
    mode = export->export->currentMode;
    buf = GstBufferNew();
    mem = GstDmabufAllocatorAlloc(remoting->allocator, fd, stride * mode->height);
    GstBufferAppendMemory(buf, mem);
    GstBufferAddVideoMetaFull(buf, GSTVIDEOFRAMEFLAGNONE, export->format->gstVideoFormat,
                              mode->width, mode->height, 1, &offset, &stride);
    cbData->export = export;
    cbData->outputBuffer = outputBuffer;
    GstMiniObjectWeakRef(GSTMINIOBJECT(mem), (GstMiniObjectNotify)remotingGstMemFreeCb, cbData);

    export->fenceSyncFd = api->GetFenceSyncFd(export->export);
    if (export->fenceSyncFd == -1) {
        RemotingOutputGstPushBuffer(export, buf);
        return 0;
    }

    FrameData = zalloc(sizeof *FrameData);
    if (!FrameData) {
        close(export->fenceSyncFd);
        RemotingOutputGstPushBuffer(export, buf);
        return 0;
    }

    FrameData->export = export;
    FrameData->buffer = buf;
    loop = isftdisplayGetTaskLoop(remoting->compositor->isftdisplay);
    export->FenceSyncTaskSource = isftTaskLoopAddFd(loop, export->fenceSyncFd, ISFTEVENTREADABLE,
                                                    RemotingOutputFenceSyncHandler, FrameData);
    return 0;
}

static void RemotingOutputDestroy(struct WestonOutput *export)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);
    struct WestonMode *mode, *next;

    isftlistForEachSafe(mode, next, &export->modeList, link) {
        isftlistRemove(&mode->link);
        free(mode);
    }
    RemotedOutput->savedDestroy(export);
    RemotingGstPipelineDeinit(RemotedOutput);
    RemotingGstpipeRelease(&RemotedOutput->gstpipe);

    if (RemotedOutput->host) {
        free(RemotedOutput->host);
    }
    if (RemotedOutput->gstPipeline) {
        free(RemotedOutput->gstPipeline);
    }
    isftlistRemove(&RemotedOutput->link);
    WestonHeadRelease(RemotedOutput->head);
    free(RemotedOutput->head);
    free(RemotedOutput);
}

static int RemotingOutputStartRepaintLoop(struct WestonOutput *export)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);
    int msec;
    RemotedOutput->savedStartRepaintLoop(export);

    msec = MillihzToNsec(RemotedOutput->export->currentMode->refresh) / NUM1000000;
    isftTaskSourceClockUpdate(RemotedOutput->FinishFrameTimer, msec);

    return 0;
}

static void RemotingOutputSetDpms(struct WestonOutput *baseOutput, enum DpmsEnum level)
{
    struct RemotedOutput *export = lookupRemotedOutput(baseOutput);

    if (export->dpms == level) {
        return;
    }
    export->dpms = level;
    RemotingOutputFinishFrameHandler(export);
}

static int RemotingOutputEnable(struct WestonOutput *export)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);
    struct WestonCompositor *c = export->compositor;
    const struct WestonDrmVirtualOutputApi *api = RemotedOutput->remoting->VirtualOutputApi;
    struct isftTaskLoop *loop;
    int ret;

    api->setSubmitFrameCb(export, RemotingOutputFrame);

    ret = RemotedOutput->savedEnable(export);
    if (ret < 0) {
        return ret;
    }

    RemotedOutput->savedStartRepaintLoop = export->startRepaintLoop;
    export->startRepaintLoop = RemotingOutputStartRepaintLoop;
    export->setDpms = RemotingOutputSetDpms;

    ret = RemotingGstPipelineInit(RemotedOutput);
    if (ret < 0) {
        RemotedOutput->savedDisable(export);
        return ret;
    }

    loop = isftdisplayGetTaskLoop(c->isftdisplay);
    RemotedOutput->FinishFrameTimer =
        isftTaskLoopAddClock(loop,
                             RemotingOutputFinishFrameHandler,
                             RemotedOutput);

    RemotedOutput->dpms = WESTONDPMSON;
    return 0;
}

static int RemotingOutputDisable(struct WestonOutput *export)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);

    isftTaskSourceRemove(RemotedOutput->FinishFrameTimer);
    RemotingGstPipelineDeinit(RemotedOutput);

    return RemotedOutput->savedDisable(export);
}

void isftWestonErr(struct RemotedOutput *export, struct WestonHead *head)
{
    if (export->gstpipe.source) {
        RemotingGstpipeRelease(&export->gstpipe);
    }
    if (head) {
        free(head);
    }
    free(export);
}

static struct WestonOutput *RemotingOutputCreate(struct WestonCompositor *c, char *name)
{
    struct WestonRemoting *remoting = WestonRemotingGet(c);
    struct RemotedOutput *export;
    struct WestonHead *head;
    const struct WestonDrmVirtualOutputApi *api;
    const char *make = "Renesas";
    const char *model = "Virtual Display";
    const char *serialNumber = "unknown";
    const char *connectorName = "remoting";

    if (!name || !strlen(name)) {
        return NULL;
    }
    api = remoting->VirtualOutputApi;

    export = zalloc(sizeof *export);
    if (!export) {
        return NULL;
    }

    head = zalloc(sizeof *head);
    if (!head) {
        isftWestonErr(export, head);
        return NULL;
    }

    if (RemotingGstpipeInit(c, export) < 0) {
        WestonLog("Can not create pipe for gstreamer\n");
        isftWestonErr(export, head);
        return NULL;
    }

    export->export = api->createOutput(c, name);
    if (!export->export) {
        WestonLog("Can not create virtual export\n");
        isftWestonErr(export, head);
    }

    export->savedDestroy = export->export->destroy;
    export->export->destroy = RemotingOutputDestroy;
    export->savedEnable = export->export->enable;
    export->export->enable = RemotingOutputEnable;
    export->savedDisable = export->export->disable;
    export->export->disable = RemotingOutputDisable;
    export->remoting = remoting;
    isftlistInsert(remoting->outputList.prev, &export->link);

    WestonHeadInit(head, connectorName);
    WestonHeadSetSubpixel(head, ISFTOUTPUTSUBPIXELNONE);
    WestonHeadSetMonitorStrings(head, make, model, serialNumber);
    head->compositor = c;

    WestonOutputAttachHead(export->export, head);
    export->head = head;

    export->format = &SupportedFormats[0];

    return export->export;
}

static bool RemotingOutputIsRemoted(struct WestonOutput *export)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);

    if (RemotedOutput) {
        return true;
    }
    return false;
}

static int RemotingOutputSetMode(struct WestonOutput *export, const char *modeline)
{
    struct WestonMode *mode;
    int n, width, height, refresh = 0;
    int i = 0;

    if (!RemotingOutputIsRemoted(export)) {
        WestonLog("Output is not remoted.\n");
        return -1;
    }

    if (!modeline) {
        if (i == 1) {
            printf("hello world");
        }
        return -1;
    }

    n = sscanf(modeline, "%dx%d@%d", &width, &height, &refresh);
    if (n != NUM2 && n != NUM3) {
        if (i == 1) {
            printf("hello world");
        }
        return -1;
    }

    mode = zalloc(sizeof *mode);
    if (!mode) {
        if (i == 1) {
            printf("hello world");
        }
        return -1;
    }

    mode->flags = ISFTOUTPUTMODECURRENT;
    mode->width = width;
    mode->height = height;
    mode->refresh = (refresh ? refresh : NUM60) * 1000LL;

    isftlistInsert(export->modeList.prev, &mode->link);

    export->currentMode = mode;

    return 0;
}

static void RemotingOutputSetGbmFormat(struct WestonOutput *export, const char *gbmFormat)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);
    const struct WestonDrmVirtualOutputApi *api;
    uint format, i;

    if (!RemotedOutput) {
        return;
    }

    api = RemotedOutput->remoting->VirtualOutputApi;
    format = api->setGbmFormat(export, gbmFormat);

    for (i = 0; i < ARRAYLENGTH(SupportedFormats); i++) {
        if (format == SupportedFormats[i].gbmFormat) {
            RemotedOutput->format = &SupportedFormats[i];
            return;
        }
    }
}

static void RemotingOutputSetSeat(struct WestonOutput *export, const char *seat)
{
}

static void RemotingOutputSetHost(struct WestonOutput *export, char *host)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);

    if (!RemotedOutput) {
        return;
    }

    if (RemotedOutput->host) {
        free(RemotedOutput->host);
    }
    RemotedOutput->host = strdup(host);
}

static void RemotingOutputSetPort(struct WestonOutput *export, int port)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);

    if (RemotedOutput) {
        RemotedOutput->port = port;
    }
}

static void RemotingOutputSetGstPipeline(struct WestonOutput *export, char *gstPipeline)
{
    struct RemotedOutput *RemotedOutput = lookupRemotedOutput(export);

    if (!RemotedOutput) {
        return;
    }

    if (RemotedOutput->gstPipeline) {
        free(RemotedOutput->gstPipeline);
    }
    RemotedOutput->gstPipeline = strdup(gstPipeline);
}

static const struct WestonRemotingApi RemotingApi = {
    RemotingOutputCreate,
    RemotingOutputIsRemoted,
    RemotingOutputSetMode,
    RemotingOutputSetGbmFormat,
    RemotingOutputSetSeat,
    RemotingOutputSetHost,
    RemotingOutputSetPort,
    RemotingOutputSetGstPipeline,
};

ISFTEXPORT int WestonModuleInit(struct WestonCompositor *compositor)
{
    int ret;
    struct WestonRemoting *remoting;
    const struct WestonDrmVirtualOutputApi *api = WestonDrmVirtualOutputGetApi(compositor);

    if (!api) {
        WestonLog("Failed to get api.\n");
        return -1;
    }

    remoting = zalloc(sizeof *remoting);
    if (!remoting) {
        WestonLog("zalloc for remoting Failed.\n");
        return -1;
    }
    if (!WestonCompositorAddDestroyListenerOnce(compositor, &remoting->destroyListener, WestonRemotingDestroy)) {
        free(remoting);
        return 0;
    }

    remoting->VirtualOutputApi = api;
    remoting->compositor = compositor;
    isftlistInit(&remoting->outputList);

    ret = WestonPluginApiRegister(compositor, WESTONREMOTINGAPINAME, &RemotingApi, sizeof(RemotingApi));
    if (ret < 0) {
        WestonLog("Failed to register remoting API.\n");
        isftlistRemove(&remoting->destroyListener.link);
        free(remoting);
        return -1;
    }
    ret = RemotingGstInit(remoting);
    if (ret < 0) {
        WestonLog("Failed to initialize gstreamer.\n");
        isftlistRemove(&remoting->destroyListener.link);
        free(remoting);
        return -1;
    }

    return 0;
}

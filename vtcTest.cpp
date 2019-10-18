/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#define LOG_NDEBUG 0
#define LOG_TAG "VTCTest"

#define ANDROID_API_JB_OR_LATER 1
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>


#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <camera/Camera.h>
#include <camera/android/hardware/ICamera.h>
#include <system/audio.h>

#include <cutils/log.h>
#include <media/mediaplayer.h>
#include <media/mediarecorder.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/ACodec.h>
#include <media/stagefright/MediaDefs.h>
#ifdef ANDROID_API_JB_OR_LATER
#include <media/stagefright/foundation/ADebug.h>
#else
//#include <media/stagefright/MediaDebug.h>
#endif
#include <media/stagefright/MPEG4Writer.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/MetaData.h>

#ifdef ANDROID_API_JB_OR_LATER
#include <gui/Surface.h>
//#include <gui/ISurface.h>
#include <gui/IGraphicBufferProducer.h>

#include <gui/ISurfaceComposer.h>
#include <gui/ISurfaceComposerClient.h>
#include <gui/SurfaceComposerClient.h>
#else
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/ISurfaceComposerClient.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#endif

#include <cutils/properties.h>
#include <media/stagefright/foundation/AString.h>

#include "OMX_Index.h"
#include "OMX_Video.h"

#include "VtcCommon.h"

#include <ui/DisplayInfo.h>

#define SLEEP_AFTER_STARTING_PREVIEW 1
#define SLEEP_AFTER_STARTING_PLAYBACK 1
#define WIDTH 1920
#define HEIGHT 1080
#define VTC_LOGD printf

using namespace android;

int mMediaServerPID = -1;
int mStartMemory = 0;
int mTestCount = 0;
int mFailCount = 0;

int filename = 0;
int mDuration = 10;
int mPlaybackDuration = 10000;
int testcase = 1;
int InsertIDRFrameEveryXSecs = 1;
int mPreviewWidth = WIDTH;
int mPreviewHeight = HEIGHT;
int mOutputFd = -1;
int camera_index = 0;
int mSliceSizeBytes = 500;
int mSliceSizeMB = 100;
int mDisable1080pTesting = 0;
int mRobustnessTestType = -1;
bool mIsSizeInBytes = true;
bool mCameraThrewError = false;
bool mMediaPlayerThrewError = false;
char mParamValue[100];
char mRecordFileName[256];
char mPlaybackFileName[256];
char mPlaybackFileName2[256];
char mPlaybackFileName3[256];
char mPlaybackFileName4[256];
char mPlaybackFileName5[256];
char mPlaybackFileName6[256];
char mPlaybackFileName7[256];
char mPlaybackFileName8[256];
char mPlaybackFileName9[256];

FILE *mResultsFP = NULL;
// If seconds <  0, only the first frame is I frame, and rest are all P frames
// If seconds == 0, all frames are encoded as I frames. No P frames
// If seconds >  0, it is the time spacing (seconds) between 2 neighboring I frames
int32_t mIFramesIntervalSec = 1;
uint32_t mVideoBitRate      = 10000000;
uint32_t mVideoSvcLayer     = 0;
uint32_t mVideoFrameRate    = 60;
uint32_t mNewVideoBitRate   = 1000000;
uint32_t mNewVideoFrameRate = 15;
uint32_t mCycles = 10;
uint32_t mVideoCodec=0;


sp<Camera> camera;
sp<SurfaceComposerClient> client;
sp<SurfaceControl> surfaceControl;
sp<Surface> previewSurface;
CameraParameters params;
sp<MediaRecorder> recorder;

sp<MediaPlayer> player;
sp<SurfaceControl> playbackSurfaceControl;
sp<Surface> playbackSurface;

sp<MediaPlayer> player2;
sp<SurfaceControl> playbackSurfaceControl2;
sp<Surface> playbackSurface2;

sp<MediaPlayer> player3;
sp<SurfaceControl> playbackSurfaceControl3;
sp<Surface> playbackSurface3;

sp<MediaPlayer> player4;
sp<SurfaceControl> playbackSurfaceControl4;
sp<Surface> playbackSurface4;

sp<MediaPlayer> player5;
sp<SurfaceControl> playbackSurfaceControl5;
sp<Surface> playbackSurface5;

sp<MediaPlayer> player6;
sp<SurfaceControl> playbackSurfaceControl6;
sp<Surface> playbackSurface6;

sp<MediaPlayer> player7;
sp<SurfaceControl> playbackSurfaceControl7;
sp<Surface> playbackSurface7;

sp<MediaPlayer> player8;
sp<SurfaceControl> playbackSurfaceControl8;
sp<Surface> playbackSurface8;

sp<MediaPlayer> player9;
sp<SurfaceControl> playbackSurfaceControl9;
sp<Surface> playbackSurface9;

sp<MediaPlayer> player10;
sp<SurfaceControl> playbackSurfaceControl10;
sp<Surface> playbackSurface10;

//To perform better
static pthread_cond_t mCond;
static pthread_mutex_t mMutex;
bool bPlaying = false;
bool bRecording = false;

uint32_t cameraWinX = 0;
uint32_t cameraWinY = 0;
uint32_t playerWinX = 0;
uint32_t playerWinY = 0;

uint32_t cameraSurfaceWidth = 1280;
uint32_t cameraSurfaceHeight = 720;
uint32_t playbackSurfaceWidth = 400;
uint32_t playbackSurfaceHeight = 400;

//forward declarations
int test_ALL();
int test_RecordDEFAULT();
int test_InsertIDRFrames();
int test_MaxNALSize();
int test_ChangeBitRate();
int test_ChangeFrameRate();
int test_PlaybackAndRecord_sidebyside();
int test_PlaybackAndRecord_PIP();
int test_PlaybackOnly();
int test_Playback3x3();
int test_Playback_change_2x2layout();
int test_Playback_change_3x3layout();
int test_Robust();
int test_RecordPlayback();
int test_CameraPreview();
int test_SvcSpace();
int test_9dec_1enc_1dec();
int test_9dec_1rec();
int test_4dec_1rec();
int test_2dec_1rec();

typedef int (*pt2TestFunction)();
pt2TestFunction TestFunctions[17] = {
    test_Playback_change_3x3layout, // 0
    test_RecordDEFAULT, // 1
    //test_InsertIDRFrames, // 2
    test_Playback_change_2x2layout, // 2
    test_Playback3x3, // 3
    test_ChangeBitRate, // 4
    test_ChangeFrameRate, // 5
    test_PlaybackAndRecord_sidebyside, // 6
    test_PlaybackAndRecord_PIP, // 7
    test_PlaybackOnly, // 8
    test_Robust, // 9
    test_RecordPlayback, //10
    test_ALL, //11
    test_9dec_1enc_1dec, // 12
    test_4dec_1rec, //13
    test_2dec_1rec, //14
    test_9dec_1rec, //15
    test_SvcSpace //16
};

class MyCameraListener: public CameraListener {
    public:
        virtual void notify(int32_t msgType, int32_t ext1, int32_t /* ext2 */) {
                        VTC_LOGD("\n\n\n notify reported an error!!!\n\n\n");

            if ( msgType & CAMERA_MSG_ERROR && (ext1 == 1)) {
                VTC_LOGD("\n\n\n Camera reported an error!!!\n\n\n");
                mCameraThrewError = true;
                pthread_cond_signal(&mCond);
            }
        }
        virtual void postData(int32_t /* msgType */,
                              const sp<IMemory>& /* dataPtr */,
                              camera_frame_metadata_t * /* metadata */){}

        virtual void postDataTimestamp(nsecs_t /* timestamp */, int32_t /* msgType */, const sp<IMemory>& /* dataPtr */){}
	virtual void postRecordingFrameHandleTimestamp(nsecs_t timestamp, native_handle_t* handle) {}
};

sp<MyCameraListener> mCameraListener;

class PlayerListener: public MediaPlayerListener {
public:
	PlayerListener(sp<MediaPlayer> player):mPlayer(player){
		}
    virtual void notify(int msg, int ext1, int ext2, const Parcel * /* obj */)
    {
        //VTC_LOGD("Notify cb: %d %d %d\n", msg, ext1, ext2);

        if ( msg == MEDIA_PREPARED ){
            VTC_LOGD("MEDIA_PREPARED!");
            mPlayer->start();
        }

        if ( msg == MEDIA_SET_VIDEO_SIZE ){
            VTC_LOGD("\nMEDIA_SET_VIDEO_SIZE!");
        }

        if ( msg == MEDIA_PLAYBACK_COMPLETE ){
            VTC_LOGD("\nMEDIA_PLAYBACK_COMPLETE!");
            pthread_cond_signal(&mCond);
        }

        if ( msg == MEDIA_ERROR ){
            VTC_LOGD("\nPLAYER REPORTED MEDIA_ERROR!");
            mMediaPlayerThrewError = true;
            pthread_cond_signal(&mCond);
        }
    }
    sp<MediaPlayer> mPlayer;
};

sp<PlayerListener> mPlayerListener;
sp<PlayerListener> mPlayerListener2;
sp<PlayerListener> mPlayerListener3;
sp<PlayerListener> mPlayerListener4;
sp<PlayerListener> mPlayerListener5;
sp<PlayerListener> mPlayerListener6;
sp<PlayerListener> mPlayerListener7;
sp<PlayerListener> mPlayerListener8;
sp<PlayerListener> mPlayerListener9;
sp<PlayerListener> mPlayerListener10;


int getMediaserverInfo(int *PID, int *VSIZE){
    FILE *fp;
    char ps_output[256];

    /* Open the command for reading. */
    fp = popen("ps mediaserver", "r");
    if (fp == NULL) {
        VTC_LOGE("Failed to get mediaserver pid !!!" );
        return -1;
    }

    /* Read the output a line at a time. We need the last line.*/
    while (fgets(ps_output, sizeof(ps_output)-1, fp) != NULL) {}
    char *substring;
    substring = strtok (ps_output," "); // first is the USER name
    substring = strtok (NULL, " "); // second is the PID
    *PID = atoi(substring);
    substring = strtok (NULL, " "); // third is the PPID
    substring = strtok (NULL, " "); // fourth is the VSIZE
    *VSIZE = atoi(substring);
    pclose(fp);
    return 0;
}

int my_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t * mutex, int waitTimeInMilliSecs) {
    if (waitTimeInMilliSecs == 0)
    {
        return pthread_cond_wait(cond, mutex);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    if (waitTimeInMilliSecs >= 1000) { // > 1 sec
        ts.tv_sec += (waitTimeInMilliSecs/1000);
    } else {
        ts.tv_nsec += waitTimeInMilliSecs * 1000000;
    }

    return pthread_cond_timedwait(cond, mutex, &ts);
}

int createPreviewSurface() {
    if(client == NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
    VTC_LOGD("\n\n jgl prex %d, prey %d, prew %d , preh %d, \n\n\n", cameraWinX, cameraWinY, cameraSurfaceWidth, cameraSurfaceHeight);
    sp<IBinder> dtoken(SurfaceComposerClient::getBuiltInDisplay(
            ISurfaceComposer::eDisplayIdMain));
    DisplayInfo dinfo;
    status_t status = SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);

    printf("Display info %d %d\n", dinfo.w, dinfo.h);
    /*
    if ((cameraSurfaceWidth == 0) || (cameraSurfaceHeight == 0)){
        cameraSurfaceWidth = WIDTH;//client->getDisplayWidth(0);
        cameraSurfaceHeight = HEIGHT;//client->getDisplayHeight(0);
    }
    */

    surfaceControl = client->createSurface(String8("previewSurface"),
            cameraSurfaceWidth,
            cameraSurfaceHeight,
            HAL_PIXEL_FORMAT_RGB_565, 0);

    previewSurface = surfaceControl->getSurface();

    client->openGlobalTransaction();
    surfaceControl->setLayer(0x7fffffff);
    surfaceControl->setPosition(cameraWinX, cameraWinY);
    surfaceControl->setSize(cameraSurfaceWidth, cameraSurfaceHeight);
    surfaceControl->show();
    client->closeGlobalTransaction();

    return 0;
}

int destroyPreviewSurface() {

    if ( NULL != previewSurface.get() ) {
        previewSurface.clear();
    }

    if ( NULL != surfaceControl.get() ) {
        surfaceControl->clear();
        surfaceControl.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int startRecording() {

    if (camera.get() == NULL) return -1;

    recorder = new MediaRecorder(String16());

    if ( NULL == recorder.get() ) {
        VTC_LOGD("Error while creating MediaRecorder\n");
        return -1;
    }

    camera->unlock();

    if ( recorder->setCamera(camera->remote(), camera->getRecordingProxy()) < 0 ) {
        VTC_LOGD("error while setting the camera\n");
        return -1;
    }

    if ( recorder->setVideoSource(1 /*VIDEO_SOURCE_CAMERA*/) < 0 ) {
        VTC_LOGD("error while configuring camera video source\n");
        return -1;
    }
#if 0
    if ( recorder->setAudioSource(AUDIO_SOURCE_DEFAULT) < 0 ) {
        VTC_LOGD("error while configuring camera audio source\n");
        return -1;
    }
#endif
    if ( recorder->setOutputFormat(OUTPUT_FORMAT_MPEG_4) < 0 ) {
        VTC_LOGD("error while configuring output format\n");
        return -1;
    }

    //if(mkdir("/mnt/obb/vtc",0777) == -1)
         //VTC_LOGD("\n Directory \'vtc\' was not created or maybe it was already created \n");

    mOutputFd = open(mRecordFileName, O_CREAT | O_RDWR);

    if(mOutputFd < 0){
        VTC_LOGD("Error while creating video filename\n");
        return -1;
    }

    if ( recorder->setOutputFile(mOutputFd, 0, 0) < 0 ) {
        VTC_LOGD("error while configuring video filename\n");

        return -1;
    }

    if ( recorder->setVideoFrameRate(mVideoFrameRate) < 0 ) {
        VTC_LOGD("error while configuring video framerate\n");
        return -1;
    }

    if ( recorder->setVideoSize(mPreviewWidth, mPreviewHeight) < 0 ) {
        VTC_LOGD("error while configuring video size\n");
        return -1;
    }

    if ( recorder->setVideoEncoder(mVideoCodec) < 0 ) {
        VTC_LOGD("error while configuring video codec\n");
        return -1;
    }
#if 0
    if ( recorder->setAudioEncoder(AUDIO_ENCODER_AMR_NB) < 0 ) {
        VTC_LOGD("error while configuring audio codec\n");
        return -1;
    }
#endif
    if ( recorder->setPreviewSurface(previewSurface->getIGraphicBufferProducer()) < 0 ) {
        VTC_LOGD("error while configuring preview surface\n");
        return -1;
    }

    sprintf(mParamValue,"video-param-encoding-bitrate=%u", mVideoBitRate);
    String8 bit_rate(mParamValue);
    if ( recorder->setParameters(bit_rate) < 0 ) {
        VTC_LOGD("error while configuring bit rate\n");
        return -1;
    }

	if(mVideoSvcLayer != 0){
	    sprintf(mParamValue,"video-param-svc-layer=%u", mVideoSvcLayer);
	    String8 svc_layer(mParamValue);
	    printf("svc layer setting %s \n", mParamValue);
	    if ( recorder->setParameters(svc_layer) < 0 ) {
	        VTC_LOGD("error while configuring svc layer\n");
	        return -1;
	    }
	}

#if 0
    sprintf(mParamValue,"time-lapse-enable=1");
    String8 tl_enable(mParamValue);
    if ( recorder->setParameters(tl_enable) < 0 ) {
        VTC_LOGD("error while configuring bit rate\n");
        return -1;
    }

    sprintf(mParamValue,"time-lapse-fps=%u", mVideoFrameRate);
    String8 tl_fps(mParamValue);
    if ( recorder->setParameters(tl_fps) < 0 ) {
        VTC_LOGD("error while configuring time lapse fps\n");
        return -1;
    }
#endif

    sprintf(mParamValue,"video-param-i-frames-interval=%u", mIFramesIntervalSec);
    String8 interval(mParamValue);
    if ( recorder->setParameters(interval) < 0 ) {
        VTC_LOGD("error while configuring i-frame interval\n");
        return -1;
    }

    if ( recorder->prepare() < 0 ) {
        VTC_LOGD("recorder prepare failed\n");
        return -1;
    }

    if ( recorder->start() < 0 ) {
        VTC_LOGD("recorder start failed\n");
        return -1;
    }

    bRecording = true;
    return 0;
}

int startRecording_svcSpace() {

    if (camera.get() == NULL) return -1;

    recorder = new MediaRecorder(String16());

    if ( NULL == recorder.get() ) {
        VTC_LOGD("Error while creating MediaRecorder\n");
        return -1;
    }

    camera->unlock();

    if ( recorder->setCamera(camera->remote(), camera->getRecordingProxy()) < 0 ) {
        VTC_LOGD("error while setting the camera\n");
        return -1;
    }

    if ( recorder->setVideoSource(1 /*VIDEO_SOURCE_CAMERA*/) < 0 ) {
        VTC_LOGD("error while configuring camera video source\n");
        return -1;
    }
#if 0
    if ( recorder->setAudioSource(AUDIO_SOURCE_DEFAULT) < 0 ) {
        VTC_LOGD("error while configuring camera audio source\n");
        return -1;
    }
#endif
    if ( recorder->setOutputFormat(OUTPUT_FORMAT_MPEG_4) < 0 ) {
        VTC_LOGD("error while configuring output format\n");
        return -1;
    }

    //if(mkdir("/mnt/obb/vtc",0777) == -1)
         //VTC_LOGD("\n Directory \'vtc\' was not created or maybe it was already created \n");

    mOutputFd = open(mRecordFileName, O_CREAT | O_RDWR |O_APPEND);

    if(mOutputFd < 0){
        VTC_LOGD("Error while creating video filename\n");
        return -1;
    }

    if ( recorder->setOutputFile(mOutputFd, 0, 0) < 0 ) {
        VTC_LOGD("error while configuring video filename\n");

        return -1;
    }

    if ( recorder->setVideoFrameRate(mVideoFrameRate) < 0 ) {
        VTC_LOGD("error while configuring video framerate\n");
        return -1;
    }

    if ( recorder->setVideoSize(mPreviewWidth, mPreviewHeight) < 0 ) {
        VTC_LOGD("error while configuring video size\n");
        return -1;
    }

    if ( recorder->setVideoEncoder(mVideoCodec) < 0 ) {
        VTC_LOGD("error while configuring video codec\n");
        return -1;
    }
#if 0
    if ( recorder->setAudioEncoder(AUDIO_ENCODER_AMR_NB) < 0 ) {
        VTC_LOGD("error while configuring audio codec\n");
        return -1;
    }
#endif
    if ( recorder->setPreviewSurface(previewSurface->getIGraphicBufferProducer()) < 0 ) {
        VTC_LOGD("error while configuring preview surface\n");
        return -1;
    }

    sprintf(mParamValue,"video-param-encoding-bitrate=%u", mVideoBitRate);
    String8 bit_rate(mParamValue);
    if ( recorder->setParameters(bit_rate) < 0 ) {
        VTC_LOGD("error while configuring bit rate\n");
        return -1;
    }

    sprintf(mParamValue,"video-param-svc-layer=%u", 55);
    String8 svc_layer(mParamValue);
    printf("svc layer setting %s \n", mParamValue);
    if ( recorder->setParameters(svc_layer) < 0 ) {
        VTC_LOGD("error while configuring svc layer\n");
        return -1;
    }

#if 0
    sprintf(mParamValue,"time-lapse-enable=1");
    String8 tl_enable(mParamValue);
    if ( recorder->setParameters(tl_enable) < 0 ) {
        VTC_LOGD("error while configuring bit rate\n");
        return -1;
    }

    sprintf(mParamValue,"time-lapse-fps=%u", mVideoFrameRate);
    String8 tl_fps(mParamValue);
    if ( recorder->setParameters(tl_fps) < 0 ) {
        VTC_LOGD("error while configuring time lapse fps\n");
        return -1;
    }
#endif

    sprintf(mParamValue,"video-param-i-frames-interval=%u", mIFramesIntervalSec);
    String8 interval(mParamValue);
    if ( recorder->setParameters(interval) < 0 ) {
        VTC_LOGD("error while configuring i-frame interval\n");
        return -1;
    }

    if ( recorder->prepare() < 0 ) {
        VTC_LOGD("recorder prepare failed\n");
        return -1;
    }

    if ( recorder->start() < 0 ) {
        VTC_LOGD("recorder start failed\n");
        return -1;
    }

    bRecording = true;
    return 0;
}

int stopRecording() {

    VTC_LOGD("stopRecording()");
    if (camera.get() == NULL) return -1;

    if ( NULL == recorder.get() ) {
        VTC_LOGD("invalid recorder reference\n");
        return -1;
    }

    if ( recorder->stop() < 0 ) {
        VTC_LOGD("recorder failed to stop\n");
        return -1;
    }

    recorder->release();
    recorder.clear();

    if ( 0 < mOutputFd ) {
        close(mOutputFd);
    }

    return 0;
}


int startPreview() {
    char value[PROPERTY_VALUE_MAX];
    //property_get("disable.VSTAB.VNF", value, "0");
    //int disable_VTAB_and_VNF = atoi(value);
    mCameraThrewError = false;
    bRecording = false;

    createPreviewSurface();
#if 1
    camera = Camera::connect(camera_index, String16(), -1, -1 );
#else
    status_t status = Camera::connectLegacy(camera_index, 550, String16(),
                Camera::USE_CALLING_UID, camera);
    if (status != NO_ERROR) {
        VTC_LOGE("camera connectLegacy failed");
        //return status;
    }
#endif
    if (camera.get() == NULL){
        VTC_LOGE("camera.get() =================== NULL");
        return -1;
    }

    VTC_LOGD("\n\n mPreviewWidth = %d, mPreviewHeight = %d, mVideoFrameRate = %d, mVideoBitRate = %d \n\n\n",
        mPreviewWidth, mPreviewHeight, mVideoFrameRate, mVideoBitRate);

    params.unflatten(camera->getParameters());
    params.setPreviewSize(mPreviewWidth, mPreviewHeight);
    params.setPreviewFrameRate(30/*mVideoFrameRate*/);
    params.set(CameraParameters::KEY_RECORDING_HINT, CameraParameters::TRUE);// Set recording hint, otherwise it defaults to high-quality and there is not enough memory available to do playback and camera preview simultaneously!
    //sprintf(mParamValue,"%u,%u", 30/*mVideoFrameRate*/*1000, 30/*mVideoFrameRate*/*1000);
    //params.set("preview-fps-range", mParamValue);

    //if(disable_VTAB_and_VNF){
    //    VTC_LOGI("\n\n\nDisabling VSTAB & VNF (noise reduction)\n\n");
    //    params.set("vstab" , 0);
    //    params.set("vnf", 0);
    //}
    params.set("video-size", "1920x1080");
    //params.set("antibanding", "off");
    //params.setFocusMode(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
    //params.set("auto-exposure", "frame-average");
    //params.set("zsl", "off");
    //params.set("video-hdr", "off");
    params.set("video-hfr", "off");
    params.set("video-hsr", "30");
    sprintf(mParamValue,"%u,%u", 30/*mVideoFrameRate*/*1000, 30/*mVideoFrameRate*/*1000);
    //sprintf(mParamValue,"%u,%u", mVideoFrameRate*1000, mVideoFrameRate*1000);
    params.set("preview-fps-range", mParamValue);
    //camera->storeMetaDataInBuffers(true);
    usleep(100*1000);
    camera->setParameters(params.flatten());
    camera->setPreviewTarget(previewSurface->getIGraphicBufferProducer());
    mCameraListener = new MyCameraListener();
    camera->setListener(mCameraListener);

    params.unflatten(camera->getParameters());
    VTC_LOGD("get(preview-fps-range) = %s\n", params.get("preview-fps-range"));
    VTC_LOGD("get(preview-fps-range-values) = %s\n", params.get("preview-fps-range-values"));
    VTC_LOGD("get(preview-size-values) = %s\n", params.get("preview-size-values"));
    VTC_LOGD("get(preview-frame-rate-values) = %s\n", params.get("preview-frame-rate-values"));
    VTC_LOGD("get(video-hfr-values) = %s\n", params.get("video-hfr-values"));
    VTC_LOGD("get(video-hsr) = %s\n", params.get("video-hsr"));
    VTC_LOGD("get(video-hfr) = %s\n", params.get("video-hfr"));

    VTC_LOGD("Starting preview\n");
    camera->startPreview();
    sleep(SLEEP_AFTER_STARTING_PREVIEW);
    return 0;
}

void stopPreview() {
    if (camera.get() == NULL) return;
    camera->stopPreview();
    camera->disconnect();
    camera.clear();
    mCameraListener.clear();
    destroyPreviewSurface();
}

int startPlayback() {

    playbackSurfaceControl = client->createSurface(String8("jglSurface"), playbackSurfaceWidth, playbackSurfaceHeight, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());

    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(playerWinX, playerWinY);
    playbackSurfaceControl->setSize(playbackSurfaceWidth, playbackSurfaceHeight);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open(mPlaybackFileName, O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);
    bPlaying = true;
    return 0;
}

int startPlayback2() {

//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 960, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(960, 540);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();



    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open(mPlaybackFileName, O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

	printf("test playback 1+1 in one process \n");
	//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 959, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(961, 0);
    playbackSurfaceControl2->setSize(959, 540);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open(mPlaybackFileName2, O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

    bPlaying = true;
    return 0;
}

int startPlayback2x2() {
    printf("test startPlayback_layout1 \n");
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 960, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(960, 540);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 960, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(960, 0);
    playbackSurfaceControl2->setSize(960, 540);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

//playback 3
    playbackSurfaceControl3 = client->createSurface(String8("jglSurface3"), 960, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl3 != NULL);
    CHECK(playbackSurfaceControl3->isValid());
	
    playbackSurface3 = playbackSurfaceControl3->getSurface();
    CHECK(playbackSurface3 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(960, 540);
    playbackSurfaceControl3->setSize(960, 540);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

    player3 = new MediaPlayer();
    mPlayerListener3 = new PlayerListener(player3);
    mMediaPlayerThrewError = false;
    player3->setListener(mPlayerListener3);
    int fd3 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize3 = lseek64(fd3, 0, SEEK_END);
    player3->setDataSource(fd3, 0, fileSize3);
    player3->setVideoSurfaceTexture(playbackSurface3->getIGraphicBufferProducer());
    player3->prepareAsync();
    player3->setLooping(true);

//playback 4
    playbackSurfaceControl4 = client->createSurface(String8("jglSurface4"), 960, 540, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl4 != NULL);
    CHECK(playbackSurfaceControl4->isValid());
	
    playbackSurface4 = playbackSurfaceControl4->getSurface();
    CHECK(playbackSurface4 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 540);
    playbackSurfaceControl4->setSize(960, 540);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    player4 = new MediaPlayer();
    mPlayerListener4 = new PlayerListener(player4);
    mMediaPlayerThrewError = false;
    player4->setListener(mPlayerListener4);
    int fd4 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize4 = lseek64(fd4, 0, SEEK_END);
    player4->setDataSource(fd4, 0, fileSize4);
    player4->setVideoSurfaceTexture(playbackSurface4->getIGraphicBufferProducer());
    player4->prepareAsync();
    player4->setLooping(true);


    bPlaying = true;
    return 0;
}

int startPlayback4dec_1rec() {
	cameraWinX=1280;
	cameraWinY=720;
	cameraSurfaceWidth=640;
	cameraSurfaceHeight=360;
	mPreviewWidth=1920;
	mPreviewHeight=1080;
	
	startPreview();
	startRecording();
    printf("test startPlayback_layout1 \n");
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(640, 0);
    playbackSurfaceControl2->setSize(640, 360);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

//playback 3
    playbackSurfaceControl3 = client->createSurface(String8("jglSurface3"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl3 != NULL);
    CHECK(playbackSurfaceControl3->isValid());
	
    playbackSurface3 = playbackSurfaceControl3->getSurface();
    CHECK(playbackSurface3 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(640, 360);
    playbackSurfaceControl3->setSize(640, 360);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

    player3 = new MediaPlayer();
    mPlayerListener3 = new PlayerListener(player3);
    mMediaPlayerThrewError = false;
    player3->setListener(mPlayerListener3);
    int fd3 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize3 = lseek64(fd3, 0, SEEK_END);
    player3->setDataSource(fd3, 0, fileSize3);
    player3->setVideoSurfaceTexture(playbackSurface3->getIGraphicBufferProducer());
    player3->prepareAsync();
    player3->setLooping(true);

//playback 4
    playbackSurfaceControl4 = client->createSurface(String8("jglSurface4"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl4 != NULL);
    CHECK(playbackSurfaceControl4->isValid());
	
    playbackSurface4 = playbackSurfaceControl4->getSurface();
    CHECK(playbackSurface4 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 360);
    playbackSurfaceControl4->setSize(640, 360);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    player4 = new MediaPlayer();
    mPlayerListener4 = new PlayerListener(player4);
    mMediaPlayerThrewError = false;
    player4->setListener(mPlayerListener4);
    int fd4 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize4 = lseek64(fd4, 0, SEEK_END);
    player4->setDataSource(fd4, 0, fileSize4);
    player4->setVideoSurfaceTexture(playbackSurface4->getIGraphicBufferProducer());
    player4->prepareAsync();
    player4->setLooping(true);


    bPlaying = true;
    return 0;
}

int startPlayback2x2_layout3() {
	
    printf("test startPlayback2x2_layout3 \n");
//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(640, 0);
    playbackSurfaceControl2->setSize(640, 360);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(640, 360);
    playbackSurfaceControl3->setSize(640, 360);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 360);
    playbackSurfaceControl4->setSize(640, 360);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();
    if(surfaceControl != NULL){
	    client->openGlobalTransaction();
	    surfaceControl->setLayer(0x7fffffff);
	    surfaceControl->setPosition(cameraWinX, cameraWinY);
	    surfaceControl->setSize(cameraSurfaceWidth, cameraSurfaceHeight);
	    surfaceControl->show();
	    client->closeGlobalTransaction();
    }

    return 0;
}

int start2dec_1rec() {
	cameraWinX=1280;
	cameraWinY=720;
	cameraSurfaceWidth=640;
	cameraSurfaceHeight=360;
	mPreviewWidth=1920;
	mPreviewHeight=1080;
	
	startPreview();
	startRecording();
    printf("test startPlayback_layout1 \n");
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(1280, 360);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/720.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 1280, 720, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(0, 0);
    playbackSurfaceControl2->setSize(1280, 720);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/1080.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

    bPlaying = true;
    return 0;
}

int startPlayback2x2_layout2() {
	printf("test startPlayback_layout2 \n");

//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(960, 540);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(960, 0);
    playbackSurfaceControl2->setSize(960, 540);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(960, 540);
    playbackSurfaceControl3->setSize(960, 540);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 540);
    playbackSurfaceControl4->setSize(960, 540);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();
    if(surfaceControl != NULL){
	    client->openGlobalTransaction();
	    surfaceControl->setLayer(0x8fffffff);
	    surfaceControl->setPosition(cameraWinX, cameraWinY);
	    surfaceControl->setSize(cameraSurfaceWidth, cameraSurfaceHeight);
	    surfaceControl->show();
	    client->closeGlobalTransaction();
    }

    return 0;
}

int startPlayback2x2_layout1() {
	printf("test startPlayback_layout1 \n");

//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(240, 0);
    playbackSurfaceControl->setSize(1440, 810);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();
//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(240, 810);
    playbackSurfaceControl2->setSize(480, 270);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(720, 810);
    playbackSurfaceControl3->setSize(480, 270);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(1200, 810);
    playbackSurfaceControl4->setSize(480, 270);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    if(surfaceControl != NULL){
	    client->openGlobalTransaction();
	    surfaceControl->setLayer(0x8fffffff);
	    surfaceControl->setPosition(cameraWinX, cameraWinY);
	    surfaceControl->setSize(cameraSurfaceWidth, cameraSurfaceHeight);
	    surfaceControl->show();
	    client->closeGlobalTransaction();
    }
    return 0;
}

int startPlayback3x3() {
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

	printf("test playback 3x3 in one process \n");
//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(640, 0);
    playbackSurfaceControl2->setSize(640, 360);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

//playback 3
    playbackSurfaceControl3 = client->createSurface(String8("jglSurface3"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl3 != NULL);
    CHECK(playbackSurfaceControl3->isValid());
	
    playbackSurface3 = playbackSurfaceControl3->getSurface();
    CHECK(playbackSurface3 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(1280, 0);
    playbackSurfaceControl3->setSize(640, 360);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

    player3 = new MediaPlayer();
    mPlayerListener3 = new PlayerListener(player3);
    mMediaPlayerThrewError = false;
    player3->setListener(mPlayerListener3);
    int fd3 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize3 = lseek64(fd3, 0, SEEK_END);
    player3->setDataSource(fd3, 0, fileSize3);
    player3->setVideoSurfaceTexture(playbackSurface3->getIGraphicBufferProducer());
    player3->prepareAsync();
    player3->setLooping(true);

//playback 4
    playbackSurfaceControl4 = client->createSurface(String8("jglSurface4"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl4 != NULL);
    CHECK(playbackSurfaceControl4->isValid());
	
    playbackSurface4 = playbackSurfaceControl4->getSurface();
    CHECK(playbackSurface4 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 360);
    playbackSurfaceControl4->setSize(640, 360);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    player4 = new MediaPlayer();
    mPlayerListener4 = new PlayerListener(player4);
    mMediaPlayerThrewError = false;
    player4->setListener(mPlayerListener4);
    int fd4 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize4 = lseek64(fd4, 0, SEEK_END);
    player4->setDataSource(fd4, 0, fileSize4);
    player4->setVideoSurfaceTexture(playbackSurface4->getIGraphicBufferProducer());
    player4->prepareAsync();
    player4->setLooping(true);

//playback 5
    playbackSurfaceControl5 = client->createSurface(String8("jglSurface5"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl5 != NULL);
    CHECK(playbackSurfaceControl5->isValid());
	
    playbackSurface5 = playbackSurfaceControl5->getSurface();
    CHECK(playbackSurface5 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl5->setLayer(0x7fffffff);
    playbackSurfaceControl5->setPosition(640, 360);
    playbackSurfaceControl5->setSize(640, 360);
    playbackSurfaceControl5->show();
    client->closeGlobalTransaction();

    player5 = new MediaPlayer();
    mPlayerListener5 = new PlayerListener(player5);
    mMediaPlayerThrewError = false;
    player5->setListener(mPlayerListener5);
    int fd5 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize5 = lseek64(fd5, 0, SEEK_END);
    player5->setDataSource(fd5, 0, fileSize5);
    player5->setVideoSurfaceTexture(playbackSurface5->getIGraphicBufferProducer());
    player5->prepareAsync();
    player5->setLooping(true);

//playback 6
    playbackSurfaceControl6 = client->createSurface(String8("jglSurface6"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl6 != NULL);
    CHECK(playbackSurfaceControl6->isValid());
	
    playbackSurface6 = playbackSurfaceControl6->getSurface();
    CHECK(playbackSurface6 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl6->setLayer(0x7fffffff);
    playbackSurfaceControl6->setPosition(1280, 360);
    playbackSurfaceControl6->setSize(640, 360);
    playbackSurfaceControl6->show();
    client->closeGlobalTransaction();

    player6 = new MediaPlayer();
    mPlayerListener6 = new PlayerListener(player6);
    mMediaPlayerThrewError = false;
    player6->setListener(mPlayerListener6);
    int fd6 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize6 = lseek64(fd6, 0, SEEK_END);
    player6->setDataSource(fd6, 0, fileSize6);
    player6->setVideoSurfaceTexture(playbackSurface6->getIGraphicBufferProducer());
    player6->prepareAsync();
    player6->setLooping(true);

//playback 7
    playbackSurfaceControl7 = client->createSurface(String8("jglSurface7"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl7 != NULL);
    CHECK(playbackSurfaceControl7->isValid());
	
    playbackSurface7 = playbackSurfaceControl7->getSurface();
    CHECK(playbackSurface7 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl7->setLayer(0x7fffffff);
    playbackSurfaceControl7->setPosition(0, 720);
    playbackSurfaceControl7->setSize(640, 360);
    playbackSurfaceControl7->show();
    client->closeGlobalTransaction();

    player7 = new MediaPlayer();
    mPlayerListener7 = new PlayerListener(player7);
    mMediaPlayerThrewError = false;
    player7->setListener(mPlayerListener7);
    int fd7 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize7 = lseek64(fd7, 0, SEEK_END);
    player7->setDataSource(fd7, 0, fileSize7);
    player7->setVideoSurfaceTexture(playbackSurface7->getIGraphicBufferProducer());
    player7->prepareAsync();
    player7->setLooping(true);


//playback 8
    playbackSurfaceControl8 = client->createSurface(String8("jglSurface8"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl8 != NULL);
    CHECK(playbackSurfaceControl8->isValid());
	
    playbackSurface8 = playbackSurfaceControl8->getSurface();
    CHECK(playbackSurface8 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl8->setLayer(0x7fffffff);
    playbackSurfaceControl8->setPosition(640, 720);
    playbackSurfaceControl8->setSize(640, 360);
    playbackSurfaceControl8->show();
    client->closeGlobalTransaction();

    player8 = new MediaPlayer();
    mPlayerListener8 = new PlayerListener(player8);
    mMediaPlayerThrewError = false;
    player8->setListener(mPlayerListener8);
    int fd8 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize8 = lseek64(fd8, 0, SEEK_END);
    player8->setDataSource(fd8, 0, fileSize8);
    player8->setVideoSurfaceTexture(playbackSurface8->getIGraphicBufferProducer());
    player8->prepareAsync();
    player8->setLooping(true);

//playback 9
    playbackSurfaceControl9 = client->createSurface(String8("jglSurface9"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl9 != NULL);
    CHECK(playbackSurfaceControl9->isValid());
	
    playbackSurface9 = playbackSurfaceControl9->getSurface();
    CHECK(playbackSurface9 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl9->setLayer(0x7fffffff);
    playbackSurfaceControl9->setPosition(1280, 720);
    playbackSurfaceControl9->setSize(640, 360);
    playbackSurfaceControl9->show();
    client->closeGlobalTransaction();

    player9 = new MediaPlayer();
    mPlayerListener9 = new PlayerListener(player9);
    mMediaPlayerThrewError = false;
    player9->setListener(mPlayerListener9);
    int fd9 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize9 = lseek64(fd9, 0, SEEK_END);
    player9->setDataSource(fd9, 0, fileSize9);
    player9->setVideoSurfaceTexture(playbackSurface9->getIGraphicBufferProducer());
    player9->prepareAsync();
    player9->setLooping(true);

    bPlaying = true;
    return 0;
}

int startPlayback3x3_1dec() {
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

	printf("test playback 3x3 in one process \n");
//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(640, 0);
    playbackSurfaceControl2->setSize(640, 360);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

//playback 3
    playbackSurfaceControl3 = client->createSurface(String8("jglSurface3"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl3 != NULL);
    CHECK(playbackSurfaceControl3->isValid());
	
    playbackSurface3 = playbackSurfaceControl3->getSurface();
    CHECK(playbackSurface3 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(1280, 0);
    playbackSurfaceControl3->setSize(640, 360);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

    player3 = new MediaPlayer();
    mPlayerListener3 = new PlayerListener(player3);
    mMediaPlayerThrewError = false;
    player3->setListener(mPlayerListener3);
    int fd3 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize3 = lseek64(fd3, 0, SEEK_END);
    player3->setDataSource(fd3, 0, fileSize3);
    player3->setVideoSurfaceTexture(playbackSurface3->getIGraphicBufferProducer());
    player3->prepareAsync();
    player3->setLooping(true);

//playback 4
    playbackSurfaceControl4 = client->createSurface(String8("jglSurface4"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl4 != NULL);
    CHECK(playbackSurfaceControl4->isValid());
	
    playbackSurface4 = playbackSurfaceControl4->getSurface();
    CHECK(playbackSurface4 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 360);
    playbackSurfaceControl4->setSize(640, 360);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    player4 = new MediaPlayer();
    mPlayerListener4 = new PlayerListener(player4);
    mMediaPlayerThrewError = false;
    player4->setListener(mPlayerListener4);
    int fd4 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize4 = lseek64(fd4, 0, SEEK_END);
    player4->setDataSource(fd4, 0, fileSize4);
    player4->setVideoSurfaceTexture(playbackSurface4->getIGraphicBufferProducer());
    player4->prepareAsync();
    player4->setLooping(true);

//playback 5
    playbackSurfaceControl5 = client->createSurface(String8("jglSurface5"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl5 != NULL);
    CHECK(playbackSurfaceControl5->isValid());
	
    playbackSurface5 = playbackSurfaceControl5->getSurface();
    CHECK(playbackSurface5 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl5->setLayer(0x7fffffff);
    playbackSurfaceControl5->setPosition(640, 360);
    playbackSurfaceControl5->setSize(640, 360);
    playbackSurfaceControl5->show();
    client->closeGlobalTransaction();

    player5 = new MediaPlayer();
    mPlayerListener5 = new PlayerListener(player5);
    mMediaPlayerThrewError = false;
    player5->setListener(mPlayerListener5);
    int fd5 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize5 = lseek64(fd5, 0, SEEK_END);
    player5->setDataSource(fd5, 0, fileSize5);
    player5->setVideoSurfaceTexture(playbackSurface5->getIGraphicBufferProducer());
    player5->prepareAsync();
    player5->setLooping(true);

//playback 6
    playbackSurfaceControl6 = client->createSurface(String8("jglSurface6"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl6 != NULL);
    CHECK(playbackSurfaceControl6->isValid());
	
    playbackSurface6 = playbackSurfaceControl6->getSurface();
    CHECK(playbackSurface6 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl6->setLayer(0x7fffffff);
    playbackSurfaceControl6->setPosition(1280, 360);
    playbackSurfaceControl6->setSize(640, 360);
    playbackSurfaceControl6->show();
    client->closeGlobalTransaction();

    player6 = new MediaPlayer();
    mPlayerListener6 = new PlayerListener(player6);
    mMediaPlayerThrewError = false;
    player6->setListener(mPlayerListener6);
    int fd6 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize6 = lseek64(fd6, 0, SEEK_END);
    player6->setDataSource(fd6, 0, fileSize6);
    player6->setVideoSurfaceTexture(playbackSurface6->getIGraphicBufferProducer());
    player6->prepareAsync();
    player6->setLooping(true);

//playback 7
    playbackSurfaceControl7 = client->createSurface(String8("jglSurface7"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl7 != NULL);
    CHECK(playbackSurfaceControl7->isValid());
	
    playbackSurface7 = playbackSurfaceControl7->getSurface();
    CHECK(playbackSurface7 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl7->setLayer(0x7fffffff);
    playbackSurfaceControl7->setPosition(0, 720);
    playbackSurfaceControl7->setSize(640, 360);
    playbackSurfaceControl7->show();
    client->closeGlobalTransaction();

    player7 = new MediaPlayer();
    mPlayerListener7 = new PlayerListener(player7);
    mMediaPlayerThrewError = false;
    player7->setListener(mPlayerListener7);
    int fd7 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize7 = lseek64(fd7, 0, SEEK_END);
    player7->setDataSource(fd7, 0, fileSize7);
    player7->setVideoSurfaceTexture(playbackSurface7->getIGraphicBufferProducer());
    player7->prepareAsync();
    player7->setLooping(true);


//playback 8
    playbackSurfaceControl8 = client->createSurface(String8("jglSurface8"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl8 != NULL);
    CHECK(playbackSurfaceControl8->isValid());
	
    playbackSurface8 = playbackSurfaceControl8->getSurface();
    CHECK(playbackSurface8 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl8->setLayer(0x7fffffff);
    playbackSurfaceControl8->setPosition(640, 720);
    playbackSurfaceControl8->setSize(640, 360);
    playbackSurfaceControl8->show();
    client->closeGlobalTransaction();

    player8 = new MediaPlayer();
    mPlayerListener8 = new PlayerListener(player8);
    mMediaPlayerThrewError = false;
    player8->setListener(mPlayerListener8);
    int fd8 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize8 = lseek64(fd8, 0, SEEK_END);
    player8->setDataSource(fd8, 0, fileSize8);
    player8->setVideoSurfaceTexture(playbackSurface8->getIGraphicBufferProducer());
    player8->prepareAsync();
    player8->setLooping(true);

//playback 9
    playbackSurfaceControl9 = client->createSurface(String8("jglSurface9"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl9 != NULL);
    CHECK(playbackSurfaceControl9->isValid());
	
    playbackSurface9 = playbackSurfaceControl9->getSurface();
    CHECK(playbackSurface9 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl9->setLayer(0x7fffffff);
    playbackSurfaceControl9->setPosition(1280, 720);
    playbackSurfaceControl9->setSize(640, 360);
    playbackSurfaceControl9->show();
    client->closeGlobalTransaction();

    player9 = new MediaPlayer();
    mPlayerListener9 = new PlayerListener(player9);
    mMediaPlayerThrewError = false;
    player9->setListener(mPlayerListener9);
    int fd9 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize9 = lseek64(fd9, 0, SEEK_END);
    player9->setDataSource(fd9, 0, fileSize9);
    player9->setVideoSurfaceTexture(playbackSurface9->getIGraphicBufferProducer());
    player9->prepareAsync();
    player9->setLooping(true);

    //playback 10
    playbackSurfaceControl10 = client->createSurface(String8("jglSurface10"), 640, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl10 != NULL);
    CHECK(playbackSurfaceControl10->isValid());

    playbackSurface10 = playbackSurfaceControl10->getSurface();
    CHECK(playbackSurface10 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl10->setLayer(0x7fffffff);
    playbackSurfaceControl10->setPosition(320, 180);
    playbackSurfaceControl10->setSize(640, 360);
    playbackSurfaceControl10->show();
    client->closeGlobalTransaction();

    player10 = new MediaPlayer();
    mPlayerListener10 = new PlayerListener(player10);
    mMediaPlayerThrewError = false;
    player10->setListener(mPlayerListener10);
    int fd10 = open("/data/1080.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize10 = lseek64(fd10, 0, SEEK_END);
    player10->setDataSource(fd10, 0, fileSize10);
    player10->setVideoSurfaceTexture(playbackSurface10->getIGraphicBufferProducer());
    player10->prepareAsync();
    player10->setLooping(true);

    bPlaying = true;
    return 0;
}

int start9dec_1enc_1dec() {
	cameraWinX=0;
	cameraWinY=0;
	cameraSurfaceWidth=1600;
	cameraSurfaceHeight=900;
//	mPreviewWidth=1920;
//	mPreviewHeight=1080;

	startPreview();
	startRecording();

    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
//playback 1
    playbackSurfaceControl = client->createSurface(String8("jglSurface"), 320, 360, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl != NULL);
    CHECK(playbackSurfaceControl->isValid());
    playbackSurface = playbackSurfaceControl->getSurface();
    CHECK(playbackSurface != NULL);

    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(1600, 0);
    playbackSurfaceControl->setSize(320, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

    player = new MediaPlayer();
    mPlayerListener = new PlayerListener(player);
    mMediaPlayerThrewError = false;
    player->setListener(mPlayerListener);
    int fd = open("/data/1080.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize = lseek64(fd, 0, SEEK_END);
    player->setDataSource(fd, 0, fileSize);
    player->setVideoSurfaceTexture(playbackSurface->getIGraphicBufferProducer());
    player->prepareAsync();
    player->setLooping(true);

	printf("test playback 3x3 in one process \n");
//playback 2
    playbackSurfaceControl2 = client->createSurface(String8("jglSurface2"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl2 != NULL);
    CHECK(playbackSurfaceControl2->isValid());
	
    playbackSurface2 = playbackSurfaceControl2->getSurface();
    CHECK(playbackSurface2 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(1600, 360);
    playbackSurfaceControl2->setSize(320, 180);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

    player2 = new MediaPlayer();
    mPlayerListener2 = new PlayerListener(player2);
    mMediaPlayerThrewError = false;
    player2->setListener(mPlayerListener2);
    int fd2 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize2 = lseek64(fd2, 0, SEEK_END);
    player2->setDataSource(fd2, 0, fileSize2);
    player2->setVideoSurfaceTexture(playbackSurface2->getIGraphicBufferProducer());
    player2->prepareAsync();
    player2->setLooping(true);

//playback 3
    playbackSurfaceControl3 = client->createSurface(String8("jglSurface3"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl3 != NULL);
    CHECK(playbackSurfaceControl3->isValid());
	
    playbackSurface3 = playbackSurfaceControl3->getSurface();
    CHECK(playbackSurface3 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(1600, 540);
    playbackSurfaceControl3->setSize(320, 180);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

    player3 = new MediaPlayer();
    mPlayerListener3 = new PlayerListener(player3);
    mMediaPlayerThrewError = false;
    player3->setListener(mPlayerListener3);
    int fd3 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize3 = lseek64(fd3, 0, SEEK_END);
    player3->setDataSource(fd3, 0, fileSize3);
    player3->setVideoSurfaceTexture(playbackSurface3->getIGraphicBufferProducer());
    player3->prepareAsync();
    player3->setLooping(true);

//playback 4
    playbackSurfaceControl4 = client->createSurface(String8("jglSurface4"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl4 != NULL);
    CHECK(playbackSurfaceControl4->isValid());
	
    playbackSurface4 = playbackSurfaceControl4->getSurface();
    CHECK(playbackSurface4 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(1600, 720);
    playbackSurfaceControl4->setSize(320, 180);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

    player4 = new MediaPlayer();
    mPlayerListener4 = new PlayerListener(player4);
    mMediaPlayerThrewError = false;
    player4->setListener(mPlayerListener4);
    int fd4 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize4 = lseek64(fd4, 0, SEEK_END);
    player4->setDataSource(fd4, 0, fileSize4);
    player4->setVideoSurfaceTexture(playbackSurface4->getIGraphicBufferProducer());
    player4->prepareAsync();
    player4->setLooping(true);

//playback 5
    playbackSurfaceControl5 = client->createSurface(String8("jglSurface5"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl5 != NULL);
    CHECK(playbackSurfaceControl5->isValid());
	
    playbackSurface5 = playbackSurfaceControl5->getSurface();
    CHECK(playbackSurface5 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl5->setLayer(0x7fffffff);
    playbackSurfaceControl5->setPosition(1600, 900);
    playbackSurfaceControl5->setSize(320, 180);
    playbackSurfaceControl5->show();
    client->closeGlobalTransaction();

    player5 = new MediaPlayer();
    mPlayerListener5 = new PlayerListener(player5);
    mMediaPlayerThrewError = false;
    player5->setListener(mPlayerListener5);
    int fd5 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize5 = lseek64(fd5, 0, SEEK_END);
    player5->setDataSource(fd5, 0, fileSize5);
    player5->setVideoSurfaceTexture(playbackSurface5->getIGraphicBufferProducer());
    player5->prepareAsync();
    player5->setLooping(true);

//playback 6
    playbackSurfaceControl6 = client->createSurface(String8("jglSurface6"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl6 != NULL);
    CHECK(playbackSurfaceControl6->isValid());
	
    playbackSurface6 = playbackSurfaceControl6->getSurface();
    CHECK(playbackSurface6 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl6->setLayer(0x7fffffff);
    playbackSurfaceControl6->setPosition(1280, 900);
    playbackSurfaceControl6->setSize(320, 180);
    playbackSurfaceControl6->show();
    client->closeGlobalTransaction();

    player6 = new MediaPlayer();
    mPlayerListener6 = new PlayerListener(player6);
    mMediaPlayerThrewError = false;
    player6->setListener(mPlayerListener6);
    int fd6 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize6 = lseek64(fd6, 0, SEEK_END);
    player6->setDataSource(fd6, 0, fileSize6);
    player6->setVideoSurfaceTexture(playbackSurface6->getIGraphicBufferProducer());
    player6->prepareAsync();
    player6->setLooping(true);

//playback 7
    playbackSurfaceControl7 = client->createSurface(String8("jglSurface7"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl7 != NULL);
    CHECK(playbackSurfaceControl7->isValid());
	
    playbackSurface7 = playbackSurfaceControl7->getSurface();
    CHECK(playbackSurface7 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl7->setLayer(0x7fffffff);
    playbackSurfaceControl7->setPosition(960, 900);
    playbackSurfaceControl7->setSize(320, 180);
    playbackSurfaceControl7->show();
    client->closeGlobalTransaction();

    player7 = new MediaPlayer();
    mPlayerListener7 = new PlayerListener(player7);
    mMediaPlayerThrewError = false;
    player7->setListener(mPlayerListener7);
    int fd7 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize7 = lseek64(fd7, 0, SEEK_END);
    player7->setDataSource(fd7, 0, fileSize7);
    player7->setVideoSurfaceTexture(playbackSurface7->getIGraphicBufferProducer());
    player7->prepareAsync();
    player7->setLooping(true);


//playback 8
    playbackSurfaceControl8 = client->createSurface(String8("jglSurface8"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl8 != NULL);
    CHECK(playbackSurfaceControl8->isValid());
	
    playbackSurface8 = playbackSurfaceControl8->getSurface();
    CHECK(playbackSurface8 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl8->setLayer(0x7fffffff);
    playbackSurfaceControl8->setPosition(640, 900);
    playbackSurfaceControl8->setSize(320, 180);
    playbackSurfaceControl8->show();
    client->closeGlobalTransaction();

    player8 = new MediaPlayer();
    mPlayerListener8 = new PlayerListener(player8);
    mMediaPlayerThrewError = false;
    player8->setListener(mPlayerListener8);
    int fd8 = open("/data/jony360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize8 = lseek64(fd8, 0, SEEK_END);
    player8->setDataSource(fd8, 0, fileSize8);
    player8->setVideoSurfaceTexture(playbackSurface8->getIGraphicBufferProducer());
    player8->prepareAsync();
    player8->setLooping(true);

//playback 9
    playbackSurfaceControl9 = client->createSurface(String8("jglSurface9"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
    CHECK(playbackSurfaceControl9 != NULL);
    CHECK(playbackSurfaceControl9->isValid());
	
    playbackSurface9 = playbackSurfaceControl9->getSurface();
    CHECK(playbackSurface9 != NULL);
    client->openGlobalTransaction();
    playbackSurfaceControl9->setLayer(0x7fffffff);
    playbackSurfaceControl9->setPosition(320, 900);
    playbackSurfaceControl9->setSize(320, 180);
    playbackSurfaceControl9->show();
    client->closeGlobalTransaction();

    player9 = new MediaPlayer();
    mPlayerListener9 = new PlayerListener(player9);
    mMediaPlayerThrewError = false;
    player9->setListener(mPlayerListener9);
    int fd9 = open("/data/city360.mp4", O_RDONLY | O_LARGEFILE);
    uint64_t fileSize9 = lseek64(fd9, 0, SEEK_END);
    player9->setDataSource(fd9, 0, fileSize9);
    player9->setVideoSurfaceTexture(playbackSurface9->getIGraphicBufferProducer());
    player9->prepareAsync();
    player9->setLooping(true);
	
//	printf("test 10st pic \n");
//	//playback 10
//		playbackSurfaceControl10 = client->createSurface(String8("jglSurface10"), 320, 180, PIXEL_FORMAT_RGB_565, 0);
//		CHECK(playbackSurfaceControl10 != NULL);
//		CHECK(playbackSurfaceControl10->isValid());
//		
//		playbackSurface10 = playbackSurfaceControl10->getSurface();
//		CHECK(playbackSurface10 != NULL);
//		client->openGlobalTransaction();
//		playbackSurfaceControl10->setLayer(0x7fffffff+1);
//		playbackSurfaceControl10->setPosition(960, 540);
//		playbackSurfaceControl10->setSize(300, 180);
//		playbackSurfaceControl10->show();
//		client->closeGlobalTransaction();
//	
//		player10 = new MediaPlayer();
//		mPlayerListener10 = new PlayerListener(player10);
//		mMediaPlayerThrewError = false;
//		player10->setListener(mPlayerListener10);
//		int fd10 = open("/data/jony.mp4", O_RDONLY | O_LARGEFILE);
//		uint64_t fileSize10 = lseek64(fd10, 0, SEEK_END);
//		player10->setDataSource(fd10, 0, fileSize10);
//		player10->setVideoSurfaceTexture(playbackSurface10->getIGraphicBufferProducer());
//		player10->prepareAsync();
//		player10->setLooping(true);
//	
    bPlaying = true;
    return 0;
}

int startPlayback3x3_layout2() {

//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(640, 360);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();

//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(640, 0);
    playbackSurfaceControl2->setSize(640, 360);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(1280, 0);
    playbackSurfaceControl3->setSize(640, 360);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(0, 360);
    playbackSurfaceControl4->setSize(640, 360);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();

//playback 5
    client->openGlobalTransaction();
    playbackSurfaceControl5->setLayer(0x7fffffff);
    playbackSurfaceControl5->setPosition(640, 360);
    playbackSurfaceControl5->setSize(640, 360);
    playbackSurfaceControl5->show();
    client->closeGlobalTransaction();

//playback 6
    client->openGlobalTransaction();
    playbackSurfaceControl6->setLayer(0x7fffffff);
    playbackSurfaceControl6->setPosition(1280, 360);
    playbackSurfaceControl6->setSize(640, 360);
    playbackSurfaceControl6->show();
    client->closeGlobalTransaction();

//playback 7
    client->openGlobalTransaction();
    playbackSurfaceControl7->setLayer(0x7fffffff);
    playbackSurfaceControl7->setPosition(0, 720);
    playbackSurfaceControl7->setSize(640, 360);
    playbackSurfaceControl7->show();
    client->closeGlobalTransaction();

//playback 8
    client->openGlobalTransaction();
    playbackSurfaceControl8->setLayer(0x7fffffff);
    playbackSurfaceControl8->setPosition(640, 720);
    playbackSurfaceControl8->setSize(640, 360);
    playbackSurfaceControl8->show();
    client->closeGlobalTransaction();

//playback 9
    client->openGlobalTransaction();
    playbackSurfaceControl9->setLayer(0x7fffffff);
    playbackSurfaceControl9->setPosition(1280, 720);
    playbackSurfaceControl9->setSize(640, 360);
    playbackSurfaceControl9->show();
    client->closeGlobalTransaction();

    return 0;
}

int startPlayback3x3_layout1() {

//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->setLayer(0x7fffffff);
    playbackSurfaceControl->setPosition(0, 0);
    playbackSurfaceControl->setSize(1440, 810);
    playbackSurfaceControl->show();
    client->closeGlobalTransaction();


//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->setLayer(0x7fffffff);
    playbackSurfaceControl2->setPosition(1440, 0);
    playbackSurfaceControl2->setSize(480, 204);
    playbackSurfaceControl2->show();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->setLayer(0x7fffffff);
    playbackSurfaceControl3->setPosition(1440, 204);
    playbackSurfaceControl3->setSize(480, 202);
    playbackSurfaceControl3->show();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->setLayer(0x7fffffff);
    playbackSurfaceControl4->setPosition(1440, 406);
    playbackSurfaceControl4->setSize(480, 202);
    playbackSurfaceControl4->show();
    client->closeGlobalTransaction();


//playback 5
    client->openGlobalTransaction();
    playbackSurfaceControl5->setLayer(0x7fffffff);
    playbackSurfaceControl5->setPosition(1440, 608);
    playbackSurfaceControl5->setSize(480, 202);
    playbackSurfaceControl5->show();
    client->closeGlobalTransaction();

//playback 6
    client->openGlobalTransaction();
    playbackSurfaceControl6->setLayer(0x7fffffff);
    playbackSurfaceControl6->setPosition(1440, 810);
    playbackSurfaceControl6->setSize(480, 270);
    playbackSurfaceControl6->show();
    client->closeGlobalTransaction();

//playback 7
    client->openGlobalTransaction();
    playbackSurfaceControl7->setLayer(0x7fffffff);
    playbackSurfaceControl7->setPosition(960, 810);
    playbackSurfaceControl7->setSize(480, 270);
    playbackSurfaceControl7->show();
    client->closeGlobalTransaction();

//playback 8
    client->openGlobalTransaction();
    playbackSurfaceControl8->setLayer(0x7fffffff);
    playbackSurfaceControl8->setPosition(480, 810);
    playbackSurfaceControl8->setSize(480, 270);
    playbackSurfaceControl8->show();
    client->closeGlobalTransaction();

//playback 9
    client->openGlobalTransaction();
    playbackSurfaceControl9->setLayer(0x7fffffff);
    playbackSurfaceControl9->setPosition(0, 810);
    playbackSurfaceControl9->setSize(480, 270);
    playbackSurfaceControl9->show();
    client->closeGlobalTransaction();

    return 0;
}

int startPlayback3x3_hide() {

//playback 1
    client->openGlobalTransaction();
    playbackSurfaceControl->hide();
    client->closeGlobalTransaction();


//playback 2
    client->openGlobalTransaction();
    playbackSurfaceControl2->hide();
    client->closeGlobalTransaction();

//playback 3
    client->openGlobalTransaction();
    playbackSurfaceControl3->hide();
    client->closeGlobalTransaction();

//playback 4
    client->openGlobalTransaction();
    playbackSurfaceControl4->hide();
    client->closeGlobalTransaction();


//playback 5
    client->openGlobalTransaction();
    playbackSurfaceControl5->hide();
    client->closeGlobalTransaction();

//playback 6
    client->openGlobalTransaction();
    playbackSurfaceControl6->hide();
    client->closeGlobalTransaction();

//playback 7
    client->openGlobalTransaction();
    playbackSurfaceControl7->hide();
    client->closeGlobalTransaction();

//playback 8
    client->openGlobalTransaction();
    playbackSurfaceControl8->hide();
    client->closeGlobalTransaction();

//playback 9
    client->openGlobalTransaction();
    playbackSurfaceControl9->hide();
    client->closeGlobalTransaction();

    return 0;
}

int stopPlayback() {

    VTC_LOGD("%d: %s", __LINE__, __FUNCTION__);
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int stopPlayback2() {

    VTC_LOGD("%d: %s", __LINE__, __FUNCTION__);
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }

    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }
	
    if ( NULL != playbackSurfaceControl2.get() ) {
        playbackSurfaceControl2->clear();
        playbackSurfaceControl2.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int stopPlayback2x2() {
    VTC_LOGD("%d: %s \n", __LINE__, __FUNCTION__);
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }
    //stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }

    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->clear();
        playbackSurfaceControl2.clear();
    }

    //stop play3 
    player3->stop();
    player3->setListener(0);
    player3->disconnect();
    player3.clear();
    mPlayerListener3.clear();

    if ( NULL != playbackSurface3.get() ) {
        playbackSurface3.clear();
    }

    if ( NULL != playbackSurfaceControl3.get() ) {
        //playbackSurfaceControl3->clear();
        playbackSurfaceControl3.clear();
    }

    //stop play4 
    player4->stop();
    player4->setListener(0);
    player4->disconnect();
    player4.clear();
    mPlayerListener4.clear();

        if ( NULL != playbackSurface4.get() ) {
            playbackSurface4.clear();
        }

        if ( NULL != playbackSurfaceControl4.get() ) {
            //playbackSurfaceControl4->clear();
            playbackSurfaceControl4.clear();
        }
        if ( NULL != client.get() ) {
            client->dispose();
            client.clear();
        }
        return 0;
}

int stopPlayback4dec_1rec() {
    VTC_LOGD("%d: %s \n", __LINE__, __FUNCTION__);
    stopRecording();
    stopPreview();
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }
    //stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }

    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->clear();
        playbackSurfaceControl2.clear();
    }

    //stop play3 
    player3->stop();
    player3->setListener(0);
    player3->disconnect();
    player3.clear();
    mPlayerListener3.clear();

    if ( NULL != playbackSurface3.get() ) {
        playbackSurface3.clear();
    }

    if ( NULL != playbackSurfaceControl3.get() ) {
        //playbackSurfaceControl3->clear();
        playbackSurfaceControl3.clear();
    }

    //stop play4 
    player4->stop();
    player4->setListener(0);
    player4->disconnect();
    player4.clear();
    mPlayerListener4.clear();

    if ( NULL != playbackSurface4.get() ) {
    playbackSurface4.clear();
    }

    if ( NULL != playbackSurfaceControl4.get() ) {
    //playbackSurfaceControl4->clear();
    playbackSurfaceControl4.clear();
    }
    if ( NULL != client.get() ) {
    client->dispose();
    client.clear();
    }
    return 0;
}

int stop2dec_1rec() {
    VTC_LOGD("%d: %s \n", __LINE__, __FUNCTION__);
    stopRecording();
    stopPreview();
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }
    //stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }

    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->clear();
        playbackSurfaceControl2.clear();
    }
    if ( NULL != client.get() ) {
    client->dispose();
    client.clear();
    }
    return 0;
}

int stopPlayback3x3() {
    startPlayback3x3_hide();

    VTC_LOGD("%d: %s", __LINE__, __FUNCTION__);
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }

//stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }
	
    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->disconnect();
        playbackSurfaceControl2.clear();
    }

//stop play3 
    player3->stop();
    player3->setListener(0);
    player3->disconnect();
    player3.clear();
    mPlayerListener3.clear();

    if ( NULL != playbackSurface3.get() ) {
        playbackSurface3.clear();
    }
	
    if ( NULL != playbackSurfaceControl3.get() ) {
        //playbackSurfaceControl3->clear();
        playbackSurfaceControl3.clear();
    }

//stop play4 
    player4->stop();
    player4->setListener(0);
    player4->disconnect();
    player4.clear();
    mPlayerListener4.clear();

    if ( NULL != playbackSurface4.get() ) {
        playbackSurface4.clear();
    }
	
    if ( NULL != playbackSurfaceControl4.get() ) {
        //playbackSurfaceControl4->clear();
        playbackSurfaceControl4.clear();
    }

//stop play5 
    player5->stop();
    player5->setListener(0);
    player5->disconnect();
    player5.clear();
    mPlayerListener5.clear();

    if ( NULL != playbackSurface5.get() ) {
        playbackSurface5.clear();
    }
	
    if ( NULL != playbackSurfaceControl5.get() ) {
        //playbackSurfaceControl5->clear();
        playbackSurfaceControl5.clear();
    }

//stop play6 
    player6->stop();
    player6->setListener(0);
    player6->disconnect();
    player6.clear();
    mPlayerListener6.clear();

    if ( NULL != playbackSurface6.get() ) {
        playbackSurface6.clear();
    }
	
    if ( NULL != playbackSurfaceControl6.get() ) {
        //playbackSurfaceControl6->clear();
        playbackSurfaceControl6.clear();
    }

//stop play7 
    player7->stop();
    player7->setListener(0);
    player7->disconnect();
    player7.clear();
    mPlayerListener7.clear();

    if ( NULL != playbackSurface7.get() ) {
        playbackSurface7.clear();
    }
	
    if ( NULL != playbackSurfaceControl7.get() ) {
        //playbackSurfaceControl7->clear();
        playbackSurfaceControl7.clear();
    }

//stop play8 
    player8->stop();
    player8->setListener(0);
    player8->disconnect();
    player8.clear();
    mPlayerListener8.clear();

    if ( NULL != playbackSurface8.get() ) {
        playbackSurface8.clear();
    }
	
    if ( NULL != playbackSurfaceControl8.get() ) {
        //playbackSurfaceControl8->clear();
        playbackSurfaceControl8.clear();
    }

//stop play9 
    player9->stop();
    player9->setListener(0);
    player9->disconnect();
    player9.clear();
    mPlayerListener9.clear();

    if ( NULL != playbackSurface9.get() ) {
        playbackSurface9.clear();
    }
	
    if ( NULL != playbackSurfaceControl9.get() ) {
        //playbackSurfaceControl9->clear();
        playbackSurfaceControl9.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int stopPlayback3x3_1dec() {
    startPlayback3x3_hide();

    VTC_LOGD("%d: %s", __LINE__, __FUNCTION__);
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }

//stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }
	
    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->disconnect();
        playbackSurfaceControl2.clear();
    }

//stop play3 
    player3->stop();
    player3->setListener(0);
    player3->disconnect();
    player3.clear();
    mPlayerListener3.clear();

    if ( NULL != playbackSurface3.get() ) {
        playbackSurface3.clear();
    }
	
    if ( NULL != playbackSurfaceControl3.get() ) {
        //playbackSurfaceControl3->clear();
        playbackSurfaceControl3.clear();
    }

//stop play4 
    player4->stop();
    player4->setListener(0);
    player4->disconnect();
    player4.clear();
    mPlayerListener4.clear();

    if ( NULL != playbackSurface4.get() ) {
        playbackSurface4.clear();
    }
	
    if ( NULL != playbackSurfaceControl4.get() ) {
        //playbackSurfaceControl4->clear();
        playbackSurfaceControl4.clear();
    }

//stop play5 
    player5->stop();
    player5->setListener(0);
    player5->disconnect();
    player5.clear();
    mPlayerListener5.clear();

    if ( NULL != playbackSurface5.get() ) {
        playbackSurface5.clear();
    }
	
    if ( NULL != playbackSurfaceControl5.get() ) {
        //playbackSurfaceControl5->clear();
        playbackSurfaceControl5.clear();
    }

//stop play6 
    player6->stop();
    player6->setListener(0);
    player6->disconnect();
    player6.clear();
    mPlayerListener6.clear();

    if ( NULL != playbackSurface6.get() ) {
        playbackSurface6.clear();
    }
	
    if ( NULL != playbackSurfaceControl6.get() ) {
        //playbackSurfaceControl6->clear();
        playbackSurfaceControl6.clear();
    }

//stop play7 
    player7->stop();
    player7->setListener(0);
    player7->disconnect();
    player7.clear();
    mPlayerListener7.clear();

    if ( NULL != playbackSurface7.get() ) {
        playbackSurface7.clear();
    }
	
    if ( NULL != playbackSurfaceControl7.get() ) {
        //playbackSurfaceControl7->clear();
        playbackSurfaceControl7.clear();
    }

//stop play8 
    player8->stop();
    player8->setListener(0);
    player8->disconnect();
    player8.clear();
    mPlayerListener8.clear();

    if ( NULL != playbackSurface8.get() ) {
        playbackSurface8.clear();
    }
	
    if ( NULL != playbackSurfaceControl8.get() ) {
        //playbackSurfaceControl8->clear();
        playbackSurfaceControl8.clear();
    }

//stop play9 
    player9->stop();
    player9->setListener(0);
    player9->disconnect();
    player9.clear();
    mPlayerListener9.clear();

    if ( NULL != playbackSurface9.get() ) {
        playbackSurface9.clear();
    }
	
    if ( NULL != playbackSurfaceControl9.get() ) {
        //playbackSurfaceControl9->clear();
        playbackSurfaceControl9.clear();
        }

    //stop play10 
    player10->stop();
    player10->setListener(0);
    player10->disconnect();
    player10.clear();
    mPlayerListener10.clear();

    if ( NULL != playbackSurface10.get() ) {
    playbackSurface10.clear();
    }

    if ( NULL != playbackSurfaceControl10.get() ) {
    //playbackSurfaceControl10->clear();
    playbackSurfaceControl10.clear();
    }

    if ( NULL != client.get() ) {
    client->dispose();
        client.clear();
    }

    return 0;
}

int stop9dec_1enc_1dec() {

    VTC_LOGD("%d: %s", __LINE__, __FUNCTION__);
    stopRecording();
    stopPreview();
    player->stop();
    player->setListener(0);
    player->disconnect();
    player.clear();
    mPlayerListener.clear();

    if ( NULL != playbackSurface.get() ) {
        playbackSurface.clear();
    }

    if ( NULL != playbackSurfaceControl.get() ) {
        //playbackSurfaceControl->clear();
        playbackSurfaceControl.clear();
    }
//stop play2 
    player2->stop();
    player2->setListener(0);
    player2->disconnect();
    player2.clear();
    mPlayerListener2.clear();

    if ( NULL != playbackSurface2.get() ) {
        playbackSurface2.clear();
    }
	
    if ( NULL != playbackSurfaceControl2.get() ) {
        //playbackSurfaceControl2->clear();
        playbackSurfaceControl2.clear();
    }

//stop play3 
    player3->stop();
    player3->setListener(0);
    player3->disconnect();
    player3.clear();
    mPlayerListener3.clear();

    if ( NULL != playbackSurface3.get() ) {
        playbackSurface3.clear();
    }
	
    if ( NULL != playbackSurfaceControl3.get() ) {
        //playbackSurfaceControl3->clear();
        playbackSurfaceControl3.clear();
    }

//stop play4 
    player4->stop();
    player4->setListener(0);
    player4->disconnect();
    player4.clear();
    mPlayerListener4.clear();

    if ( NULL != playbackSurface4.get() ) {
        playbackSurface4.clear();
    }
	
    if ( NULL != playbackSurfaceControl4.get() ) {
        //playbackSurfaceControl4->clear();
        playbackSurfaceControl4.clear();
    }

//stop play5 
    player5->stop();
    player5->setListener(0);
    player5->disconnect();
    player5.clear();
    mPlayerListener5.clear();

    if ( NULL != playbackSurface5.get() ) {
        playbackSurface5.clear();
    }
	
    if ( NULL != playbackSurfaceControl5.get() ) {
        //playbackSurfaceControl5->clear();
        playbackSurfaceControl5.clear();
    }

//stop play6 
    player6->stop();
    player6->setListener(0);
    player6->disconnect();
    player6.clear();
    mPlayerListener6.clear();

    if ( NULL != playbackSurface6.get() ) {
        playbackSurface6.clear();
    }
	
    if ( NULL != playbackSurfaceControl6.get() ) {
        //playbackSurfaceControl6->clear();
        playbackSurfaceControl6.clear();
    }

//stop play7 
    player7->stop();
    player7->setListener(0);
    player7->disconnect();
    player7.clear();
    mPlayerListener7.clear();

    if ( NULL != playbackSurface7.get() ) {
        playbackSurface7.clear();
    }
	
    if ( NULL != playbackSurfaceControl7.get() ) {
        //playbackSurfaceControl7->clear();
        playbackSurfaceControl7.clear();
    }

//stop play8 
    player8->stop();
    player8->setListener(0);
    player8->disconnect();
    player8.clear();
    mPlayerListener8.clear();

    if ( NULL != playbackSurface8.get() ) {
        playbackSurface8.clear();
    }
	
    if ( NULL != playbackSurfaceControl8.get() ) {
        //playbackSurfaceControl8->clear();
        playbackSurfaceControl8.clear();
    }

//stop play9 
    player9->stop();
    player9->setListener(0);
    player9->disconnect();
    player9.clear();
    mPlayerListener9.clear();

    if ( NULL != playbackSurface9.get() ) {
        playbackSurface9.clear();
    }
	
    if ( NULL != playbackSurfaceControl9.get() ) {
        //playbackSurfaceControl9->clear();
        playbackSurfaceControl9.clear();
    }

    if ( NULL != client.get() ) {
        client->dispose();
        client.clear();
    }

    return 0;
}

int verfiyByPlayback() {
    VTC_LOGD(" ============= verfiyByPlayback ============= ");
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
    playbackSurfaceWidth = WIDTH;//client->getDisplayInfo(0);
    playbackSurfaceHeight = HEIGHT;//client->getDisplayHeight(0);
    VTC_LOGD("Panel WxH = %d x %d", playbackSurfaceWidth, playbackSurfaceHeight);

    startPlayback();
    pthread_mutex_lock(&mMutex);
    if (bPlaying){
        pthread_cond_wait(&mCond, &mMutex);
    }
    pthread_mutex_unlock(&mMutex);
    stopPlayback();
    return 0;
}

int test_CameraPreview() {
    startPreview();
    int64_t start = systemTime();
    sleep(mDuration);
    int64_t end = systemTime();
    stopPreview();

    return 0;
}

int test_RecordDEFAULT() {
    VTC_LOGI("\n\nRecorded Output is stored in %s\n\n", mRecordFileName);
    startPreview();
    int64_t start = systemTime();
    startRecording();
    sleep(mDuration);
    stopRecording();
    int64_t end = systemTime();
    stopPreview();
    //fprintf(stderr, "encoding %d frames in %d  us\n", nFrames, (end-start)/1000);
    //fprintf(stderr, "encoding speed is: %.2f fps\n", (nFrames * 1E9) / (end-start));

    return 0;
}

int test_SvcSpace() {
    VTC_LOGI("\n\nRecorded Output is stored in %s\n\n", mRecordFileName);
    startPreview();
    int64_t start = systemTime();
    startRecording();
    sleep(mDuration/2);
    stopRecording();
	startRecording_svcSpace();
    sleep(mDuration/2);
    stopRecording();
    int64_t end = systemTime();
	
    stopPreview();
    //fprintf(stderr, "encoding %d frames in %d  us\n", nFrames, (end-start)/1000);
    //fprintf(stderr, "encoding speed is: %.2f fps\n", (nFrames * 1E9) / (end-start));

    return 0;
}

int test_RecordPlayback() {
    VTC_LOGI("\n\nRecorded Output is stored in %s\n\n", mRecordFileName);
    startPreview();
    int64_t start = systemTime();
    startRecording();
    sleep(mDuration);
    stopRecording();
    int64_t end = systemTime();
    stopPreview();
    //fprintf(stderr, "encoding %d frames in %" PRId64 " us\n", nFrames, (end-start)/1000);
    //fprintf(stderr, "encoding speed is: %.2f fps\n", (nFrames * 1E9) / (end-start));
    test_PlaybackOnly();
    return 0;
}

int test_InsertIDRFrames() {
    status_t err = 0;
    mIFramesIntervalSec = 0;
    startPreview();
    startRecording();

    int duration = 0;
    while (duration < mDuration) {
        sleep(InsertIDRFrameEveryXSecs);
        duration += InsertIDRFrameEveryXSecs;

        sprintf(mParamValue,"video-param-insert-i-frame=1");
        String8 param(mParamValue);
        err = recorder->setParameters(param);
        if (err != OK) return -1;
        VTC_LOGI("\n Inserted an IDR Frame. \n");
    };

    stopRecording();
    stopPreview();

    return 0;
}


int test_MaxNALSize() {
    status_t err = 0;

    if (mIsSizeInBytes) {
        //Testing size base on bytes
        CHECK(mPreviewWidth > 320);
        CHECK(mSliceSizeBytes >= 256);
    } else {
        //Testing size base on MB
        CHECK(mSliceSizeMB > 6);
        CHECK(mSliceSizeMB < (((mPreviewWidth+15)>> 4) * ((mPreviewHeight+15)>> 4)));
        /* Max # of MB
            1080p=8160
            720p=3600
            VGA=1200
        */
    }

    /* Other limitations:
    4. Input content type should be progressive
    6. Changing parameters at run time will not have effect until next I-frame.
    7. Incase of doing the initial setting of nPFrames = 0 (only initial frame is I-frame and all others P-frames),
        you must request an I-frame to the codec after you have set nSlicesize to see your changes take place.
    */

    mIFramesIntervalSec = 0; //may be needed. dont know for sure. if it is not zero, then in OMXCodec, under support for B Frames, something is done.
    startPreview();
    startRecording();

    if (mIsSizeInBytes) {
        sprintf(mParamValue,"video-param-nalsize-bytes=%u", mSliceSizeBytes);
        String8 param(mParamValue);
        err = recorder->setParameters(param);
        if (err != OK) return -1;
        VTC_LOGI("\n Set the Slice Size in bytes.\n");
    } else {
        sprintf(mParamValue,"video-param-nalsize-macroblocks=%u", mSliceSizeMB);
        String8 param(mParamValue);
        err = recorder->setParameters(param);
        if (err != OK) return -1;
        VTC_LOGI("\n Set the Slice Size in MB\n");
    }

    // Change won't take effect until next IFrame. So, force an IFrame.

    sprintf(mParamValue,"video-param-insert-i-frame=1");
    String8 paramI(mParamValue);
    err = recorder->setParameters(paramI);
    if (err != OK) return -1;
    VTC_LOGI("\n Inserted an IDR Frame. \n");

    sleep(mDuration);
    stopRecording();
    stopPreview();
    mIFramesIntervalSec = 1;
    return 0;
}


int test_ChangeBitRate() {
    startPreview();
    startRecording();
    sleep(mDuration/2);

    sprintf(mParamValue,"video-config-encoding-bitrate=%u", mNewVideoBitRate);
    String8 param(mParamValue);
    status_t err = recorder->setParameters(param);
    if (err != OK) return -1;
    VTC_LOGI("\n\nSet new bitrate. \n\n");

    sleep(mDuration/2);
    stopRecording();
    stopPreview();
    return 0;
}


int test_ChangeFrameRate() {
    startPreview();
    startRecording();
    sleep(mDuration/2);

    // Changing the framerate in camera.
    params.unflatten(camera->getParameters());
    VTC_LOGD("Setting new framerate: %d", mNewVideoFrameRate);
    sprintf(mParamValue,"%u,%u", mNewVideoFrameRate*1000, mNewVideoFrameRate*1000);
    params.set("preview-fps-range", mParamValue);
    VTC_LOGD("get(preview-fps-range) = %s", params.get("preview-fps-range"));
    camera->setParameters(params.flatten());

    // Changing the framerate in encoder.
    sprintf(mParamValue,"video-config-encoding-framerate=%u", mNewVideoFrameRate);
    String8 param(mParamValue);
    status_t err = recorder->setParameters(param);
    if (err != OK) return -1;
    VTC_LOGI("\n\nSet new framerate. \n\n");

    sleep(mDuration/2);
    stopRecording();
    stopPreview();
    return 0;
}

int test_Playback3x3() {
    VTC_LOGI("\n\playback 3x3 in\n\n");

    startPlayback3x3();

    pthread_mutex_lock(&mMutex);
    if (bPlaying ){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);

    stopPlayback3x3();

    return 0;
}

int test_Playback_change_2x2layout() {
    VTC_LOGI("\n\playback change layout \n\n");

    startPlayback2x2();
	sleep(3);

    for(int i=0;i<10;i++){
	    startPlayback2x2_layout1();
		sleep(3);
	    startPlayback2x2_layout2();
		sleep(3);
    }
    stopPlayback2x2();

    return 0;
}

int test_Playback_change_3x3layout() {
    VTC_LOGI("\n\playback change layout \n\n");

	    startPlayback3x3();
		sleep(3);
	    for(int i=0;i<20;i++){
		    startPlayback3x3_layout1();
			sleep(3);
		    startPlayback3x3_layout2();
			sleep(3);
	    }
	    stopPlayback3x3();
    return 0;
}

int test_9dec_1enc_1dec() {
    VTC_LOGI("\n\playback 9dec + 1rec \n\n");
		cameraWinX=960;
		cameraWinY=540;
		cameraSurfaceWidth=640;
		cameraSurfaceHeight=360;
		mPreviewWidth=1920;
		mPreviewHeight=1080;
	

    startPlayback3x3_1dec();
    startPreview();
    startRecording();
	sleep(3);
    for(int i=0; i<10;i++){
        startPlayback3x3_layout1() ;
        sleep(3);
        startPlayback3x3_layout2();
        sleep(3);
    }
    startPlayback2x2_layout3();
    pthread_mutex_lock(&mMutex);
    if (bPlaying && bRecording && !mMediaPlayerThrewError){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);
    stopRecording();
    stopPreview();
    stopPlayback3x3_1dec();
    return 0;
}

int test_9dec_1rec() {
    VTC_LOGI("\n\playback 9dec + 1rec \n\n");
		cameraWinX=960;
		cameraWinY=540;
		cameraSurfaceWidth=640;
		cameraSurfaceHeight=360;
		mPreviewWidth=1920;
		mPreviewHeight=1080;
	

    startPlayback3x3();
    startPreview();
    startRecording();
	sleep(3);
    for(int i=0; i<10;i++){
        startPlayback3x3_layout1() ;
        sleep(3);
        startPlayback3x3_layout2();
        sleep(3);
    }
    startPlayback2x2_layout3();
    pthread_mutex_lock(&mMutex);
    if (bPlaying && bRecording && !mMediaPlayerThrewError){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);
    stopRecording();
    stopPreview();
    stopPlayback3x3();
    return 0;
}

int test_4dec_1rec() {
    VTC_LOGI("\n\playback 4dec+1rec \n\n");

    startPlayback4dec_1rec();
	sleep(3);
    for(int i=0; i<10*60*24*2;i++){
        startPlayback2x2_layout1() ;
        sleep(3);
        startPlayback2x2_layout2();
        sleep(3);
    }
    startPlayback2x2_layout3();
    pthread_mutex_lock(&mMutex);
    if (bPlaying && bRecording && !mMediaPlayerThrewError){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);
    stopPlayback4dec_1rec();
    return 0;
}

int test_2dec_1rec() {
    VTC_LOGI("\n\playback change layout \n\n");

    start2dec_1rec();
    pthread_mutex_lock(&mMutex);
    if (bPlaying && bRecording && !mMediaPlayerThrewError){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);
    stop2dec_1rec();
    return 0;
}

int test_PlaybackAndRecord_sidebyside() {
    VTC_LOGI("\n\nRecorded Output is stored in %s\n\n", mRecordFileName);
    if(client ==NULL){
        client = new SurfaceComposerClient();
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
    int panelwidth = 1920;//client->getDisplayWidth(0);
    int panelheight = 1080;//client->getDisplayHeight(0);
    VTC_LOGD("Panel WxH = %d x %d", panelwidth, panelheight);
    if (panelwidth < panelheight) {//Portrait Phone
        VTC_LOGD("\nPortrait Device\n");
        playbackSurfaceWidth = panelwidth;
        playbackSurfaceHeight = panelheight/2;
        playerWinX = 0;
        playerWinY = 0;

        cameraWinX = 0;
        cameraWinY = playbackSurfaceHeight;
        cameraSurfaceWidth = panelwidth;
        cameraSurfaceHeight = panelheight/2;
    } else if(0) {// Landscape
        VTC_LOGD("\n Landscape Device\n");
        playbackSurfaceWidth = panelwidth/2;
        playbackSurfaceHeight = panelheight/2;
        playerWinX = 0;
        playerWinY = 0;

        cameraWinX = playbackSurfaceWidth;
        cameraWinY = playbackSurfaceHeight;
        cameraSurfaceWidth = panelwidth/2;
        cameraSurfaceHeight = panelheight/2;
    }

    startPlayback2();
    sleep(SLEEP_AFTER_STARTING_PLAYBACK);
    startPreview();
    startRecording();

    pthread_mutex_lock(&mMutex);
    if (bPlaying && bRecording && !mMediaPlayerThrewError){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);

    stopRecording();
    stopPreview();
    stopPlayback2();

    cameraWinX = 0;
    cameraWinY = 0;
    playerWinX = 0;
    playerWinY = 0;
    return 0;
}


int test_PlaybackAndRecord_PIP() {
    VTC_LOGI("\n\nRecorded Output is stored in %s\n\n", mRecordFileName);
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
    uint32_t panelwidth = WIDTH;//client->getDisplayWidth(0);
    uint32_t panelheight = HEIGHT;//client->getDisplayHeight(0);
    VTC_LOGD("Panel WxH = %d x %d", panelwidth, panelheight);
    if (panelwidth < panelheight) {//Portrait Phone
        VTC_LOGD("\nPortrait Device\n");
        playbackSurfaceWidth = panelwidth;
        playbackSurfaceHeight = panelheight;
        playerWinX = 0;
        playerWinY = 0;

        cameraSurfaceWidth = panelwidth/2;
        cameraSurfaceHeight = panelheight/4;
        cameraWinX = (panelwidth - cameraSurfaceWidth) / 2;
        cameraWinY = 0;
    } else { // Landscape
        VTC_LOGD("\n Landscape Device\n");
        playbackSurfaceWidth = panelwidth;
        playbackSurfaceHeight = panelheight;
        playerWinX = 0;
        playerWinY = 0;

        cameraSurfaceWidth = panelwidth/3;
        cameraSurfaceHeight = panelheight/3;
        cameraWinX = ((panelwidth - cameraSurfaceWidth) / 2) + panelwidth/4;
        cameraWinY = 0;

    }

    startPlayback();
    sleep(3);
    startPreview();
    startRecording();

    while (bPlaying && bRecording && !mMediaPlayerThrewError) {
        int rc;
        pthread_mutex_lock(&mMutex);
        rc = my_pthread_cond_timedwait(&mCond, &mMutex, 100);
        pthread_mutex_unlock(&mMutex);

        if (rc != ETIMEDOUT){
            break; //exit while loop
        }
        /* Move preview */
        cameraWinY +=2;
        if ((cameraWinY+ cameraSurfaceHeight) > panelheight) cameraWinY = 0;
        client->openGlobalTransaction();
        surfaceControl->setPosition(cameraWinX, cameraWinY);
        client->closeGlobalTransaction();

        if (cameraWinY >  cameraSurfaceHeight/2){
            client->openGlobalTransaction();
            surfaceControl->hide();
            client->closeGlobalTransaction();
        } else {
            client->openGlobalTransaction();
            surfaceControl->show();
            client->closeGlobalTransaction();
        }
    };

    stopRecording();
    stopPreview();
    stopPlayback();

    cameraWinX = 0;
    cameraWinY = 0;
    playerWinX = 0;
    playerWinY = 0;
    return 0;
}

int test_PlaybackOnly()
{
    if(client ==NULL){
        client = new SurfaceComposerClient();
        //CHECK_EQ(client->initCheck(), (status_t)OK);
        if(client->initCheck() != (status_t)OK)
        VTC_LOGD(" initCheck error ");
    }
    int panelwidth = WIDTH;//client->getDisplayWidth(0);
    int panelheight = HEIGHT;//client->getDisplayHeight(0);
    VTC_LOGD("Panel WxH = %d x %d", panelwidth, panelheight);
    if (panelwidth < panelheight) {//Portrait Phone
        VTC_LOGD("\nPortrait Device\n");
        playbackSurfaceWidth = panelwidth;
        playbackSurfaceHeight = panelheight/2;
        playerWinX = 0;
        playerWinY = 0;

        cameraWinX = 0;
        cameraWinY = playbackSurfaceHeight;
        cameraSurfaceWidth = panelwidth;
        cameraSurfaceHeight = panelheight/2;
    } else {// Landscape
        VTC_LOGD("\n Landscape Device\n");
    }

    startPlayback();
    pthread_mutex_lock(&mMutex);
    if (bPlaying){
        my_pthread_cond_timedwait(&mCond, &mMutex, mPlaybackDuration);
    }
    pthread_mutex_unlock(&mMutex);
    stopPlayback();
    playerWinX = 0;
    playerWinY = 0;
    return 0;

}

void updatePassRate(int test_status, bool verifyRecordedClip) {

    mTestCount++;

    if (verifyRecordedClip){
        sleep(2);
        strcpy(mPlaybackFileName,  mRecordFileName);
        verfiyByPlayback();
    }

    // Wait for 10 seconds to make sure the memory settle.
    VTC_LOGD("%d: %s: Evaluating test results. Looking for memory leak. Waiting for memory to settle down ....", __LINE__, __FUNCTION__);
    sleep(10);

    int currentMediaServerPID;
    int endMemory;
    getMediaserverInfo(&currentMediaServerPID, &endMemory);
    int memDiff = endMemory - mStartMemory;
    if (memDiff < 0) {
        memDiff = 0;
    }
    VTC_LOGD("\n\n======================= Memory Leak [in bytes] = %d =======================\n\n", memDiff);sleep(1);

    int old_mFailCount = mFailCount;
    if (mMediaPlayerThrewError) mFailCount++;
    else if (mCameraThrewError) mFailCount++;
    else if (bPlaying == false) mFailCount++;
    else if (bRecording == false) mFailCount++;
    else if (test_status != 0) mFailCount++;
    else if (mMediaServerPID != currentMediaServerPID) mFailCount++; //implies mediaserver crashed. So, increment failure count.
    else if (memDiff > 10000) mFailCount++; //implies memory leak. So, increment failure count.

    VTC_LOGD("\n\nTest Results:\n\nNo. of Tests Executed = %d\nPASS = %d\nFAIL = %d\n\n", mTestCount, (mTestCount-mFailCount), (mFailCount*-1));

    mResultsFP = fopen("/sdcard/VTC_TEST_RESULTS.TXT", "a");
    if (mResultsFP != NULL) {
        if (old_mFailCount != mFailCount)
            fprintf(mResultsFP,  "%03d\tFAIL\t[%s]\n", mTestCount, mRecordFileName);
        else fprintf(mResultsFP,  "%03d\tPASS\t[%s]\n", mTestCount, mRecordFileName);
        fclose(mResultsFP);
    }

    mMediaServerPID = currentMediaServerPID; //Initialize for next test
    mStartMemory = endMemory; //I shouldn't be doing this right??
}

int test_Robust() {
    int status = 0;
    uint32_t cyclesCompleted = 0;
    getMediaserverInfo(&mMediaServerPID, &mStartMemory);

    if (mRobustnessTestType != -1){
        if ((mRobustnessTestType >= 1) || (mRobustnessTestType <= 8)) {
            for ( cyclesCompleted = 0; cyclesCompleted < mCycles; cyclesCompleted++){
                VTC_LOGD("\n\n\n############################ Iteration: %d. Goal: %d ############################\n\n\n", cyclesCompleted, mCycles);
                status = TestFunctions[mRobustnessTestType]();
                updatePassRate(status, false);
            }
        }
        return 0;
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_0039_Robustness_last_recorded.3gp");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);

    // Each cycle will play a selected number of different resolution scenarios
    // Starting from low to high resolution
    for ( cyclesCompleted = 0; cyclesCompleted < mCycles; cyclesCompleted++){
        VTC_LOGD("\n\n\n############################ Iteration: %d. Goal: %d ############################\n\n\n", cyclesCompleted, mCycles);
        mVideoBitRate = 3000000;
        mVideoFrameRate = 30;

        sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_000249_H264_VGA_1Mbps_eAACplus_48khz_64kbps.mp4");
        mPreviewWidth = 640;
        mPreviewHeight = 480;
        status = test_PlaybackAndRecord_sidebyside();
        updatePassRate(status, false);

        sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV-720p-JamesBond.MP4");
        mPreviewWidth = 1280;
        mPreviewHeight = 720;
        status = test_PlaybackAndRecord_sidebyside();
        updatePassRate(status, false);
    }

    return 0;
}


int test_ALL()
{
    // Automated Unit Test suite
    VTC_LOGD("\n\nExecuting %s \n\n", __FUNCTION__);
    int status = 0;
    char value[PROPERTY_VALUE_MAX];
    getMediaserverInfo(&mMediaServerPID, &mStartMemory);
    mDuration = 10;
    mResultsFP = fopen("/sdcard/VTC_TEST_RESULTS.TXT", "w+"); // To create a new file each time
    fclose(mResultsFP);

    property_get("disable.1080p.testing", value, "0");
    mDisable1080pTesting = 0;//atoi(value);
    if(mDisable1080pTesting){
        VTC_LOGD("\n\n\n\n########  1080p Testing as been disable  #######\n\n\n");
    }

    uint32_t resolution[3]={1/*1080 */, 2 /*720p */, 3/*480p */};

    uint32_t a[3]={2, 10, 0};
    uint32_t b[3]={3, 8, 0};
    uint32_t c[3]={1, 15, 0};
    int i=0, k=0;

    for(k=0;k++;k<3){
        uint32_t res=resolution[k];
        uint32_t *t;
        switch(res){
        case 1:// 1080p
            mPreviewWidth = 1920;
            mPreviewHeight = 1080;
            InsertIDRFrameEveryXSecs = 2;
            t=a;
            break;
        case 2:
            mPreviewWidth = 1280;
            mPreviewHeight = 720;
            InsertIDRFrameEveryXSecs = 5;
            t=b;
            break;
        case 3:
            mPreviewWidth = 640;
            mPreviewHeight = 480;
            InsertIDRFrameEveryXSecs = 8;
            t=c;
            break;
        default:
            break;
        }
        
        for(i=0;i++;i<3){
            sprintf(mRecordFileName,  "/data/output/utr_%03d_%dp_30fps_1mbps_i-frame-%dsec.mp4", mTestCount,mPreviewHeight, t[i]);
            VTC_LOGD("\n\n###################################################### recording. filename: %s\n\n", mRecordFileName);
            mIFramesIntervalSec = t[i];
            status = test_RecordDEFAULT();
            updatePassRate(status, true);
        }

        int f1[3]={30, 30, 15};
        int f2[3]={24, 15, 30};
        for(i=0;i<3;i++){
             sprintf(mRecordFileName,  "/data/output/utr_%03d_%dp_1mbps_%dfps-%dfps.mp4", mTestCount,mPreviewHeight,f1[i], f2[i]);
             VTC_LOGD("\n\n###################################################### recording. filename: %s\n\n", mRecordFileName);
             mVideoFrameRate = f1[i];
             mNewVideoFrameRate = f2[i];
             status = test_ChangeFrameRate();
             updatePassRate(status, true);
        }

        sprintf(mRecordFileName,  "/data/output/utr_%03d_%dp_30fps_1mbps_i-frames_every-%dsec.mp4", mTestCount,mPreviewHeight, InsertIDRFrameEveryXSecs);
        VTC_LOGD("\n\n###################################################### recording. filename: %s\n\n", mRecordFileName);
        status = test_InsertIDRFrames();
        updatePassRate(status, true);


    }

  if(!mDisable1080pTesting){
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_1080p_30fps_1Mbps_max-1000MB.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    mVideoFrameRate = 30;
    mIsSizeInBytes = false;
    mSliceSizeMB = 1000;
    status = test_MaxNALSize();
    updatePassRate(status, true);
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_30fps_1Mbps_max-300MB.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    mIsSizeInBytes = false;
    mSliceSizeMB = 300;
    status = test_MaxNALSize();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_VGAp_30fps_1Mbps_max-8MB.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 640;
    mPreviewHeight = 480;
    mIsSizeInBytes = false;
    mSliceSizeMB = 8;
    status = test_MaxNALSize();
    updatePassRate(status, true);

  if(!mDisable1080pTesting){
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_1080p_30fps_1Mbps_i-frames_every-2sec.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    InsertIDRFrameEveryXSecs = 2;
    status = test_InsertIDRFrames();
    updatePassRate(status, true);
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_30fps_1Mbps_i-frames_every-5sec.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    InsertIDRFrameEveryXSecs = 5;
    status = test_InsertIDRFrames();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_VGA_30fps_1Mbps_i-frames_every-8.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 640;
    mPreviewHeight = 480;
    InsertIDRFrameEveryXSecs = 8;
    mDuration = 20;
    status = test_InsertIDRFrames();
    updatePassRate(status, true);

    if(!mDisable1080pTesting){
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_1080p_30fps_1Mbps_max-1000bytes.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    mIsSizeInBytes = true;
    mSliceSizeBytes = 1000;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_MaxNALSize();
    updatePassRate(status, true);
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_30fps_1Mbps_max-500bytes.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    mIsSizeInBytes = true;
    mSliceSizeBytes = 500;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_MaxNALSize();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_VGA_30fps_1Mbps_max-256bytes.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 640;
    mPreviewHeight = 480;
    mIsSizeInBytes = true;
    mSliceSizeBytes = 256;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_MaxNALSize();
    updatePassRate(status, true);

    //PIP TC
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_D1PAL_30fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_000795_H264_D1PAL_25fps_4Mbps_NB_AMR_8Khz_12.2Kbps.mp4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 720;
    mPreviewHeight = 576;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_D1PAL_30fps_1Mbps_PIP.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_000795_H264_D1PAL_25fps_4Mbps_NB_AMR_8Khz_12.2Kbps.mp4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 720;
    mPreviewHeight = 576;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_PlaybackAndRecord_PIP();
    updatePassRate(status, true);

    //Simultaneous playback/record
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_VGA_30fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_000249_H264_VGA_1Mbps_eAACplus_48khz_64kbps.mp4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 640;
    mPreviewHeight = 480;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_24fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/FinalFantasy13_720p_mono_3.8Mbps_27fps.MP4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 24;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_30fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV-720p-JamesBond.MP4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);

    if(!mDisable1080pTesting){
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_1080p_24fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_001181_Toy_Story3Official_Trailer_in_FullHD1080p_h264_BP_L4.0_1920x1080_24fps_1Mbps_eAACplus_44100Hz.mp4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 24;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_1080p_30fps_1Mbps_simultaneous.3gp", mTestCount);
    sprintf(mPlaybackFileName,  "/mnt/ext_sdcard/vtc_playback/AV_000858_FinalFantasy13_1080p_h264_bp_30fps_8mbps_aac_lc.mp4");
    VTC_LOGD("\n\n################################## Recording. Filename: %s\n\n", mRecordFileName);
    VTC_LOGD("\n\n################################## Playing. Filename: %s\n\n", mPlaybackFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    mVideoBitRate = 1000000;
    mVideoFrameRate = 30;
    status = test_PlaybackAndRecord_sidebyside();
    updatePassRate(status, true);
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_QQVGA_15fps_bps_i-frames_every-8.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 160;
    mPreviewHeight = 120;
    mDuration = 10;
    mVideoBitRate = 262144;
    mVideoFrameRate = 15;
    status = test_RecordDEFAULT();
    updatePassRate(status, true);
    //Framerate and Bitrate change TC done, set default
    mVideoFrameRate = 30;
    mVideoBitRate = 1000000;

  if(!mDisable1080pTesting){
    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d1_1080p_30fps_from-5Mbps-to-100kbps.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1920;
    mPreviewHeight = 1080;
    mNewVideoBitRate = 100000;
    mVideoBitRate = 5000000;
    status = test_ChangeBitRate();
    updatePassRate(status, true);
    }

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_720p_30fps_from-5Mbps-to-100kbps.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 1280;
    mPreviewHeight = 720;
    mNewVideoBitRate = 100000;
    mVideoBitRate = 5000000;
    status = test_ChangeBitRate();
    updatePassRate(status, true);

    sprintf(mRecordFileName,  "/mnt/sdcard/vtc/UTR_%03d_VGA_30fps_from-5Mbps-to-100kbps.3gp", mTestCount);
    VTC_LOGD("\n\n###################################################### Recording. Filename: %s\n\n", mRecordFileName);
    mPreviewWidth = 640;
    mPreviewHeight = 480;
    mNewVideoBitRate = 50000;
    mVideoBitRate = 1000000;
    status = test_ChangeBitRate();
    updatePassRate(status, true);

    //Bit rate change TC done, set default
    mVideoBitRate = 1000000;

    /////////////////////////////     END   of  Unit Test     /////////////////////////////////////////

    VTC_LOGD("\n\nTest Results:\n\nNo. of Tests = %d\nPASS = %d\nFAIL = %d\n\n", mTestCount, (mTestCount-mFailCount), (mFailCount*-1));
    char results[256];
    mResultsFP = fopen("/sdcard/VTC_TEST_RESULTS.TXT", "r");
    if (mResultsFP != NULL) {
        while( fgets(results, sizeof(results), mResultsFP) != NULL ) VTC_LOGD("%s", results);
        fclose(mResultsFP);
    }
    return 0;
}


void printUsage() {
    printf("\n\nApplication for testing VTC requirements");
    printf("\nUsage: /system/bin/VTCTestApp test_case_id");
    printf("\n\n\nTest Case ID can be any of the following");
    printf("\n11 - Runs all the tests listed in the UTR. Pass/Fail must be determined by the tester after examining the recorded clips.");
    printf("\n1 - Default test case. Does not require any parameters to be set.");
    //printf("\n2 - Tests the ability to request I-Frame generation real time. Option: -I");
    printf("\n3 - Tests the ability to specify the maximum allowed frame size. Option: -s");
    printf("\n4 - Tests the ability to change the bitrate at runtime. Option: -B");
    printf("\n5 - Tests the ability to change the framerate at runtime. Option: -F");
    printf("\n6 - Test Playback and Record. Option: -p, -n");
    printf("\n7 - Test PIP. Option: -p, -n");
    printf("\n8 - Test Video playback Only. Option: -p");
    printf("\n9 - Robustness. Default: play and record the predefined resolutions (VGA & 720p). Option: -c, -v");
    printf("\n10 - record and playback the /sdcard/output.mp4 default ");
    printf("\n11 - test camera preview ");
    printf("\n12 - test 360P9dec+1080P1rec + 1080Pdec, .e.g ./vtcTest 15 -t 30000(ms); Note must have /data/city360.mp4 /data/jony360.mp4  /data/1080.mp4");
    printf("\n13 - test 360P4dec+1080P1rec case, .e.g ./vtcTest 13 -t 30000(ms); Note must have /data/city360.mp4 /data/jony360.mp4");
    printf("\n14 - test 1080P2dec+1080P1rec case, .e.g ./vtcTest 14 -t 30000(ms); Note must have /data/1080.mp4 /data/720.mp4 ");
    printf("\n15 - test 360P9dec+1080P1rec case, .e.g ./vtcTest 15 -t 30000(ms); Note must have /data/city360.mp4 /data/jony360.mp4");
    printf("\n0 - test 3x3 layout switch case , .e.g ./vtcTest 0 -t 30000(ms); Note must have /data/city360.mp4 /data/jony360.mp4.");
    printf("\n2 - test 2x2 layout switch case , e.g ./vtcTest 2 -t 30000(ms); Note must have /data/city360.mp4 /data/jony360.mp4");
    printf("\n11 - test camera preview ");

    printf("\n\n\nAvailable Options:");
	printf("\n-a video codec: [0] AVC [1] HEVC (default: 0)\n");
    printf("\n-n: Record Filename(/mnt/sdcard/vtc/video_0.3gp) is appended with this number. Default = %d", filename);
    printf("\n-w: Preview/Record Width. Default = %d", mPreviewWidth);
    printf("\n-h: Preview/Record Height. Default = %d", mPreviewHeight);
    printf("\n-d: Recording time in secs. Default = %d seconds", mDuration);
    printf("\n-i: I Frame Interval in secs. Default = %d", mIFramesIntervalSec);
    printf("\n-s: Slice Size bytes. Default = %d", mSliceSizeBytes);
    printf("\n-M: Slice Size Macroblocks. Default = %d", mSliceSizeMB);
    printf("\n-b: Bitrate. Default = %d", mVideoBitRate);
    printf("\n-f: Framerate. Default = %d", mVideoFrameRate);
    printf("\n-I: Insert I Frames. Specify the period in secs. Default = %d second", InsertIDRFrameEveryXSecs);
    printf("\n-B: Change bitrate at runtime to this new value. Default = %d", mNewVideoBitRate);
    printf("\n-F: Change framerate at runtime to this new value. Default = %d", mNewVideoFrameRate);
    printf("\n-p: Playback Filename. Default = %s", mPlaybackFileName);
    printf("\n-c: Robustness, number of cycles. Default = %d", mCycles);
    printf("\n-v: Robustness. Which test to repeat? Range: 1-8. Read test descriptions above. Default = -1 = play and record the predefined resolutions (VGA & 720p)");
    printf("\n-t: Playback Duration in milli secs. Default = %d = Play till EOF", mPlaybackDuration);

    printf("\n-playx: Playback pos x");
    printf("\n-playy: Playback pos y");
    printf("\n-playw: Playback width");
    printf("\n-playh: Playback height");
    printf("\n-prex: Preview pos x");
    printf("\n-prey: Preview pos y");
    printf("\n-prew: Preview width");
    printf("\n-preh: Preview height");
    printf("\n-encw: record width");
    printf("\n-ench: record height");
    printf("\n\n\n");

}

int main (int argc, char* argv[]) {
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();
    pthread_mutex_init(&mMutex, NULL);
    pthread_cond_init(&mCond, NULL);

    int opt;
    const char* const short_options = "a:n:w:l:h:d:s:i:I:b:f:B:F:p:q:M:c:t:v:";
    const struct option long_options[] = {
        {"record_filename", 1, NULL, 'n'},
        {"width", 1, NULL, 'w'},
        {"height", 1, NULL, 'h'},
        {"record_duration", 1, NULL, 'd'},
        {"slice_size_bytes", 1, NULL, 's'},
        {"i_frame_interval", 1, NULL, 'i'},
        {"insert_I_frames", 1, NULL, 'I'},
        {"bitrate", 1, NULL, 'b'},
        {"new_bitrate", 1, NULL, 'B'},
        {"framerate", 1, NULL, 'f'},
        {"new_framerate", 1, NULL, 'F'},
        {"playback_filename", 1, NULL, 'p'},
        {"playback_filename2", 1, NULL, 'q'},
        {"slice_size_MB", 1, NULL, 'M'},
        {"num_cycles", 1, NULL, 'c'},
        {"playback_duration", 1, NULL, 't'},
        {"robustness_test_type", 1, NULL, 'v'},
        {"playx", required_argument, NULL, 1 },
        {"playy", required_argument, NULL, 2 },
        {"playw", required_argument, NULL, 3 },
        {"playh", required_argument, NULL, 4 },
        {"prex", required_argument, NULL, 5 },
        {"prey", required_argument, NULL, 6 },
        {"prew", required_argument, NULL, 7 },
        {"preh", required_argument, NULL, 8 },
        {"encw", required_argument, NULL, 9 },
        {"ench", required_argument, NULL, 10 },
        {NULL, 0, NULL, 0}
    };

    sprintf(mPlaybackFileName,  "/sdcard/output.mp4");
    sprintf(mRecordFileName,  "/sdcard/output.mp4");

    if (argc < 2){
        printUsage();
        return 0;
    }
    testcase = atoi(argv[1]);

    while((opt = getopt_long_only(argc, argv, short_options, long_options, NULL)) != -1) {
        switch(opt) {
            case 1:
                playerWinX = atoi(optarg);
                break;
            case 2:
                playerWinY= atoi(optarg);
                break;
            case 3:
                playbackSurfaceWidth = atoi(optarg);
                break;
            case 4:
                playbackSurfaceHeight= atoi(optarg);
                 break;
            case 5:
                cameraWinX= atoi(optarg);
                break;
            case 6:
                cameraWinY= atoi(optarg);
                break;
            case 7:
                cameraSurfaceWidth= atoi(optarg);
                break;
            case 8:
                cameraSurfaceHeight= atoi(optarg);
                 break;
            case 9:
                mPreviewWidth= atoi(optarg);
                 break;
            case 10:
                mPreviewHeight= atoi(optarg);
                 break;
            case 'n':
                strcpy(mRecordFileName, optarg);
                break;
            case 'w':
                mPreviewWidth = atoi(optarg);
                break;
            case 'h':
                mPreviewHeight = atoi(optarg);
                break;
            case 'd':
                mDuration = atoi(optarg);
                break;
            case 's':
                mSliceSizeBytes = atoi(optarg);
                mIsSizeInBytes = true;
                break;
            case 'b':
                mVideoBitRate = atoi(optarg);
                break;
            case 'l':
                mVideoSvcLayer = atoi(optarg);
                break;
            case 'B':
                mNewVideoBitRate = atoi(optarg);
                break;
			
            case 'a':
            {
                mVideoCodec = atoi(optarg);
                if (mVideoCodec < 0 || mVideoCodec > 2) {
                    printf("error parameter\n");
                }
				if(mVideoCodec == 0)
					mVideoCodec = VIDEO_ENCODER_H264;
				else if(mVideoCodec == 1)
					mVideoCodec = VIDEO_ENCODER_H264;
					//mVideoCodec = VIDEO_ENCODER_H265;
                break;
            }
            case 'f':
                mVideoFrameRate = atoi(optarg);
                break;
            case 'F':
                mNewVideoFrameRate = atoi(optarg);
                break;
            case 'i':
                mIFramesIntervalSec = atoi(optarg);
                break;
            case 'I':
                InsertIDRFrameEveryXSecs = atoi(optarg);
                break;
            case 'p':
                strcpy(mPlaybackFileName, optarg);
                VTC_LOGD("Playback clip %s", mPlaybackFileName);
                break;
	case 'q':
                strcpy(mPlaybackFileName2, optarg);
                VTC_LOGD("Playback clip %s", mPlaybackFileName2);
                break;
            case 'M':
                mSliceSizeMB = atoi(optarg);
                mIsSizeInBytes = false;
                break;
            case 'c':
                mCycles = atoi(optarg);
                break;
            case 't':
                mPlaybackDuration = atoi(optarg);
                break;
            case 'v':
                mRobustnessTestType = atoi(optarg);
                break;
            case ':':
                VTC_LOGE("\nError - Option `%c' needs a value\n\n", optopt);
                return -1;
            case '?':
                VTC_LOGE("\nError - No such option: `%c'\n\n", optopt);
                return -1;
        }
    }

    system("echo VTCTestApp > /sys/power/wake_lock");
    TestFunctions[testcase]();
    system("echo VTCTestApp > /sys/power/wake_unlock");
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
    return 0;
}


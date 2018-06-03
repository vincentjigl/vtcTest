#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/time.h>
#include <utils/List.h>
#include <utils/Errors.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>

//#include <OMX_VideoExt.h>
//#include <OMX_IndexExt.h>
#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_Index.h"
#include "OMX_Types.h"
#include "Rockchip_OMX_Core.h"


using namespace android;
#define VA_FRAME_PICTURE        0x00000000 
#define VA_TOP_FIELD            0x00000001
#define VA_BOTTOM_FIELD         0x00000002
/* 
 * Pre-defined fourcc codes
 */
#define VA_FOURCC_NV12		0x3231564E
#define VA_FOURCC_NV21		0x3132564E
#define VA_FOURCC_AI44		0x34344149
#define VA_FOURCC_RGBA		0x41424752
#define VA_FOURCC_RGBX		0x58424752
#define VA_FOURCC_BGRA		0x41524742
#define VA_FOURCC_BGRX		0x58524742
#define VA_FOURCC_ARGB		0x42475241
#define VA_FOURCC_XRGB		0x42475258
#define VA_FOURCC_ABGR          0x52474241
#define VA_FOURCC_XBGR          0x52474258
#define VA_FOURCC_UYVY          0x59565955
#define VA_FOURCC_YUY2          0x32595559
#define VA_FOURCC_AYUV          0x56555941
#define VA_FOURCC_NV11          0x3131564e
#define VA_FOURCC_YV12          0x32315659
#define VA_FOURCC_P208          0x38303250
#define VA_FOURCC_IYUV          0x56555949
#define VA_FOURCC_YV24          0x34325659
#define VA_FOURCC_YV32          0x32335659
#define VA_FOURCC_Y800          0x30303859
#define VA_FOURCC_IMC3          0x33434D49
#define VA_FOURCC_411P          0x50313134
#define VA_FOURCC_422H          0x48323234
#define VA_FOURCC_422V          0x56323234
#define VA_FOURCC_444P          0x50343434
#define VA_FOURCC_RGBP          0x50424752
#define VA_FOURCC_BGRP          0x50524742
#define VA_FOURCC_411R          0x52313134 /* rotated 411P */

#include "loadsurface_yuv.h"

#if 1
#define OMXENC_PRINT(STR, ARG...) OMXENC_Log(STR, ##ARG)
#else
#define OMXENC_PRINT LOGE
#define LOG_TAG "OMXENCTEST_jgl"
#endif

#define OMXENC_CHECK_ERROR(_e_, _s_)                \
  if (_e_ != OMX_ErrorNone){                        \
    printf("\n%s::%d ERROR %x : %s \n", __FUNCTION__, __LINE__,  _e_, _s_); \
    OMXENC_HandleError(pAppData, _e_);              \
    goto EXIT;                                      \
  }                                                 \

#define OMXENC_CHECK_EXIT(_e_, _s_)                 \
  if (_e_ != OMX_ErrorNone){                        \
    printf("\n%s ::%d ERROR %x : %s \n", __FUNCTION__, __LINE__, _e_, _s_); \
    goto EXIT;                                      \
  }

#define NUM_OF_IN_BUFFERS 10
#define NUM_OF_OUT_BUFFERS 10
#define MAX_NUM_OF_PORTS 16
#define MAX_EVENTS 256

enum State {
    UNLOADED,
    LOADED,
    LOADED_TO_IDLE,
    IDLE_TO_EXECUTING,
    EXECUTING,
    EXECUTING_TO_IDLE,
    IDLE_TO_LOADED,
    RECONFIGURING,
    ERROR
};

struct omx_message {
    enum {
        EVENT,
        EMPTY_BUFFER_DONE,
        FILL_BUFFER_DONE,

    } type;

    void* node;

    union {
        // if type == EVENT
        struct {
            OMX_EVENTTYPE event;
            OMX_U32 data1;
            OMX_U32 data2;
        } event_data;

        // if type == EMPTY/FILL_BUFFER_DONE
        struct {
            void* buffer;
        } buffer_data;
    } u;
};


typedef enum OMXENC_PORT_INDEX {
    VIDENC_INPUT_PORT = 0x0,
    VIDENC_OUTPUT_PORT
} OMXENC_PORT_INDEX;

typedef enum OMXENC_STATE {
    OMXENC_StateLoaded = 0x0,
    OMXENC_StateUnLoad,
    OMXENC_StateReady,
    OMXENC_StateStarting,
    OMXENC_StateEncoding,
    OMXENC_StateWaitEvent,
    OMXENC_StatePause,
    OMXENC_StateStop,
    OMXENC_StateError
} OMXENC_STATE;

typedef enum VIDENC_TEST_NAL_FORMAT {
    VIDENC_TEST_NAL_UNIT = 0,
    VIDENC_TEST_NAL_SLICE,
    VIDENC_TEST_NAL_FRAME
} VIDENC_TEST_NAL_FORMAT;

typedef struct MYDATATYPE {
    OMX_HANDLETYPE pHandle;
    char szInFile[128];
    char szOutFile[128];
    char *szOutFileNal;
    int nWidth;
    int nHeight;
    int nFlags;
    OMX_COLOR_FORMATTYPE eColorFormat;
    OMX_U32 nBitrate;
    OMX_U32 nFramerate;
    OMX_VIDEO_CODINGTYPE eCompressionFormat;
    //OMX_NALUFORMATSTYPE eNaluFormat;
    char szCompressionFormat[128];
    OMX_U32 eLevel;
    char eProfile[32];
    OMX_STATETYPE eState;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef[MAX_NUM_OF_PORTS];
    OMX_PARAM_PORTDEFINITIONTYPE* pInPortDef;
    OMX_PARAM_PORTDEFINITIONTYPE* pOutPortDef;
    FILE* fIn;
    FILE* fOut;

    OMX_U32 nCurrentFrameIn;
    OMX_U32 nCurrentFrameOut;
    OMX_CALLBACKTYPE* pCb;
    OMX_COMPONENTTYPE* pComponent;
    OMX_BUFFERHEADERTYPE* pInBuff[NUM_OF_IN_BUFFERS];
    OMX_BUFFERHEADERTYPE* pOutBuff[NUM_OF_OUT_BUFFERS];

    OMX_BOOL bSyncMode;
    OMX_BOOL bShowPic;
    OMX_VIDEO_CONTROLRATETYPE eControlRate;
    char szControlRate[128];
    OMX_U32 nQpI;
    OMX_BOOL bShowBitrateRealTime;
    OMX_BOOL bMetadataMode;
    OMX_BOOL bDeblockFilter;
    OMX_U32 nTargetBitRate;
    OMX_U32 nGOBHeaderInterval;
    OMX_U32 nPorts;
    void* pEventArray[MAX_EVENTS];

    State eCurrentState;
    OMX_U32  nQPIoF;
    unsigned int nWriteLen;
    unsigned int nBitrateSize;
    OMX_U32 nSavedFrameNum;
    //
    OMX_U32 nISliceNum;
    OMX_U32 nPSliceNum;
    OMX_U32 nSliceSize;
    //
    OMX_U32 nIDRInterval;
    int nPFrames;
    //OMX_U32 nPFrames;
    OMX_U32 nBFrames;
    OMX_VIDEO_CONFIG_NALSIZE *pNalSize;
    OMX_U32 nFramecount;

    //AIR
    OMX_BOOL bAirEnable;
    OMX_BOOL bAirAuto;
    float nAirMBs;
    OMX_U32 nAirThreshold;

    //Intre refresh
    OMX_VIDEO_INTRAREFRESHTYPE eRefreshMode;
    OMX_U32 nIrMBs;
    OMX_U32 nIrRef;
    OMX_U32 nCirMBs;

    //OMX_VIDEO_CONFIG_INTEL_BITRATETYPE
    OMX_U32 nInitQP;
    OMX_U32 nMinQP;
    OMX_U32 nMaxQP;
    OMX_U32 nMaxEncodeBitrate;
    OMX_U32 nTargetPercentage;
    OMX_U32 nWindowSize;
    OMX_BOOL bFrameSkip;
    OMX_U32 nDisableBitsStuffing;
    //
    //
    OMX_U32 nByteRate;
    OMX_U32 nBufToRead;
    OMX_BOOL bStart;
    OMX_BOOL bRCAlter;
    OMX_BOOL bSNAlter;
    OMX_BOOL bVUIEnable;
    //statistics
    int nEncBegin, nEncEnd;
    int nUpload, nDownload, nTc1, nTc2;

    //for vp8 encoder
    FILE *pfVP8File;
    unsigned int vp8_coded_frame_size;
    unsigned int pts;
    unsigned int number_of_rewind;
    unsigned int only_render_k_frame_one_time;
    unsigned int max_frame_size_ratio;
    unsigned int number_of_layer;
    OMX_U32 nBitrateForLayer0;
    OMX_U32 nBitrateForLayer1;
    OMX_U32 nBitrateForLayer2;
    OMX_U32 nFramerateForLayer0;
    OMX_U32 nFramerateForLayer1;
    OMX_U32 nFramerateForLayer2;
    OMX_U32 nSendFrameCount;

} MYDATATYPE;

typedef struct EVENT_PRIVATE {
    OMX_EVENTTYPE eEvent;
    OMX_U32 nData1;
    OMX_U32 nData2;
    MYDATATYPE* pAppData;
    OMX_PTR pEventData;
} EVENT_PRIVATE;



template<class T>
static void InitOMXParams(T *params)
{
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

OMX_ERRORTYPE OMXENC_HandleError(MYDATATYPE* pAppData, OMX_ERRORTYPE eError);

void OMXENC_FreeResources(MYDATATYPE* pAppData);

OMX_ERRORTYPE OMXENC_DynamicChangeVPX(MYDATATYPE* pAppData);
OMX_ERRORTYPE OMXENC_SetVPXParameter(MYDATATYPE* pAppData);
OMX_ERRORTYPE OMXENC_SetConfigVPX(MYDATATYPE* pAppData);
void OMXENC_WriteIvfFileHeader(MYDATATYPE *pAppData);
void OMXENC_WriteIvfFileFrame(MYDATATYPE *pAppData, OMX_BUFFERHEADERTYPE* pBuffer);


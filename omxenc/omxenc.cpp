#include "omxenc.h"

static OMX_STRING StrVideoEncoder;
static OMX_STRING StrVideoEncoderAVC = "OMX.rk.video_encoder.avc";

static Mutex mLock;
static Condition mAsyncCompletion;

bool mNoOutputFlag = OMX_FALSE;
bool mRunning = OMX_FALSE;
pthread_t mThread;
static Mutex mMsgLock;
static Condition mMessageReceive;
android::List<omx_message> msgQueue;

static int index_metadata = 0;
static OMX_U32 nInputBufNum = 4; // use for substitue  NUM_OF_IN_BUFFERS NUM_OF_OUT_BUFFERS
static uint8_t *gUsrptr[NUM_OF_IN_BUFFERS];
int yyparse(void);

OMX_BOOL bPrintLog = OMX_TRUE;
void OMXENC_Log(const char *strFormat, ...)
{
    if (bPrintLog == OMX_FALSE)
        ;
    else {
        va_list list;
        va_start(list, strFormat);
        vfprintf(stdout, strFormat, list);
        va_end(list);
    }
}

int GetTickCount()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}


static int row_shift = 0;
int YUV_Generator_Planar(int width, int height,
                         unsigned char *Y_start, int Y_pitch,
                         unsigned char *U_start, int U_pitch,
                         unsigned char *V_start, int V_pitch,
                         int UV_interleave
                        )
{
    int box_width = 8;

    row_shift++;
    if (row_shift == 16) row_shift = 0;

    V_start = U_start + 1;
    yuvgen_planar(width,  height,
                  Y_start,  Y_pitch,
                  U_start,  U_pitch,
                  V_start,  V_pitch,
                  VA_FOURCC_NV12,  box_width,  row_shift,
                  0);

    return 0;
}

OMX_ERRORTYPE OMXENC_FreeBuffer(MYDATATYPE* pAppData)
{
    OMXENC_PRINT("%s begin\n", __FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;
    OMX_U32 nCounter;

    //eError = OMX_SendCommand(pHandle, OMX_CommandPortDisable, VIDENC_OUTPUT_PORT, NULL);
    //OMXENC_CHECK_EXIT(eError, "Error at OMX_SendCommand function");
    //eError = OMX_SendCommand(pHandle, OMX_CommandPortDisable, VIDENC_INPUT_PORT, NULL);
    //OMXENC_CHECK_EXIT(eError, "Error from OMX_SendCommand function\n");

    for (nCounter = 0; nCounter < pAppData->pOutPortDef->nBufferCountActual; nCounter++)
        eError = OMX_FreeBuffer(pHandle,
                                pAppData->pOutPortDef->nPortIndex, pAppData->pOutBuff[nCounter]);
    OMXENC_CHECK_EXIT(eError, "Error at OMX_FreeBuffer function");



    for (nCounter = 0; nCounter < pAppData->pInPortDef->nBufferCountActual; nCounter++)
        eError = OMX_FreeBuffer(pHandle,
                                pAppData->pInPortDef->nPortIndex, pAppData->pInBuff[nCounter]);
    OMXENC_CHECK_EXIT(eError, "Error at OMX_FreeBuffer function");

EXIT:
    OMXENC_PRINT("%s end\n",  __FUNCTION__);
    return eError;
}

OMX_ERRORTYPE OMXENC_DeInit(MYDATATYPE* pAppData)
{
    OMXENC_PRINT("%s begin \n",  __FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int eErr = 0;
    OMX_U32 nCounter;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;


    if (pAppData->bShowPic)
        unsetenv("AUTO_UV");

    /* Free Component Handle */
    if (pHandle)
        eError = OMX_FreeHandle(pHandle);
    OMXENC_CHECK_EXIT(eError, "Error in OMX_FreeHandle   ===");

    /* De-Initialize OMX Core */
    eError = OMX_Deinit();
    OMXENC_CHECK_EXIT(eError, "Error in OMX_Deinit  ===");

    mRunning = OMX_FALSE;
 
    /* shutdown */
    if (pAppData->fIn)
        fclose(pAppData->fIn);
    fclose(pAppData->fOut);

    if (pAppData->pfVP8File != NULL) {
        fclose(pAppData->pfVP8File);
        pAppData->pfVP8File = NULL;
    }

    pAppData->fIn = NULL;
    pAppData->fOut = NULL;
    free(pAppData->pCb);
    free(pAppData->pInPortDef);
    free(pAppData->pOutPortDef);

    free(pAppData);
    pAppData = NULL;

EXIT:
    OMXENC_PRINT("%s end\n",  __FUNCTION__);
    return eError;
}

OMX_ERRORTYPE OMXENC_HandleError(MYDATATYPE* pAppData, OMX_ERRORTYPE eError)
{
    OMXENC_PRINT(">> %s\n", __FUNCTION__);
    OMX_ERRORTYPE eErrorHandleError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;
    OMX_U32 nCounter;
    int eErr = 0;


    switch (pAppData->eCurrentState) {
    case LOADED_TO_IDLE:
    case IDLE_TO_EXECUTING:
    case EXECUTING:
    case EXECUTING_TO_IDLE:
        OMXENC_FreeBuffer(pAppData);
    case LOADED:
        eError = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        OMXENC_CHECK_EXIT(eError, "Error at OMX_SendCommand function");
        OMXENC_DeInit(pAppData);
    case UNLOADED:
        eError = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        OMXENC_CHECK_EXIT(eError, "Error at OMX_SendCommand function");
        OMXENC_PRINT("Free Resources\n");
        OMXENC_DeInit(pAppData);
    default:
        ;
    }

EXIT:
    OMXENC_PRINT(">> %s\n", __FUNCTION__);
    return eErrorHandleError;
}


int OMXENC_fill_data(OMX_BUFFERHEADERTYPE *pBuf, FILE *fIn, int bufferSize)
{
    int nRead = 0;
    int nError = 0;
    int frame_id;
    int pitch_y = 0;
    int pitch_u = 0;
    int row;
    void *UV_start;
    MYDATATYPE *pApp = (MYDATATYPE*)pBuf->pAppPrivate;

    if (pApp->fIn) {
        nRead = fread(pBuf->pBuffer, 1, bufferSize, fIn);
        if (nRead == -1) {
            OMXENC_PRINT("Error Reading File!\n");
        }
        nError = ferror(fIn);
        if (nError != 0) {
            OMXENC_PRINT("ERROR: reading file\n");
        }
        nError = feof(fIn);
        if (nError != 0) {
            OMXENC_PRINT("EOS of source file rewind it");
            clearerr(fIn);
            rewind(fIn);
            nRead = fread(pBuf->pBuffer, 1, bufferSize, fIn);
            pApp->number_of_rewind++;
        }
    } else {
        nRead = pApp->nWidth * pApp->nHeight * 3 / 2;
        YUV_Generator_Planar(pApp->nWidth, pApp->nHeight,
                             pBuf->pBuffer, pApp->nWidth,
                             pBuf->pBuffer + pApp->nWidth * pApp->nHeight,  pApp->nWidth,
                             pBuf->pBuffer + pApp->nWidth * pApp->nHeight,  pApp->nWidth,
                             1);
    }

    pBuf->nFilledLen = nRead;
    OMXENC_PRINT("%s,  buffer 0x%x, buffersize %d , length %d \n",
                 __FUNCTION__,
                 *((int*)pBuf->pBuffer),
                 bufferSize, pBuf->nFilledLen);

    return nRead;
}

OMX_ERRORTYPE OMXENC_Stop(MYDATATYPE* pAppData)
{
    OMXENC_PRINT("%s begin\n", __FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;


    eError = OMX_SendCommand(pAppData->pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMXENC_CHECK_EXIT(eError, "Error at OMX_SendCommand function");

    OMXENC_PRINT("current state %d (%4d)\n", pAppData->eCurrentState, __LINE__);
    {
        Mutex::Autolock autoLock(mLock);
        while (pAppData->eCurrentState != LOADED)
            mAsyncCompletion.wait(mLock);
    }

EXIT:
    OMXENC_PRINT("%s end \n", __FUNCTION__);
    return eError;
}


OMX_ERRORTYPE OMXENC_SetConfig(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    if (pAppData->eCompressionFormat == OMX_VIDEO_CodingAVC) {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD intraperiodConfig;
        InitOMXParams(&intraperiodConfig);
        intraperiodConfig.nPortIndex = OMX_DirOutput;
        intraperiodConfig.nIDRPeriod = pAppData->nIDRInterval;
        intraperiodConfig.nPFrames = pAppData->nPFrames;
        eError = OMX_SetConfig(pAppData->pHandle, OMX_IndexConfigVideoAVCIntraPeriod, &intraperiodConfig);
        OMXENC_CHECK_ERROR((OMX_ERRORTYPE)eError, "Error at setConfig force IDR function");
    }

EXIT:
    return eError;
}

OMX_ERRORTYPE OMXENC_Read(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    pAppData->pComponent = (OMX_COMPONENTTYPE*)pHandle;


    /*Initialize Frame Counter */
    pAppData->nCurrentFrameIn = 0;
    pAppData->nCurrentFrameOut = 0;
    pAppData->nByteRate = 0;
    pAppData->bStart = OMX_FALSE;
    pAppData->nWriteLen = 0;

    /* Send FillThisBuffer to OMX Video Encoder */
    for (uint32_t nCounter = 0; nCounter < pAppData->pOutPortDef->nBufferCountActual; nCounter++) {
        OMXENC_PRINT("FillThisBuffer output buffer %p \n", pAppData->pOutBuff[nCounter]);
        eError = OMX_FillThisBuffer(pAppData->pComponent, pAppData->pOutBuff[nCounter]);
        OMXENC_CHECK_EXIT(eError, "Error in FillThisBuffer");
    }


    /* Send EmptyThisBuffer to OMX Video Encoder */
    for (uint32_t nCounter = 0; nCounter < pAppData->pInPortDef->nBufferCountActual; nCounter++) {
        pAppData->nTc1 = GetTickCount();
        pAppData->pInBuff[nCounter]->nFilledLen =
            OMXENC_fill_data(pAppData->pInBuff[nCounter],
                    pAppData->fIn,
                    pAppData->pInPortDef->nBufferSize);
        pAppData->nUpload += (GetTickCount() - pAppData->nTc1);

        if (pAppData->nFramecount < 5) {
            if (pAppData->nCurrentFrameIn == pAppData->nFramecount - 1) {
                OMXENC_PRINT("total frame count %d\n", pAppData->nFramecount);
                pAppData->pInBuff[nCounter]->nFlags |= OMX_BUFFERFLAG_EOS;
            }
        }

        OMXENC_PRINT("EmptyThisBuffer input buffer %p \n", pAppData->pInBuff[nCounter]);
        eError = OMX_EmptyThisBuffer(pAppData->pComponent, pAppData->pInBuff[nCounter]);
        OMXENC_CHECK_ERROR(eError, "Error at EmptyThisBuffer function");
        pAppData->nCurrentFrameIn++;
    }

    {
        Mutex::Autolock autoLock(mLock);
        while(pAppData->nFlags != OMX_BUFFERFLAG_EOS)
            mAsyncCompletion.wait(mLock);
    }
    OMXENC_PRINT("Receive EOS notify \n");
    eError = OMX_SendCommand(pAppData->pHandle, OMX_CommandFlush, OMX_ALL, NULL);
    OMXENC_CHECK_ERROR(eError, "Error at OMX_CommandFlush ");
    {
        Mutex::Autolock autoLock(mLock);
        while (pAppData->eCurrentState != EXECUTING_TO_IDLE)
            mAsyncCompletion.wait(mLock);
    }

EXIT:
    return eError;
}

void setState(State* curState, State newState)
{
    Mutex::Autolock autoLock(mLock);
    *curState = newState;
    mAsyncCompletion.signal();
}

OMX_ERRORTYPE OMXENC_CommandHandle(MYDATATYPE* pAppData, OMX_U32 cmd)
{
    OMXENC_PRINT("OnCommand: cmd %d, cur state %d \n", cmd, pAppData->eCurrentState);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;
    OMX_U32 nCounter = 0;

    switch (cmd) {
    case OMX_StateIdle:
        if (pAppData->eCurrentState == LOADED_TO_IDLE) {

            eError = OMX_SendCommand(pAppData->pHandle,
                                     OMX_CommandStateSet, OMX_StateExecuting, NULL);
            OMXENC_CHECK_EXIT(eError, "Error OMX_SendCommand EXECUTING\n");
            setState(&pAppData->eCurrentState, IDLE_TO_EXECUTING);


        } else if (pAppData->eCurrentState == EXECUTING_TO_IDLE) {

            OMXENC_FreeBuffer(pAppData);

            eError = OMX_SendCommand(pAppData->pHandle,
                                     OMX_CommandStateSet, OMX_StateLoaded, NULL);
            OMXENC_CHECK_EXIT(eError, "Error OMX_SendCommand Loaded \n");
            setState(&pAppData->eCurrentState, IDLE_TO_LOADED);

        } else {
            OMXENC_PRINT("%4d error state.", __LINE__);
            return OMX_ErrorInvalidState;
        }
        break;
    case OMX_StateExecuting:
        if (pAppData->eCurrentState == IDLE_TO_EXECUTING)
            setState(&pAppData->eCurrentState, EXECUTING);
        else {
            OMXENC_PRINT("%4d error state.", __LINE__);
            return OMX_ErrorInvalidState;
        }
        break;
    case OMX_StateLoaded:
        if (pAppData->eCurrentState == IDLE_TO_LOADED)
            setState(&pAppData->eCurrentState, LOADED);
        else {
            OMXENC_PRINT("%4d error state.", __LINE__);
            return OMX_ErrorInvalidState;
        }
        break;
    case OMX_StateInvalid:
        OMXENC_PRINT("Component in OMX_StateInvalid\n");
        pAppData->eCurrentState = ERROR;
        break;
    default:
        OMXENC_PRINT("should not be here.");
        break;
    }

EXIT:
    return eError;
}

//message thread
OMX_ERRORTYPE OMXENC_EventHandler(OMX_PTR pData, 
        OMX_EVENTTYPE eEvent, 
        OMX_U32 nData1, 
        OMX_U32 nData2)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    MYDATATYPE *pAppData = (MYDATATYPE*) pData;

    OMXENC_PRINT("EventHandler: event %d nData1 %d nData2 : 0x%x\n", eEvent, nData1, nData2);

    switch (eEvent) {
    case OMX_EventCmdComplete:
        switch (nData1) {
        case OMX_CommandStateSet:
            eError = OMXENC_CommandHandle(pAppData, nData2);
            OMXENC_CHECK_ERROR(eError, "OMXENC_CommandHandle");
            break;
        case OMX_CommandFlush:
            OMXENC_PRINT("OMX_CommandFlush port %ld  \n", nData2);
            if (pAppData->eCurrentState == EXECUTING && nData2 == 1) {
                setState(&pAppData->eCurrentState, EXECUTING_TO_IDLE);
            }

            break;
        case OMX_CommandPortDisable:
            OMXENC_PRINT("%s:%d  OMX_CommandPortDisable event handle \n", __FUNCTION__, __LINE__);
            break;
        case OMX_CommandMarkBuffer:
            ;
        }
        break;
    case OMX_EventError: {
        printf("\n%s::%d, nData2 : %ld\n", __FUNCTION__, __LINE__, nData2);

        OMXENC_Stop(pAppData);
        eError = OMX_SendCommand(pAppData->pHandle,
                                 OMX_CommandStateSet, OMX_StateLoaded, NULL);
        OMXENC_CHECK_EXIT(eError, "Error at OMX_SendCommand function");

        eError =  OMXENC_DeInit(pAppData);
        OMXENC_CHECK_EXIT(eError, "Error at OMX_Deinit function");
        exit(0);
        break;
    }
    case OMX_EventBufferFlag:
        OMXENC_PRINT("Detect EOS at EmptyThisBuffer function \n");
        mNoOutputFlag = OMX_TRUE;
        pAppData->nFlags = OMX_BUFFERFLAG_EOS;
        mAsyncCompletion.signal();
        break;
    default:
        OMXENC_CHECK_ERROR(OMX_ErrorUndefined, "Error at EmptyThisBuffer function");
        break;
    }

EXIT:
    return eError;
}

//message thread
OMX_ERRORTYPE OMXENC_FillBufferDone(OMX_PTR pData, 
        OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    MYDATATYPE* pAppData = (MYDATATYPE*)pData;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    unsigned int vp8_frame_length = 0;

    if (mNoOutputFlag == OMX_TRUE)
       return OMX_ErrorNone;

    OMXENC_PRINT("FILLBUFFERDONE %p size %d value %x, nCurrentFrameIn %d, nCurrentFrameOut %d (%4d)\n",
                 pBuffer,
                 pBuffer->nFilledLen,
                 *(int*)pBuffer->pBuffer,
                 pAppData->nCurrentFrameIn,
                 pAppData->nCurrentFrameOut,
                 __LINE__);


    if (pAppData->eCurrentState != EXECUTING) {
        OMXENC_PRINT("%4d error state.", __LINE__);
        goto EXIT;
    }

    /* check is it is the last buffer */
    if (pBuffer->nFlags & OMX_BUFFERFLAG_EOS)
        OMXENC_PRINT("Last OutBuffer\n");

    pAppData->nCurrentFrameOut++;

    /*App sends last buffer as null buffer, so buffer with EOS contains only garbage*/
    if (pBuffer->nFilledLen) {
            pAppData->nWriteLen += fwrite(pBuffer->pBuffer + pBuffer->nOffset,
                                          1, pBuffer->nFilledLen, pAppData->fOut);

        pAppData->nBitrateSize += pBuffer->nFilledLen;
        pAppData->nSavedFrameNum++;
        if (ferror(pAppData->fOut))
            printf("ERROR: writing to file=\n");

        if (fflush(pAppData->fOut))
            printf("ERROR: flushing file -----\n");

        if (pAppData->nSavedFrameNum % pAppData->nFramerate== 0) {
            printf(".");
            fflush(stdout);
        }


        if (pAppData->bShowBitrateRealTime) {
            if ((pAppData->nSavedFrameNum) % pAppData->nFramerate == 0) {
                printf("PERFORMANCE:       frame(%d - %d), bitrate : %d kbps\n", 
                        pAppData->nSavedFrameNum - pAppData->nFramerate, 
                        pAppData->nSavedFrameNum,
                        pAppData->nBitrateSize * 8 / 1000);
                pAppData->nBitrateSize = 0;
            }
        }
    }


    pBuffer->nFilledLen = 0;

    eError = OMX_FillThisBuffer(pAppData->pComponent, pBuffer);
    OMXENC_CHECK_ERROR(eError, "Error at FillThisBuffer function");

EXIT:
    return eError;
}

//message thread
OMX_ERRORTYPE OMXENC_EmptyBufferDone(OMX_PTR pData, 
        OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    MYDATATYPE* pAppData = (MYDATATYPE*)pData;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    if (mNoOutputFlag == OMX_TRUE)
       return OMX_ErrorNone;

    OMXENC_PRINT("EMPTYBUFFERDONE %p size %d value %x, nCurrentFrameIn %d, nCurrentFrameOut %d (%4d) \n",
                 pBuffer, pBuffer->nFilledLen,
                 *(int*)pBuffer->pBuffer,
                 pAppData->nCurrentFrameIn,
                 pAppData->nCurrentFrameOut,
                 __LINE__);

    if (pAppData->eCurrentState != EXECUTING) {
        OMXENC_PRINT("error state (%4d)", __LINE__);
        goto EXIT;
    }


    if (pAppData->nCurrentFrameIn == pAppData->nFramecount) {
        OMXENC_PRINT("total frame count %d\n", pAppData->nFramecount);
        pBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
    }


    if (pAppData->fIn) {
        pBuffer->nFilledLen = OMXENC_fill_data(pBuffer, pAppData->fIn, pAppData->pInPortDef->nBufferSize);
    }

    eError = OMX_EmptyThisBuffer(pAppData->pComponent, pBuffer);
    OMXENC_CHECK_ERROR(eError, "Error at EmptyThisBuffer function");

    pAppData->nCurrentFrameIn++;

EXIT:
    return eError;
}

void setPortDefinition(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (input) */
    /* OMX_VIDEO_PORTDEFINITION values for input port */
    InitOMXParams(pAppData->pInPortDef);
    pAppData->pInPortDef->nPortIndex = OMX_DirInput;
    eError = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, pAppData->pInPortDef);
    OMXENC_CHECK_EXIT(eError, "Error at GetParameter(IndexParamPortDefinition I)");
    nInputBufNum = pAppData->pInPortDef->nBufferCountActual;
    pAppData->pInPortDef->nBufferSize = pAppData->nWidth * pAppData->nHeight * 3 / 2;
    pAppData->pInPortDef->format.video.eColorFormat = pAppData->eColorFormat;
    pAppData->pInPortDef->format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    pAppData->pInPortDef->format.video.nStride = pAppData->nWidth;
    pAppData->pInPortDef->format.video.nSliceHeight = pAppData->nHeight;
    pAppData->pInPortDef->format.video.xFramerate = pAppData->nFramerate << 16;
    pAppData->pInPortDef->format.video.nFrameWidth = pAppData->nWidth;
    pAppData->pInPortDef->format.video.nFrameHeight = pAppData->nHeight;

    eError = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, pAppData->pInPortDef);
    OMXENC_CHECK_EXIT(eError, "Error at SetParameter(IndexParamPortDefinition I)");
    /* To get nBufferSize */

    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (output) */
    InitOMXParams(pAppData->pOutPortDef);
    pAppData->pOutPortDef->nPortIndex = OMX_DirOutput;
    eError = OMX_GetParameter(pHandle, OMX_IndexParamPortDefinition, pAppData->pOutPortDef);
    OMXENC_CHECK_EXIT(eError, "Error at GetParameter(IndexParamPortDefinition O)");
    pAppData->pOutPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_VIDEO_CodingUnused;
    pAppData->pOutPortDef->format.video.eCompressionFormat = pAppData->eCompressionFormat;
    pAppData->pOutPortDef->format.video.nFrameWidth = pAppData->nWidth;
    pAppData->pOutPortDef->format.video.nFrameHeight = pAppData->nHeight;
    pAppData->pOutPortDef->format.video.nStride = pAppData->nWidth;
    pAppData->pOutPortDef->format.video.nSliceHeight = pAppData->nHeight;
    pAppData->pOutPortDef->format.video.nBitrate = pAppData->nBitrate;
    pAppData->pOutPortDef->format.video.xFramerate = pAppData->nFramerate << 16;

    eError = OMX_SetParameter(pHandle, OMX_IndexParamPortDefinition, pAppData->pOutPortDef);
    OMXENC_CHECK_EXIT(eError, "Error at SetParameter(IndexParamPortDefinition O)");

EXIT:
    return;
}

OMX_ERRORTYPE OMXENC_SetH264Parameter(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    /* Set the component's OMX_VIDEO_PARAM_AVCTYPE structure (output) */
    /* OMX_VIDEO_PORTDEFINITION values for output port */
    OMX_VIDEO_PARAM_AVCTYPE h264_video_param;
    memset(&h264_video_param, 0x0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    InitOMXParams(&h264_video_param);
    h264_video_param.nPortIndex = OMX_DirOutput;
    h264_video_param.eLevel = OMX_VIDEO_AVCLevel41;
    if (strcmp(pAppData->eProfile, "BP"))
        h264_video_param.eProfile = OMX_VIDEO_AVCProfileBaseline;
    else if (strcmp(pAppData->eProfile, "HP"))
        h264_video_param.eProfile = OMX_VIDEO_AVCProfileHigh;
    else if (strcmp(pAppData->eProfile, "MP"))
        h264_video_param.eProfile = OMX_VIDEO_AVCProfileMain;
    h264_video_param.nPFrames = pAppData->nPFrames;
    h264_video_param.nBFrames = pAppData->nBFrames;
    h264_video_param.bDirect8x8Inference = OMX_TRUE;
    h264_video_param.bEntropyCodingCABAC = OMX_TRUE;
    eError = OMX_SetParameter(pHandle, OMX_IndexParamVideoAvc, &h264_video_param);
    OMXENC_CHECK_EXIT(eError, "Error at SetParameter(IndexParamVideoAvc)");

    /* Set the component's OMX_NALSTREAMFORMATTYPE  structure (output) */
    if (pAppData->nCirMBs > 0) {
        if (pAppData->nCirMBs < 100) {
            OMX_VIDEO_PARAM_INTRAREFRESHTYPE intraRefresh;
            InitOMXParams(&intraRefresh);
            intraRefresh.nPortIndex = OMX_DirOutput;
            intraRefresh.eRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
            intraRefresh.nAirMBs = 0;
            intraRefresh.nAirRef = 0;
            intraRefresh.nCirMBs = (pAppData->nCirMBs * pAppData->nWidth * pAppData->nHeight) / (256 * 100);
            eError = OMX_SetParameter(pHandle,
                                      (OMX_INDEXTYPE)OMX_IndexParamVideoIntraRefresh, &intraRefresh);
            OMXENC_CHECK_EXIT(eError, "Error setConfig VideoBytestream");
        } else
            printf("cir mb over max limit\n");
    }

#if 0
    /* Set the component's OMX_NALSTREAMFORMATTYPE  structure (output) */
    OMX_NALSTREAMFORMATTYPE nalStreamFormat;
    InitOMXParams(&nalStreamFormat);
    nalStreamFormat.nPortIndex = OMX_DirOutput;
    nalStreamFormat.eNaluFormat = pAppData->eNaluFormat;
    eError = OMX_SetParameter(pHandle,
                              (OMX_INDEXTYPE)OMX_IndexParamNalStreamFormat, &nalStreamFormat);
    OMXENC_CHECK_EXIT(eError, "Error setConfig VideoBytestream");
#endif

EXIT:
    return eError;
}

OMX_ERRORTYPE OMXENC_SetMpeg4Parameter(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    /* Set the component's OMX_VIDEO_PARAM_H263TYPE structure (output) */
    if (pAppData->eCompressionFormat == OMX_VIDEO_CodingH263) {
        OMX_VIDEO_PARAM_H263TYPE h263_video_param;
        InitOMXParams(&h263_video_param);
        h263_video_param.nPortIndex = VIDENC_OUTPUT_PORT;
        h263_video_param.nGOBHeaderInterval = pAppData->nGOBHeaderInterval;
        h263_video_param.eLevel = OMX_VIDEO_H263Level10;
        h263_video_param.eProfile = OMX_VIDEO_H263ProfileBaseline;
        eError = OMX_SetParameter(pHandle, OMX_IndexParamVideoH263, &h263_video_param);
        OMXENC_CHECK_EXIT(eError, "Error at SetParameter");
    }

    /* Set the component's OMX_VIDEO_PARAM_MPEG4TYPE structure (output) */
    if (pAppData->eCompressionFormat == OMX_VIDEO_CodingMPEG4) {
        OMX_VIDEO_PARAM_MPEG4TYPE mpeg4_video_param;
        InitOMXParams(&mpeg4_video_param);
        mpeg4_video_param.nPortIndex = VIDENC_OUTPUT_PORT;
        mpeg4_video_param.eLevel = OMX_VIDEO_MPEG4Level0;
        mpeg4_video_param.eProfile = OMX_VIDEO_MPEG4ProfileSimple;

        eError = OMX_SetParameter(pHandle, OMX_IndexParamVideoMpeg4, &mpeg4_video_param);
        OMXENC_CHECK_EXIT(eError, "Error at SetParameter");
    }

EXIT:
    return eError;
}

void OMXENC_configCodec(MYDATATYPE* pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;

    setPortDefinition(pAppData);

    switch (pAppData->eCompressionFormat) {
    case OMX_VIDEO_CodingH263:
        eError = OMXENC_SetMpeg4Parameter(pAppData);
        OMXENC_CHECK_EXIT(eError, "Error returned from SetMpeg4Parameter()");
        break;
    case OMX_VIDEO_CodingMPEG4:
        eError = OMXENC_SetMpeg4Parameter(pAppData);
        OMXENC_CHECK_EXIT(eError, "Error returned from SetMpeg4Parameter()");
        break;
    case OMX_VIDEO_CodingAVC:
        eError = OMXENC_SetH264Parameter(pAppData);
        OMXENC_CHECK_EXIT(eError, "Error returned from SetH264Parameter()");
        break;
    case OMX_VIDEO_CodingVP8:
        break;
    default:
        OMXENC_PRINT("Invalid compression format value.\n");
        eError = OMX_ErrorUnsupportedSetting;
        goto EXIT;
    }

    OMX_VIDEO_PARAM_BITRATETYPE pParamBitrate;
    InitOMXParams(&pParamBitrate);
    pParamBitrate.nPortIndex = OMX_DirOutput;
    pParamBitrate.nTargetBitrate = pAppData->nBitrate;
    pParamBitrate.eControlRate = pAppData->eControlRate;
    eError = OMX_SetParameter(pHandle,
                              (OMX_INDEXTYPE)OMX_IndexParamVideoBitrate, &pParamBitrate);
    OMXENC_CHECK_EXIT(eError, "Error at SetParameter(OMX_IndexParamIntelBitrate");


EXIT:
    return ;
}

//component work thread
OMX_ERRORTYPE OMXENC_onEvent(OMX_HANDLETYPE hComponent, 
        OMX_PTR pData, 
        OMX_EVENTTYPE eEvent, 
        OMX_U32 nData1, 
        OMX_U32 nData2, 
        OMX_PTR pEventData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMXENC_PRINT("OnEvent: event 0x%x nData1 %d nData2 : 0x%x\n", eEvent, nData1, nData2);
    omx_message msg;
    msg.type = omx_message::EVENT;
    msg.node = pData;
    msg.u.event_data.event = eEvent;
    msg.u.event_data.data1 = nData1;
    msg.u.event_data.data2 = nData2;

    {
        Mutex::Autolock autoLock(mMsgLock);
        msgQueue.push_back(msg);
        mMessageReceive.signal();
    }


EXIT:
    return eError;
}

//component work thread
OMX_ERRORTYPE OMXENC_onEmptyBufferDone(OMX_HANDLETYPE hComponent, 
        OMX_PTR pData, 
        OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXENC_PRINT("onEmptyBufferDone %p  (%4d)\n", pBuffer, __LINE__);
    omx_message msg;

    msg.type = omx_message::EMPTY_BUFFER_DONE;
    msg.node = pData;
    msg.u.buffer_data.buffer = pBuffer;
    {
        Mutex::Autolock autoLock(mMsgLock);
        msgQueue.push_back(msg);
        mMessageReceive.signal();
    }


EXIT:
    return eError;
}

//component work thread
OMX_ERRORTYPE OMXENC_onFillBufferDone(OMX_HANDLETYPE hComponent, 
        OMX_PTR pData, 
        OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    omx_message msg;

    OMXENC_PRINT("onFillBufferDone %p  (%4d)\n", pBuffer, __LINE__);
    msg.type = omx_message::FILL_BUFFER_DONE;
    msg.node = pData;
    msg.u.buffer_data.buffer = pBuffer;
    {
        Mutex::Autolock autoLock(mMsgLock);
        msgQueue.push_back(msg);
        mMessageReceive.signal();
    }

EXIT:
    return eError;
}

//message thread
void *on_message(void *)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    omx_message msg;

    while(mRunning){
        Mutex::Autolock autoLock(mMsgLock);
        while(msgQueue.empty()){
            mMessageReceive.wait(mMsgLock);
                while(!msgQueue.empty()){
                    msg = *msgQueue.begin();
                    msgQueue.erase(msgQueue.begin());
                    OMXENC_PRINT(" message type %d flag \n", msg.type);

                    switch (msg.type) {
                        case omx_message::EVENT:
                            {
                                eError = OMXENC_EventHandler(msg.node, 
                                        msg.u.event_data.event, 
                                        msg.u.event_data.data1,
                                        msg.u.event_data.data2);
                                OMXENC_CHECK_EXIT(eError, "OMXENC_EventHandlererror ");
                            }
                            break;
                        case omx_message::FILL_BUFFER_DONE:
                            {
                                eError = OMXENC_FillBufferDone(msg.node, 
                                        (OMX_BUFFERHEADERTYPE *)msg.u.buffer_data.buffer);
                                OMXENC_CHECK_EXIT(eError, "OMXENC_EventHandlererror ");
                            }
                            break;
                        case omx_message::EMPTY_BUFFER_DONE:
                            {
                                eError = OMXENC_EmptyBufferDone(msg.node, 
                                        (OMX_BUFFERHEADERTYPE *)msg.u.buffer_data.buffer);
                                OMXENC_CHECK_EXIT(eError, "OMXENC_EventHandlererror ");
                            }
                            break;
                        default:
                            break;

                    }
                }
        }
    }

EXIT:
    return NULL;
}

OMX_ERRORTYPE OMXENC_Create(MYDATATYPE* pAppData)
{
    OMXENC_PRINT("=====test %s:%d ======", __FUNCTION__, __LINE__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle;
    int32_t err;
    OMX_CALLBACKTYPE sCb = {OMXENC_onEvent, 
                            OMXENC_onEmptyBufferDone, 
                            OMXENC_onFillBufferDone};

    mRunning = OMX_TRUE;
    err = pthread_create(&mThread, NULL, on_message, NULL);
    if(err != 0)
        printf("can't create thread: %s\n", strerror(err));

    if (pAppData->bShowPic)
        setenv("AUTO_UV", "test", 0);

    if (pAppData->eCompressionFormat == OMX_VIDEO_CodingAVC)
        StrVideoEncoder = StrVideoEncoderAVC;
    else if (pAppData->eCompressionFormat == OMX_VIDEO_CodingVP8) {
        ;
    }
    OMXENC_PRINT("StrVideoEncoder %s", StrVideoEncoder);

    pAppData->pCb = (OMX_CALLBACKTYPE *)malloc(sizeof(OMX_CALLBACKTYPE));
    pAppData->pInPortDef =
        (OMX_PARAM_PORTDEFINITIONTYPE *)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    pAppData->pOutPortDef =
        (OMX_PARAM_PORTDEFINITIONTYPE*)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    if (!pAppData->pCb && !pAppData->pInPortDef && !pAppData->pOutPortDef) {
        OMXENC_PRINT("malloc error port definition %d\n", __LINE__);
        goto EXIT;
    }

    /* Initialize OMX Core */
    eError = OMX_Init();
    OMXENC_CHECK_EXIT(eError, "OMX_Init error");

    *pAppData->pCb = sCb;
    /* Get VideoEncoder Component Handle */
    eError = OMX_GetHandle(&pHandle, StrVideoEncoder, pAppData, pAppData->pCb);
    OMXENC_CHECK_EXIT(eError, "OMX_GetHanle error ");
    if (pHandle == NULL) {
        OMXENC_PRINT("GetHandle return Null Pointer\n");
        return OMX_ErrorUndefined;
    }

    pAppData->pHandle = pHandle;
    OMXENC_PRINT("handle %x \n", pHandle);

    OMX_PARAM_COMPONENTROLETYPE cRole;
    InitOMXParams(&cRole);
    if (pAppData->eCompressionFormat == OMX_VIDEO_CodingAVC)
        strncpy((char*)cRole.cRole, "video_encoder.avc\0", 18);
    else if (pAppData->eCompressionFormat == OMX_VIDEO_CodingH263)
        strncpy((char*)cRole.cRole, "video_encoder.h263\0", 19);
    else if (pAppData->eCompressionFormat == OMX_VIDEO_CodingMPEG4)
        strncpy((char*)cRole.cRole, "video_encoder.mpeg4\0", 20);
    else if (pAppData->eCompressionFormat == OMX_VIDEO_CodingVP8)
        strncpy((char*)cRole.cRole, "video_encoder.vp8\0", 18);
    eError = OMX_SetParameter(pHandle, OMX_IndexParamStandardComponentRole, &cRole);
    OMXENC_CHECK_EXIT(eError, "set component role error");

    OMXENC_configCodec(pAppData);

    pAppData->eCurrentState = LOADED;

EXIT:
    return eError;
}

void allocateOnPort(MYDATATYPE* pAppData,
                    OMX_PARAM_PORTDEFINITIONTYPE* port,
                    OMX_BUFFERHEADERTYPE** buf)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    InitOMXParams(port);
    eError = OMX_GetParameter(pAppData->pHandle, OMX_IndexParamPortDefinition, port);
    OMXENC_CHECK_EXIT(eError, "Error at GetParameter(IndexParamPortDefinition I)");
    OMXENC_PRINT("%s Width %d, Height %d, nStride %d, nBufferSize %d, eColorFormat %d xFramerate %d buffer num %d\n",
                 port->nPortIndex == OMX_DirInput ? "Input" : "Output",
                 port->format.video.nFrameWidth,
                 port->format.video.nFrameHeight,
                 port->format.video.nStride,
                 port->nBufferSize,
                 port->format.video.eColorFormat,
                 port->format.video.xFramerate >> 16,
                 port->nBufferCountActual);

    for (int32_t nCounter = 0; nCounter < port->nBufferCountActual; nCounter++) {
        eError = OMX_AllocateBuffer(pAppData->pHandle,
                                    &buf[nCounter],
                                    port->nPortIndex,
                                    pAppData,
                                    port->nBufferSize);
        OMXENC_PRINT("%s, buf %p \n",
                     port->nPortIndex == OMX_DirInput ? "Input" : "Output", buf[nCounter]);

        OMXENC_CHECK_EXIT(eError, "Error OMX_AllocateBuffer");
    }

EXIT:
    return;
}

OMX_ERRORTYPE OMXENC_Init(MYDATATYPE* pAppData)
{
    OMXENC_PRINT("%s begin\n", __FUNCTION__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = pAppData->pHandle;
    OMX_U32 nCounter;

    eError = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMXENC_CHECK_EXIT(eError, "Error SendCommand OMX_StateIdle");
    setState(&pAppData->eCurrentState, LOADED_TO_IDLE);

    allocateOnPort(pAppData, pAppData->pOutPortDef, pAppData->pOutBuff);
    allocateOnPort(pAppData, pAppData->pInPortDef, pAppData->pInBuff);

    {
        Mutex::Autolock autoLock(mLock);
        while (pAppData->eCurrentState != EXECUTING)
            mAsyncCompletion.wait(mLock);
    }

EXIT:
    OMXENC_PRINT("%s end \n", __FUNCTION__);
    return eError;
}

void PrintHelpInfo(void)
{
    printf("./omx_encode <options>\n");
    printf("   -plog print the trace log\n");
    printf("   -show_pic encode and show the child picture \n");
    printf("   -t <H264|MPEG4|H263|JPEG|VP8>\n");
    printf("   -w <width> -h <height>\n");
    printf("   -framecount [num]\n");
    printf("   -n [num]\n");
    printf("   -o <coded file>\n");
    printf("   -f [num]\n");
    printf("   -configfile <framename> read dynamic RC settings from configure file\n");
    printf("   -npframe [num] number of P frames between I frames\n");
    printf("   -nbframe [num] number of B frames between I frames\n");
    printf("   -idrinterval [num]  Specifics the number of I frames between two IDR frames (for H264 only)\n");
    printf("   -bitrate [num]\n");
    printf("   -initialqp [num], initial_qp of VAEncMiscParameterRateControl structure\n");
    printf("   -minqp [num], min_qp of VAEncMiscParameterRateControl structure\n");
    printf("   -maxqp [num], max_qp of VAEncMiscParameterRateControl structure\n");
    printf("   -rcMode <NONE|CBR|VBR|VCM|CQP|VBR_CONTRAINED>\n");
    printf("   for VP8 encode VCM mode means FW CBR_HRD mode\n");
    printf("   -syncmode: sequentially upload source, encoding, save result, no multi-thread\n");
    printf("   -metadata: metadata mode enable\n");
    printf("   -surface <malloc|gralloc> \n");
    printf("   -srcyuv <filename> load YUV from a file\n");
    printf("   -fourcc <NV12|IYUV|YV12> source YUV fourcc\n");
    printf("   -recyuv <filename> save reconstructed YUV into a file\n");
    printf("   -profile <BP|MP|HP>\n");
    printf("   -level <level>\n");
    printf("   -islicenum [num] I frame slice number\n");
    printf("   -pslicenum [num] P frame slice number\n");
    printf("   -maxslicesize [num] max slice size\n");
    printf("   -windowsize [num] window size in milliseconds allowed for bitrate to reach target\n");
    printf("   -maxencodebitrate [num] max bitrate\n");
    printf("   -targetpercentage [num] target percentage\n");
    printf("   -airenable Enable AIR\n");
    printf("   -airauto AIR auto\n");
    printf("   -airmbs [num] minimum number of macroblocks to refresh in a frame when adaptive intra-refresh (AIR) is enabled\n");
    printf("   -air_threshold [num] AIR threshold\n");
    printf("   -pbitrate show dynamic bitrate result\n");
    printf("   -num_cir_mbs [num] number of macroblocks to be coded as intra when cyclic intra-refresh (CIR) is enabled\n");
    printf("   -layer the number of temporal layer should be [1,3]\n");
    printf("   -bitrate0 the bitrate of layer0 for temporal layer encode\n");
    printf("   -bitrate1 the bitrate of layer0+layer1 for temporal layer encode\n");
    printf("   -bitrate2 the bitrate of layer0+layer1+layer2 for temporal layer encode\n");
    printf("example:\n\t omxenc -w 1920 -h 1080 -bitrate 10000000 -minqp 0 -initialqp 0 -intra_period 30 -idrinterval 2\n");
    printf("example for VP8 two temporal layers encode:\n\t omxenc \
            -t VP8 -layer 2 -w [width] -h [height] -bitrate [bitrate] \
            -bitrate0 [bitrate0] -bitrate1 [bitrate1] -rcMode [CBR|VCM] \
            -o [output file path] -n [framecount] -srcyuv [src yuv path]\n");
    printf("example for VP8 three temporal layers encode:\n\t omxenc -t VP8 \
            -layer 3 -w [width] -h [height] -bitrate [bitrate] -bitrate0 [bitrate0]\
            -bitrate1 [bitrate1] -bitrate2 [bitrate2] -rcMode [CBR|VCM] \
            -o [output file path] -n [framecount] -srcyuv [src yuv path]\n");

}

static int print_input(MYDATATYPE* pAppData)
{
    printf("\n");
    printf("INPUT:    Try to encode %s...\n", pAppData->szCompressionFormat);
    printf("INPUT:    Resolution   : %dx%d, %d frames\n",
           pAppData->nWidth, pAppData->nHeight, pAppData->nFramecount);
    printf("INPUT:    Source YUV   : %s", pAppData->szInFile ? "FILE" : "AUTO generated");
    printf("\n");

    printf("INPUT:    RateControl  : %s\n", pAppData->szControlRate);
    printf("INPUT:    FrameRate    : %d\n", pAppData->nFramerate);
    printf("INPUT:    Bitrate      : %d(initial)\n", pAppData->nBitrate);
    //printf("INPUT:    Slieces      : %d\n", frame_slice_count);
    printf("INPUT:    IntraPeriod  : %d\n", pAppData->nPFrames + 1);
    printf("INPUT:    IDRPeriod    : %d\n", pAppData->nIDRInterval * (pAppData->nPFrames + 1));
    printf("INPUT:    IpPeriod     : %d\n", pAppData->nBFrames + 1);
    printf("INPUT:    Initial QP   : %d\n", pAppData->nInitQP);
    printf("INPUT:    Min QP       : %d\n", pAppData->nMinQP);
    printf("INPUT:    Layer Number : %d\n", pAppData->number_of_layer);
    printf("\n"); /* return back to startpoint */

    return 0;
}

OMX_ERRORTYPE OMXENC_CheckArgs(int argc, char** argv, MYDATATYPE** pAppDataTmp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    MYDATATYPE* pAppData;

    pAppData = (MYDATATYPE*)malloc(sizeof(MYDATATYPE));
    if (!pAppData) {
        printf("malloc(pAppData) error. %d\n", __LINE__);
        exit(0);
    }
    memset(pAppData, 0, sizeof(MYDATATYPE));

    *pAppDataTmp = pAppData;
    bPrintLog = OMX_FALSE;
    //pAppData->eNaluFormat = OMX_NaluFormatStartCodes;
    //pAppData->eNaluFormat = (OMX_NALUFORMATSTYPE)OMX_NaluFormatStartCodesSeparateFirstHeader;
    pAppData->eCurrentState = UNLOADED;
    pAppData->nQPIoF = 0;
    pAppData->nFlags = 0;
    pAppData->nISliceNum = 2;
    pAppData->nPSliceNum = 2;
    pAppData->nIDRInterval = 0;
    pAppData->nPFrames = 20;
    pAppData->nBFrames = 0;
    pAppData->nSavedFrameNum = 0;
    pAppData->nFramecount = 50;
    memcpy(pAppData->szInFile, "auto", 4);
    memcpy(pAppData->szOutFile, "/data/out.264", 13);
    pAppData->nWidth = 320;
    pAppData->nHeight = 240;
    pAppData->nSliceSize = 0;
    pAppData->nGOBHeaderInterval = 30;
    pAppData->eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pAppData->nFramerate = 30;
    pAppData->nBitrate = 15000000;
    pAppData->eCompressionFormat = OMX_VIDEO_CodingAVC;
    strncpy(pAppData->szCompressionFormat, "h264", 4);
    pAppData->eControlRate = OMX_Video_ControlRateDisable;
    strncpy(pAppData->szControlRate, "vbr", 3);
    pAppData->eLevel = OMX_VIDEO_AVCLevel31;
    strncpy(pAppData->eProfile, "BP", 2);
    pAppData->nMinQP = 0;
    pAppData->nInitQP = 0;
    pAppData->nMaxQP = 51;
    pAppData->nMaxEncodeBitrate = 15000000;
    pAppData->nTargetPercentage = 66;
    pAppData->nWindowSize = 1000;
    pAppData->bFrameSkip = OMX_FALSE;
    pAppData->bShowPic = OMX_FALSE;
    pAppData->nDisableBitsStuffing =  0;
    pAppData->bMetadataMode = OMX_FALSE;
    pAppData->bSyncMode = OMX_FALSE;
    pAppData->bShowBitrateRealTime = OMX_FALSE;
    pAppData->number_of_rewind = 0;
    //config_fn = "/data/config-omx-test";
    //AIR
    pAppData->bAirAuto = OMX_FALSE;
    pAppData->bAirEnable = OMX_FALSE;
    pAppData->nAirMBs = 0;
    pAppData->nAirThreshold = 0;

    //Intra refresh
    pAppData->eRefreshMode = OMX_VIDEO_IntraRefreshAdaptive;
    pAppData->nIrRef = 0;
    pAppData->nCirMBs = 0;

    //
    pAppData->bSNAlter = OMX_FALSE;
    pAppData->bRCAlter = OMX_FALSE;
    pAppData->bDeblockFilter = OMX_FALSE;
    //
    pAppData->only_render_k_frame_one_time = 0;
    pAppData->max_frame_size_ratio = 0;
    // for sand test app
    pAppData->number_of_layer = 1;
    pAppData->nBitrateForLayer0 = 0;
    pAppData->nBitrateForLayer1 = 0;
    pAppData->nBitrateForLayer2 = 0;
    pAppData->nFramerateForLayer0 = 0;
    pAppData->nFramerateForLayer1 = 0;
    pAppData->nFramerateForLayer2 = 0;

    const struct option long_opts[] = {
        {"help", no_argument, NULL, 0 },
        {"bitrate", required_argument, NULL, 1 },
        {"minqp", required_argument, NULL, 2 },
        {"initialqp", required_argument, NULL, 3 },
        {"intra_period", required_argument, NULL, 4 },
        {"idr_period", required_argument, NULL, 5 },
        {"ip_period", required_argument, NULL, 6 },
        {"rcMode", required_argument, NULL, 7 },
        {"srcyuv", required_argument, NULL, 9 },
        {"recyuv", required_argument, NULL, 10 },
        {"fourcc", required_argument, NULL, 11 },
        {"syncmode", no_argument, NULL, 12 },
        {"enablePSNR", no_argument, NULL, 13 },
        {"surface", required_argument, NULL, 14 },
        {"priv", required_argument, NULL, 15 },
        {"framecount", required_argument, NULL, 16 },
        {"entropy", required_argument, NULL, 17 },
        {"profile", required_argument, NULL, 18 },
        {"sliceqp", required_argument, NULL, 19 },
        {"level", required_argument, NULL, 20 },
        {"stridealign", required_argument, NULL, 21 },
        {"heightalign", required_argument, NULL, 22 },
        {"surface", required_argument, NULL, 23 },
        {"slices", required_argument, NULL, 24 },
        {"configfile", required_argument, NULL, 25 },
        {"quality", required_argument, NULL, 26 },
        {"autokf", required_argument, NULL, 27 },
        {"kf_dist_min", required_argument, NULL, 28 },
        {"kf_dist_max", required_argument, NULL, 29 },
        {"error_resilient", required_argument, NULL, 30 },
        {"maxqp", required_argument, NULL, 31},
        {"bit_stuffing", no_argument, NULL, 32},
        {"plog", no_argument, NULL, 33},
        {"metadata", no_argument, NULL, 34},
        {"qpi", no_argument, NULL, 35},
        {"vuiflag", no_argument, NULL, 36},
        {"forceIDR", no_argument, NULL, 37},
        {"refreshIntraPeriod", no_argument, NULL, 38},
        {"windowsize", no_argument, NULL, 39},
        {"maxencodebitrate",  required_argument, NULL, 40},
        {"targetpercentage",   required_argument, NULL, 41},
        {"islicenum", required_argument, NULL, 42},
        {"pslicenum", required_argument, NULL, 43},
        {"idrinterval", required_argument, NULL, 44},
        {"npframe", required_argument, NULL, 45},
        {"nbframe", required_argument, NULL, 46},
        {"rc_alter", no_argument, NULL, 47},
        {"bit_stuffing_dis", no_argument, NULL, 48},
        {"frame_skip_dis", no_argument, NULL, 49},
        {"num_cir_mbs", required_argument, NULL, 50},
        {"maxslicesize", required_argument, NULL, 51},
        {"intrarefresh", required_argument, NULL, 52},
        {"airmbs", required_argument, NULL, 53},
        {"airref", required_argument, NULL, 54},
        {"airenable", no_argument, NULL, 56},
        {"airauto", no_argument, NULL, 57},
        {"air_threshold", required_argument, NULL, 58},
        {"pbitrate", no_argument, NULL, 59},
        {"show_pic", no_argument, NULL, 60},
        {"max_frame_size_ratio", required_argument, NULL, 61},
        {"layer", required_argument, NULL, 62},
        {"bitrate0", required_argument, NULL, 63},
        {"bitrate1", required_argument, NULL, 64},
        {"bitrate2", required_argument, NULL, 65},
        {"framerate0", required_argument, NULL, 66},
        {"framerate1", required_argument, NULL, 67},
        {"framerate2", required_argument, NULL, 68},
        {"hh", no_argument, NULL, 69},
        {NULL, no_argument, NULL, 0 }
    };

    int long_index, i, tmp, intra_idr_period_tmp = -1;
    char c;
    int argNum;
    while ((c = getopt_long_only(argc, argv, "t:w:h:n:r:f:o:?", long_opts, &long_index)) != EOF) {
        switch (c) {
        case 69:
        case 0:
            PrintHelpInfo();
            exit(0);
        case 33:
            bPrintLog = OMX_TRUE;
            break;
        case 59:
            pAppData->bShowBitrateRealTime = OMX_TRUE;
            break;
        case 60: //depend on the common load_surface.h has the blend picture fucntion
            pAppData->bShowPic = OMX_TRUE;
            break;
        case 9:
            strncpy(pAppData->szInFile, optarg, strlen(optarg));
            pAppData->fIn = fopen(pAppData->szInFile, "r");
            if (!pAppData->fIn) {
                printf("Failed to open input file <%s>", pAppData->szInFile);
                return OMX_ErrorBadParameter;
            }
            break;
        case 'w':
            pAppData->nWidth = atoi(optarg);
            break;
        case 'h':
            pAppData->nHeight = atoi(optarg);
            break;
        case 'f':
            pAppData->nFramerate = atoi(optarg);
            break;
        case  1:
            pAppData->nBitrate = strtol(optarg, NULL, 0);
            break;
        case 14:
            break;
        case 't':
            //Select encoding type
            memset(pAppData->szCompressionFormat, 0x00, 128 * sizeof(char));
            strncpy(pAppData->szCompressionFormat, optarg, strlen(optarg));
            if (!strcmp(optarg, "H263")) {
                pAppData->eCompressionFormat = OMX_VIDEO_CodingH263;
                memcpy(pAppData->szOutFile, "/data/out.263", 13);
            } else if (!strcmp(optarg, "MPEG4")) {
                pAppData->eCompressionFormat = OMX_VIDEO_CodingMPEG4;
            } else if (!strcmp(optarg, "H264")) {
                pAppData->eCompressionFormat = OMX_VIDEO_CodingAVC;
            } else if (!strcmp(optarg, "VP8")) {
                pAppData->eCompressionFormat = OMX_VIDEO_CodingVP8;
            } else {
                PrintHelpInfo();
                exit(0);
            }
            break;
        case 20:
            pAppData->eLevel = atoi(optarg);
            break;
        case 'o':
            memset(pAppData->szOutFile, 0 , sizeof(pAppData->szOutFile));
            strncpy(pAppData->szOutFile, optarg, strlen(optarg));
            break;
        case 18:
            strncpy(pAppData->eProfile, optarg, strlen(optarg));
            break;
        case 34:
            pAppData->bMetadataMode = OMX_TRUE;
            break;
        case 12:
            pAppData->bSyncMode = OMX_TRUE;
            break;
        case 7:
            //Select encoding type
            memset(pAppData->szControlRate, 0x00, sizeof(pAppData->szControlRate));
            strncpy(pAppData->szControlRate, optarg, strlen(optarg));
            if (!strcmp(optarg, "CBR")) {
                pAppData->eControlRate = OMX_Video_ControlRateConstant;
            } else if (!strcmp(optarg, "VBR")) {
                pAppData->eControlRate = OMX_Video_ControlRateVariable;
            } else if (!strcmp(optarg, "disable")) {
                pAppData->eControlRate = OMX_Video_ControlRateDisable;
            } else {
                pAppData->eControlRate = OMX_Video_ControlRateVariable;
            }
            break;
        case 4:
            pAppData->nPFrames = atoi(optarg) - 1;
            break;
        case 5:
            pAppData->nIDRInterval = atoi(optarg);
            break;
        case 45:
            pAppData->nPFrames = atoi(optarg);
            break;
        case 'n':
        case 16:
            pAppData->nFramecount = atoi(optarg);
            break;
        case 35:
            pAppData->nQpI = atoi(optarg);
            break;
        case 25:
            break;
        case 36:
            pAppData->bVUIEnable = OMX_TRUE;
            break;
        case 2:
            pAppData->nMinQP = atoi(optarg);
            break;
        case 3:
            pAppData->nInitQP = atoi(optarg);
            break;
        case 31:
            pAppData->nMaxQP = atoi(optarg);
            break;
        case 39:
            pAppData->nWindowSize = atoi(optarg);
            break;
        case 40:
            pAppData->nMaxEncodeBitrate = atoi(optarg);
            break;
        case 41:
            pAppData->nTargetPercentage = atoi(optarg);
            break;
        case 42:
            pAppData->nISliceNum = atoi(optarg);
            break;
        case 43:
            pAppData->nPSliceNum = atoi(optarg);
            break;
        case 44:
            pAppData->nIDRInterval = atoi(optarg);
            break;
        case 46:
            pAppData->nBFrames = atoi(optarg);
            break;
        case 47:
            pAppData->bRCAlter = OMX_TRUE;
            break;
        case 48:
            pAppData->nDisableBitsStuffing = 1;
            break;
        case 49:
            pAppData->bFrameSkip = OMX_TRUE;
            break;
        case 50:
            pAppData->nCirMBs = atoi(optarg);
            break;
        case 51:
            pAppData->nSliceSize = atoi(optarg);
            if (pAppData->nSliceSize > pAppData->nWidth * pAppData->nHeight * 3 / 2) {
                printf("error slice size setting\n");
                exit(0);
            }
            break;
        case 52:
            pAppData->eRefreshMode = (OMX_VIDEO_INTRAREFRESHTYPE)atoi(optarg);
            break;
        case 53:
            pAppData->nAirMBs = atoi(optarg);
            break;
        case 54:
            pAppData->nIrRef = atoi(optarg);
            break;
        case 56:
            pAppData->bAirEnable = OMX_TRUE;
            break;
        case 57:
            pAppData->bAirAuto = OMX_TRUE;
            break;
        case 58:
            pAppData->nAirThreshold = atoi(optarg);
            break;
        case 61:
            pAppData->max_frame_size_ratio = atoi(optarg);
            break;
        case 62:
            pAppData->number_of_layer = atoi(optarg);
            break;
        case 63:
            pAppData->nBitrateForLayer0 = strtol(optarg, NULL, 0);
            break;
        case 64:
            pAppData->nBitrateForLayer1 = strtol(optarg, NULL, 0);
            break;
        case 65:
            pAppData->nBitrateForLayer2 = strtol(optarg, NULL, 0);
            break;
        case 66:
            pAppData->nFramerateForLayer0 = strtol(optarg, NULL, 0);
            break;
        case 67:
            pAppData->nFramerateForLayer1 = strtol(optarg, NULL, 0);
            break;
        case 68:
            pAppData->nFramerateForLayer2 = strtol(optarg, NULL, 0);
            break;


        }
    }

    pAppData->fOut = fopen(pAppData->szOutFile, "w");
    if (!pAppData->fOut) {
        printf("Failed to open output file <%s>\n", pAppData->szOutFile);
        return OMX_ErrorBadParameter;
    }

    print_input(pAppData);
EXIT:
    return eError;
}

int main(int argc, char** argv)
{
    printf("=====test======");
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    MYDATATYPE *pAppData;
    printf("=====test %s:%d ======", __FUNCTION__, __LINE__);

    eError = OMXENC_CheckArgs(argc, argv, &pAppData);
    OMXENC_CHECK_ERROR(eError, "Error OMXENC_CheckArgs");
    OMXENC_PRINT("=====test %s:%d ======", __FUNCTION__, __LINE__);

    pAppData->nEncBegin = pAppData->nTc2 = GetTickCount();
    OMXENC_PRINT("=====test %s:%d ======", __FUNCTION__, __LINE__);

    eError = OMXENC_Create(pAppData);
    OMXENC_CHECK_ERROR(eError, "Error OMXENC_Create");
    OMXENC_PRINT("=====test %s:%d ======", __FUNCTION__, __LINE__);

    eError = OMXENC_Init(pAppData);
    OMXENC_CHECK_ERROR(eError, "Error OMXENC_Init");


    eError = OMXENC_SetConfig(pAppData);
    OMXENC_CHECK_EXIT(eError, "Error OMXENC_SetConfig\n");


    eError = OMXENC_Read(pAppData);
    OMXENC_CHECK_ERROR(eError, "Error at  OMXENC_Read");


    eError = OMXENC_Stop(pAppData);
    OMXENC_CHECK_ERROR(eError, "Error at  OMXENC_Stop");

    pAppData->nCurrentFrameIn = pAppData->nCurrentFrameIn - pAppData->number_of_rewind;

    pAppData->nEncEnd = GetTickCount();

    printf("\n\n");
    printf("PERFORMANCE:          Number of frames encoded : %ldframes\n", pAppData->nCurrentFrameOut - 1);
    printf("PERFORMANCE:      Total size of frames encoded : %ldkb\n", pAppData->nWriteLen / 1000);
    printf("PERFORMANCE:                Total time elapsed : %dms\n", pAppData->nEncEnd - pAppData->nEncBegin);
    printf("PERFORMANCE:          Time upload data encoder : %dms\n", pAppData->nUpload);
    printf("PERFORMANCE:        Time spend at encoder side : %dms\n",
           pAppData->nEncEnd - pAppData->nEncBegin - pAppData->nUpload);
    printf("PERFORMANCE:   Resolution(%4dx%4d) performance : %ffps\n",
           pAppData->nWidth, pAppData->nHeight,
           (float)(pAppData->nCurrentFrameOut) / (float)(pAppData->nEncEnd - pAppData->nEncBegin) * 1000);

    eError = OMXENC_DeInit(pAppData);

EXIT:
    exit(0);
    return eError;
}

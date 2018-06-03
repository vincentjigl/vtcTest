LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
#LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := \
                          libcutils \
                          libutils \
                          liblog


LOCAL_C_INCLUDES := \
    frameworks/native/include/ui \
    hardware/rk29/rockchip_omx/core/\
    hardware/rk29/rockchip_omx/include/rockchip \
    hardware/rk29/rockchip_omx/include/khronos \

LOCAL_SHARED_LIBRARIES += \
                          libOMX_Core \
                          libvencoder\
                          #libomxvpu_enc \
#libstagefrighthw
LOCAL_STATIC_LIBRARIES := libstagefrighthw
#libRkOMX_Basecomponent \
                          libRkOMX_OSAL \

LOCAL_SRC_FILES := omxenc.cpp 
LOCAL_MODULE := omxenc 

include $(BUILD_EXECUTABLE)


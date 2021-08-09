LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := vtcTest.cpp

LOCAL_C_INCLUDES += \
    $(TOP)/frameworks/native/include/media/openmax \
    $(TOP)/frameworks/native/include/media/hardware\
    $(TOP)/frameworks/native/include\
    $(TOP)/frameworks/av/include\
    $(TOP)/frameworks/av/include/media\
    $(TOP)/frameworks/base/include

LOCAL_SHARED_LIBRARIES := \
    libcamera_client \
    libmedia \
    libbinder \
    libcutils \
    libutils \
    libui \
    liblog \
    libgui

LOCAL_SHARED_LIBRARIES += \
    libstagefright \
    libstagefright_foundation

LOCAL_CFLAGS +=-Wall -fno-short-enums -O0 -g -D___ANDROID___ $(ANDROID_API_CFLAGS)
LOCAL_CFLAGS += -Wno-unused-parameter \
                -Wno-unused-variable \
                -Wno-unknown-escape-sequence \
                -Wno-unused-comparison
#                -Wno-
LOCAL_LDFLAGS := -Wl #--unresolved-symbols=ignore-all

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE := vtcTest
include $(BUILD_EXECUTABLE)
#include $(BUILD_STATIC_LIBRARY)

###############################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES := VTCLoopback.cpp IOMXEncoder.cpp IOMXDecoder.cpp

LOCAL_C_INCLUDES += \
    $(DOMX_PATH)/omx_core/inc \

LOCAL_C_INCLUDES += \
    $(TOP)/frameworks/native/include \
    $(TOP)/frameworks/native/include/media/openmax \
    $(TOP)/frameworks/native/include/media/hardware\
    $(TOP)/frameworks/av/include\
    $(TOP)/frameworks/base/include

LOCAL_SHARED_LIBRARIES := \
    libcamera_client \
    libstagefright \
    libmedia \
    libbinder \
    libcutils \
    libutils \
    liblog \
    libgui \
    libui

LOCAL_SHARED_LIBRARIES += \
    libstagefright_foundation

LOCAL_CFLAGS +=-Wall -fno-short-enums -O0 -g $(ANDROID_API_CFLAGS)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE := vtcloopback
#include $(BUILD_EXECUTABLE)

###############################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= CameraHardwareInterfaceTest.cpp

ifdef ANDROID_API_JB_OR_LATER
LOCAL_C_INCLUDES += \
    $(TOP)/frameworks/av/services/camera/libcameraservice
else
LOCAL_C_INCLUDES += \
    $(TOP)/frameworks/base/include \
    $(TOP)/frameworks/base/services/camera/libcameraservice
endif

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libutils \
    libcutils \
    libbinder \
    libmedia \
    libui \
    libgui \
    libcamera_client \
    libhardware

LOCAL_MODULE:= CameraHardwareInterfaceTest
LOCAL_MODULE_TAGS:= tests

LOCAL_CFLAGS += -Wall -fno-short-enums -O0 -g -D___ANDROID___ $(ANDROID_API_CFLAGS)

# Add TARGET FLAG for OMAP4 and OMAP5 boards only
# First eliminate OMAP3 and then ensure that this is not used
# for customer boards.
ifneq ($(TARGET_BOARD_PLATFORM),omap3)
    ifeq ($(findstring omap, $(TARGET_BOARD_PLATFORM)),omap)
        LOCAL_CFLAGS += -DTARGET_OMAP4
    endif
endif

#include $(BUILD_HEAPTRACKED_EXECUTABLE)


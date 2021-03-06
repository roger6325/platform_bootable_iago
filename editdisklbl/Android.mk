LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),x86)

LOCAL_SRC_FILES := \
	editdisklbl.c \
        config_mbr.c \
        diskconfig.c \
        diskutils.c \
        write_lst.c


LOCAL_CFLAGS := -O2 -g -W -Wall -Werror -D_LARGEFILE64_SOURCE

LOCAL_MODULE := editdisklbl
LOCAL_STATIC_LIBRARIES := libcutils liblog

include $(BUILD_HOST_EXECUTABLE)

endif   # TARGET_ARCH == x86

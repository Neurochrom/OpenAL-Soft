LOCAL_PATH := $(call my-dir)

########################################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE     := openal

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../include
LOCAL_C_INCLUDES        := $(LOCAL_EXPORT_C_INCLUDES)           \
                           $(LOCAL_PATH)                        \
                           $(LOCAL_PATH)/../../Alc              \
                           $(LOCAL_PATH)/../../OpenAL32/Include \

LOCAL_SRC_FILES  := ../../OpenAL32/alAuxEffectSlot.c \
                    ../../OpenAL32/alBuffer.c        \
                    ../../OpenAL32/alEffect.c        \
                    ../../OpenAL32/alError.c         \
                    ../../OpenAL32/alExtension.c     \
                    ../../OpenAL32/alFilter.c        \
                    ../../OpenAL32/alListener.c      \
                    ../../OpenAL32/alMidi.c          \
                    ../../OpenAL32/alSource.c        \
                    ../../OpenAL32/alState.c         \
                    ../../OpenAL32/alThunk.c         \
                    ../../Alc/ALc.c                  \
                    ../../Alc/alcConfig.c            \
                    ../../Alc/alcRing.c              \
                    ../../Alc/ALu.c                  \
                    ../../Alc/bs2b.c                 \
                    ../../Alc/helpers.c              \
                    ../../Alc/hrtf.c                 \
                    ../../Alc/mixer.c                \
                    ../../Alc/mixer_c.c              \
                    ../../Alc/panning.c              \
                    ../../Alc/threads.c              \
                    ../../Alc/backends/base.c        \
                    ../../Alc/backends/loopback.c    \
                    ../../Alc/backends/opensl.c      \
                    ../../Alc/backends/null.c        \
                    ../../Alc/effects/autowah.c      \
                    ../../Alc/effects/chorus.c       \
                    ../../Alc/effects/compressor.c   \
                    ../../Alc/effects/dedicated.c    \
                    ../../Alc/effects/distortion.c   \
                    ../../Alc/effects/echo.c         \
                    ../../Alc/effects/equalizer.c    \
                    ../../Alc/effects/flanger.c      \
                    ../../Alc/effects/modulator.c    \
                    ../../Alc/effects/null.c         \
                    ../../Alc/effects/reverb.c       \

ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS    += -DHAVE_NEON=1 -DHAVE_ARM_NEON_H=1
LOCAL_SRC_FILES += ../../Alc/mixer_neon.c.arm.neon
endif

ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_CFLAGS    += -DHAVE_SSE=1 -DHAVE_XMMINTRIN_H=1 -DHAVE_CPUID_H=1
LOCAL_SRC_FILES += ../../Alc/mixer_sse.c
endif

LOCAL_CFLAGS += -ffast-math -std=c99 -DAL_BUILD_LIBRARY -DAL_ALEXT_PROTOTYPES
LOCAL_LDLIBS += -lOpenSLES

LOCAL_STATIC_LIBRARIES := cpufeatures

include $(BUILD_SHARED_LIBRARY)

$(call import-module,cpufeatures)

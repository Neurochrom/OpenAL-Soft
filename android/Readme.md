OpenAL Soft for Android
=======================

Running ndk-build in this folder will build OpenAL Soft on Android using OpenSL backend.

OpenSL backend is available starting with Android 2.3 (API level 9).

To use OpenAL in your application, you can build it when you are building your Android.mk. To do that, include following line in your Android.mk:

    include path/to/openal-soft/jni/Android.mk

Or you can build libraries here and add following fragment to your Android.mk file:

    include $(CLEAR_VARS)
    LOCAL_MODULE := openal
    LOCAL_SRC_FILES := path/to/openal-soft/libs/$(TARGET_ARCH_ABI)/libopenal.so
    LOCAL_EXPORT_C_INCLUDES := path/to/openal-soft/include
    include $(PREBUILT_SHARED_LIBRARY)

After that you can simply add `openal` to `LOCAL_SHARED_LIBRARIES` variable for your library.

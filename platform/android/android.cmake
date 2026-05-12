# Android platform integration
# Included by app/CMakeLists.txt when ANDROID=1

# Required Android properties
set_property(TARGET neurx_app PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/package)

set_property(TARGET neurx_app PROPERTY QT_ANDROID_ABIS
    "arm64-v8a;armeabi-v7a;x86_64")

set_property(TARGET neurx_app PROPERTY QT_ANDROID_MIN_SDK_VERSION 26)
set_property(TARGET neurx_app PROPERTY QT_ANDROID_TARGET_SDK_VERSION 34)

set_property(TARGET neurx_app APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
    ${CMAKE_BINARY_DIR}/core/libneurx_core.so
)

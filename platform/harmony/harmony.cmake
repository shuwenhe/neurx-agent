# HarmonyOS cross-compilation notes
#
# HarmonyOS uses the OHOS (OpenHarmony) NDK toolchain.
# Pass -DCMAKE_TOOLCHAIN_FILE=<OHOS_SDK>/native/cmake/ohos.toolchain.cmake
# and -DHARMONY_SDK_ROOT=<OHOS_SDK> when configuring.
#
# The Qt for HarmonyOS port (available from Qt 6.7+) provides Qt Quick
# support through the OHOS platform plugin.

if(NEURX_PLATFORM_HARMONY)
    message(STATUS "Configuring for HarmonyOS (OHOS)")

    set_target_properties(neurx_app PROPERTIES
        OUTPUT_NAME "neurx"
    )

    target_compile_definitions(neurx_app PRIVATE NEURX_HARMONY)

    # OHOS requires shared libraries packaged in the .hap
    set_target_properties(neurx_core PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

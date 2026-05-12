# iOS platform integration
# Included by app/CMakeLists.txt when IOS=1

set_target_properties(neurx_app PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME          "NeurX"
    MACOSX_BUNDLE_GUI_IDENTIFIER       "io.neurx.app"
    MACOSX_BUNDLE_BUNDLE_VERSION       ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST           ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY     "1,2"
    XCODE_ATTRIBUTE_SWIFT_VERSION              ""
)

# Codesign placeholder – set via environment in CI
set_target_properties(neurx_app PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
)

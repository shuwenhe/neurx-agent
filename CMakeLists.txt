cmake_minimum_required(VERSION 3.21)

project(neurx
    VERSION 0.1.0
    DESCRIPTION "NeurX AgentOS - Cross-Platform Agent Operating System"
    HOMEPAGE_URL "https://github.com/neurx/neurx"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --------------------------------------------------------------------------
# Qt6 components
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Network
    Concurrent
    Sql
    Qml
)
# WebSockets is optional — install via Qt Maintenance Tool if needed
find_package(Qt6 OPTIONAL_COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    message(STATUS "Qt6 WebSockets: found")
else()
    message(WARNING "Qt6 WebSockets not found — WebSocket features disabled")
endif()

qt_standard_project_setup(REQUIRES 6.5)

# --------------------------------------------------------------------------
# Platform detection
# --------------------------------------------------------------------------
if(ANDROID)
    message(STATUS "Platform: Android")
elseif(IOS)
    message(STATUS "Platform: iOS")
elseif(WIN32)
    message(STATUS "Platform: Windows")
elseif(APPLE)
    message(STATUS "Platform: macOS")
elseif(UNIX)
    message(STATUS "Platform: Linux")
endif()

# HarmonyOS cross-compilation toolchain detection
if(DEFINED HARMONY_SDK_ROOT)
    message(STATUS "Platform: HarmonyOS")
    set(NEURX_PLATFORM_HARMONY ON)
endif()

# --------------------------------------------------------------------------
# Global compile definitions
# --------------------------------------------------------------------------
add_compile_definitions(
    NEURX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    NEURX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    NEURX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    $<$<CONFIG:Debug>:NEURX_DEBUG>
)

# --------------------------------------------------------------------------
# Subprojects
# --------------------------------------------------------------------------
add_subdirectory(core)
add_subdirectory(plugins)
add_subdirectory(app)

# --------------------------------------------------------------------------
# Install rules
# --------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS neurx_app
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
qt_generate_deploy_qml_app_script(
    TARGET neurx_app
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

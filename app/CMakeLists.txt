cmake_minimum_required(VERSION 3.21)

qt_add_executable(neurx_app
    main.cpp
    bridge/RuntimeBridge.cpp
    bridge/AgentListModel.cpp
    bridge/LogModel.cpp
)

qt_add_qml_module(neurx_app
    URI neurx
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppShell.qml
        qml/pages/DashboardPage.qml
        qml/pages/AgentsPage.qml
        qml/pages/ChatPage.qml
        qml/pages/ToolsPage.qml
        qml/pages/SettingsPage.qml
        qml/components/AgentCard.qml
        qml/components/ChatBubble.qml
        qml/components/StatusDot.qml
        qml/components/SideNav.qml
    RESOURCES
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Bold.ttf
        resources/icons/neurx.png
)

target_include_directories(neurx_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(neurx_app PRIVATE
    neurx_core
    neurx_plugins
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Gui
)

# ------------- Android -------------
if(ANDROID)
    include(${CMAKE_SOURCE_DIR}/platform/android/android.cmake)
endif()

# ------------- iOS -----------------
if(IOS)
    include(${CMAKE_SOURCE_DIR}/platform/ios/ios.cmake)
endif()

# ------------- macOS ---------------
if(APPLE AND NOT IOS)
    set_target_properties(neurx_app PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "NeurX"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.neurx.app"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

# ------------- Windows -------------
if(WIN32)
    set_target_properties(neurx_app PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

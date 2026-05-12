cmake_minimum_required(VERSION 3.21)

qt_add_library(neurx_plugins STATIC)

target_sources(neurx_plugins PRIVATE
    PluginLoader.cpp
    # Built-in plugins
    builtin/WebSearchTool.cpp
    builtin/ReActAgent.cpp
)

target_include_directories(neurx_plugins
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(neurx_plugins
    PUBLIC  neurx_core
    PRIVATE Qt6::Core Qt6::Network
)

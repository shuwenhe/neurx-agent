cmake_minimum_required(VERSION 3.21)

qt_add_library(neurx_core STATIC)

target_sources(neurx_core PRIVATE
    # Agent subsystem
    agent/Agent.cpp
    agent/AgentRegistry.cpp
    agent/AgentRunner.cpp
    agent/AgentMessage.cpp
    agent/CodexAgent.cpp
    # Runtime
    runtime/AgentRuntime.cpp
    runtime/TaskScheduler.cpp
    runtime/EventBus.cpp
    runtime/WorkspaceContext.cpp
    runtime/ErrorRecovery.cpp
    # Tool system
    tools/ToolRegistry.cpp
    tools/BaseTool.cpp
    # LLM connector
    llm/LlmClient.cpp
    # Memory
    memory/MemoryStore.cpp
    # Logging
    log/Logger.cpp
)

target_include_directories(neurx_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(neurx_core
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        Qt6::Sql
)
if(Qt6WebSockets_FOUND)
    target_link_libraries(neurx_core PUBLIC Qt6::WebSockets)
    target_compile_definitions(neurx_core PUBLIC NEURX_WEBSOCKETS_ENABLED)
endif()

cmake_minimum_required(VERSION 3.21)

qt_add_library(neurx_core STATIC)

target_sources(neurx_core PRIVATE
    # Agent subsystem
    agent/Agent.cpp
    agent/AgentRegistry.cpp
    agent/AgentRunner.cpp
    agent/AgentMessage.cpp
    # Runtime
    runtime/AgentRuntime.cpp
    runtime/TaskScheduler.cpp
    runtime/EventBus.cpp
    # Tool system
    tools/ToolRegistry.cpp
    tools/BaseTool.cpp
    # LLM connector
    llm/LlmClient.cpp
    llm/LlmRequest.cpp
    llm/LlmResponse.cpp
    # Memory
    memory/MemoryStore.cpp
    memory/VectorIndex.cpp
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
        Qt6::WebSockets
        Qt6::Concurrent
        Qt6::Sql
)

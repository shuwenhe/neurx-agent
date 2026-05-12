# NeurX AgentOS

> A cross-platform **Agent Operating System** built with **Qt 6**
> Supports Windows · macOS · Linux · iOS · Android · HarmonyOS

---

## Architecture

```
neurx/
├── CMakeLists.txt          # Top-level build entry (Qt6 + CMake)
├── core/                   # Core framework (pure C++20, no UI dependency)
│   ├── agent/              # Agent base class, registry, messages, runner
│   ├── runtime/            # AgentRuntime, TaskScheduler, EventBus
│   ├── tools/              # BaseTool, ToolRegistry
│   ├── llm/                # LlmClient (OpenAI-compatible streaming)
│   ├── memory/             # MemoryStore (SQLite KV + full-text search)
│   └── log/                # Logger (file + Qt signal)
├── plugins/                # Built-in and dynamically loadable plugins
│   └── builtin/            # WebSearch / FileSystem / Shell / Http / ReActAgent
├── app/                    # Qt Quick application entry + QML UI
│   ├── main.cpp
│   ├── bridge/             # C++↔QML bridges (RuntimeBridge, AgentListModel, LogModel)
│   └── qml/
│       ├── Main.qml        # Window root
│       ├── AppShell.qml    # Side navigation + page stack
│       ├── components/     # AgentCard, ChatBubble, SideNav, StatusDot
│       └── pages/          # Dashboard, Agents, Chat, Tools, Settings
└── platform/               # Platform-specific configuration
    ├── android/            # AndroidManifest + CMake config
    ├── ios/                # Info.plist + CMake config
    └── harmony/            # OHOS toolchain notes + CMake config
```

---

## Core Concepts

| Concept | Description |
|---------|-------------|
| **Agent** | An autonomous entity that receives messages, calls LLMs, and uses tools |
| **AgentRegistry** | Global agent registry — handles creation, destruction, and message routing |
| **AgentRuntime** | Top-level runtime that starts and shuts down all subsystems |
| **TaskScheduler** | Priority task queue with async execution via `QtConcurrent` |
| **EventBus** | Publish/subscribe event bus for decoupled inter-module communication |
| **LlmClient** | OpenAI-compatible REST client with streaming support |
| **ToolRegistry** | Tool registry that auto-generates JSON Schema for LLM function-calling |
| **MemoryStore** | SQLite-backed persistent key-value store with full-text search |
| **ReActAgent** | Built-in ReAct agent (Reason → Act → Observe loop) |

---

## Getting Started

### Requirements

- Qt 6.5+ (Quick / QuickControls2 / Network / WebSockets / Sql / Concurrent)
- CMake 3.21+
- C++20 compiler (GCC 12 / Clang 15 / MSVC 2022)

### Desktop (macOS / Linux / Windows)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/app/neurx_app
```

### Android

```bash
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DQT_HOST_PATH=/path/to/qt-host \
  -DCMAKE_PREFIX_PATH=/path/to/qt-android
cmake --build build-android --target package
```

### iOS

Open the project via Qt Creator or `cmake -G Xcode`, then select the iOS target and build.

### HarmonyOS (OHOS)

```bash
cmake -B build-harmony \
  -DCMAKE_TOOLCHAIN_FILE=$OHOS_SDK/native/cmake/ohos.toolchain.cmake \
  -DHARMONY_SDK_ROOT=$OHOS_SDK \
  -DQT_HOST_PATH=/path/to/qt-host
cmake --build build-harmony
```

---

## Extending Agents & Tools

### Custom Agent

```cpp
#include "core/agent/Agent.h"

class MyAgent : public neurx::Agent {
    Q_OBJECT
public:
    MyAgent() : Agent("MyAgent") {}
    void receiveMessage(const QVariantMap &msg) override {
        // Handle message, call LLM, invoke tools …
    }
};

// Register
neurx::AgentRegistry::instance().registerAgent(new MyAgent);
```

### Custom Tool

```cpp
#include "core/tools/BaseTool.h"

class MyTool : public neurx::BaseTool {
    Q_OBJECT
public:
    MyTool() : BaseTool("my_tool", "Does something useful") {}
    QVariantMap execute(const QVariantMap &params) override {
        return {{"result", "Hello from MyTool"}};
    }
};

neurx::ToolRegistry::instance().registerTool(new MyTool);
```

---

## License

MIT

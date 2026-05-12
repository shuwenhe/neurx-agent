# NeurX AgentOS

> 基于 **Qt 6** 的跨平台 Agent 操作系统
> 支持 Windows · macOS · Linux · iOS · Android · HarmonyOS

---

## 架构总览

```
neurx/
├── CMakeLists.txt          # 顶层构建入口 (Qt6 + CMake)
├── core/                   # 核心框架（纯 C++20，无 UI 依赖）
│   ├── agent/              # Agent 基类、注册表、消息、Runner
│   ├── runtime/            # AgentRuntime、TaskScheduler、EventBus
│   ├── tools/              # BaseTool、ToolRegistry
│   ├── llm/                # LlmClient（OpenAI-compatible streaming）
│   ├── memory/             # MemoryStore（SQLite KV + 全文搜索）
│   └── log/                # Logger（文件 + 信号）
├── plugins/                # 内建与可动态加载的插件
│   └── builtin/            # WebSearch / FileSystem / Shell / Http / ReActAgent
├── app/                    # Qt Quick 应用入口 + QML UI
│   ├── main.cpp
│   ├── bridge/             # C++↔QML 桥接（RuntimeBridge、AgentListModel、LogModel）
│   └── qml/
│       ├── Main.qml        # 窗口根节点
│       ├── AppShell.qml    # 侧边导航 + 页面栈
│       ├── components/     # AgentCard、ChatBubble、SideNav、StatusDot
│       └── pages/          # Dashboard、Agents、Chat、Tools、Settings
└── platform/               # 各平台专属配置
    ├── android/            # AndroidManifest + CMake 配置
    ├── ios/                # Info.plist + CMake 配置
    └── harmony/            # OHOS toolchain 说明 + CMake 配置
```

---

## 核心概念

| 概念 | 说明 |
|------|------|
| **Agent** | 独立运行的智能体，可接收消息、调用 LLM、使用 Tool |
| **AgentRegistry** | 全局代理注册表，负责创建、销毁和消息路由 |
| **AgentRuntime** | 顶层运行时，统一启动/关闭所有子系统 |
| **TaskScheduler** | 优先级任务队列，通过 `QtConcurrent` 异步执行 |
| **EventBus** | 发布/订阅事件总线，解耦各模块 |
| **LlmClient** | OpenAI-compatible REST 客户端（支持流式输出） |
| **ToolRegistry** | 工具注册表，自动生成 JSON Schema 供 LLM function-calling 使用 |
| **MemoryStore** | SQLite 持久化键值存储 + 全文搜索 |
| **ReActAgent** | 内建 ReAct 范式代理（Reason → Act → Observe 循环） |

---

## 快速开始

### 依赖

- Qt 6.5+ （含 Quick / QuickControls2 / Network / WebSockets / Sql / Concurrent）
- CMake 3.21+
- C++20 编译器（GCC 12 / Clang 15 / MSVC 2022）

### 桌面构建（macOS / Linux / Windows）

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

在 Xcode 中通过 Qt Creator 或 `cmake -G Xcode` 打开后，
选择 iOS 目标直接编译。

### HarmonyOS (OHOS)

```bash
cmake -B build-harmony \
  -DCMAKE_TOOLCHAIN_FILE=$OHOS_SDK/native/cmake/ohos.toolchain.cmake \
  -DHARMONY_SDK_ROOT=$OHOS_SDK \
  -DQT_HOST_PATH=/path/to/qt-host
cmake --build build-harmony
```

---

## 扩展 Agent / Tool

### 自定义 Agent

```cpp
#include "core/agent/Agent.h"

class MyAgent : public neurx::Agent {
    Q_OBJECT
public:
    MyAgent() : Agent("MyAgent") {}
    void receiveMessage(const QVariantMap &msg) override {
        // 处理消息，调用 LLM，使用 Tool …
    }
};

// 注册
neurx::AgentRegistry::instance().registerAgent(new MyAgent);
```

### 自定义 Tool

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

## 许可证

MIT

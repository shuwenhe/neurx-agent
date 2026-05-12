#include "PluginLoader.h"
#include "builtin/WebSearchTool.h"
#include "builtin/FileSystemTool.h"
#include "builtin/ShellTool.h"
#include "builtin/HttpTool.h"
#include "builtin/ReActAgent.h"
#include "../core/tools/ToolRegistry.h"
#include "../core/agent/AgentRegistry.h"
#include <QDir>
#include <QPluginLoader>

namespace neurx {

PluginLoader::PluginLoader(QObject *parent) : QObject(parent) {}

void PluginLoader::loadDirectory(const QString &path)
{
    QDir dir(path);
    const auto entries = dir.entryInfoList(
        {"*.so", "*.dylib", "*.dll"}, QDir::Files);
    for (const auto &fi : entries) {
        QPluginLoader loader(fi.absoluteFilePath());
        if (loader.load()) {
            emit pluginLoaded(fi.baseName());
        } else {
            emit pluginFailed(fi.baseName(), loader.errorString());
        }
    }
}

void PluginLoader::registerBuiltins()
{
    auto &tools = ToolRegistry::instance();
    tools.registerTool(new WebSearchTool);
    tools.registerTool(new FileSystemTool);
    tools.registerTool(new ShellTool);
    tools.registerTool(new HttpTool);

    auto &agents = AgentRegistry::instance();
    agents.registerAgent(new ReActAgent("ReAct-1"));

    emit pluginLoaded("builtin");
}

} // namespace neurx

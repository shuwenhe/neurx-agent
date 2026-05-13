#include "PluginLoader.h"
#include "builtin/WebSearchTool.h"
#include "builtin/CodexWorkspaceTool.h"
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
    tools.registerTool(new CodexReadFileTool);
    tools.registerTool(new CodexSearchWorkspaceTool);
    tools.registerTool(new CodexRunCommandTool);
    tools.registerTool(new CodexProposePatchTool);
    tools.registerTool(new CodexCreateFileTool);
    tools.registerTool(new CodexEditFileTool);
    tools.registerTool(new CodexFindSymbolTool);
    tools.registerTool(new CodexFindReferencesTool);

    auto &agents = AgentRegistry::instance();
    agents.registerAgent(new ReActAgent("ReAct-1"));

    emit pluginLoaded("builtin");
}

} // namespace neurx

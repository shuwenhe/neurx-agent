#pragma once
#include <QObject>
#include <QString>

namespace neurx {

/// Discovers and loads plugin shared libraries from a directory.
class PluginLoader : public QObject
{
    Q_OBJECT
public:
    explicit PluginLoader(QObject *parent = nullptr);

    /// Load all plugins from the given directory.
    void loadDirectory(const QString &path);

    /// Register all built-in (compiled-in) plugins.
    void registerBuiltins();

signals:
    void pluginLoaded(const QString &name);
    void pluginFailed(const QString &name, const QString &reason);
};

} // namespace neurx

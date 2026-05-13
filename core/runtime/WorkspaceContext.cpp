#include "WorkspaceContext.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace neurx {

WorkspaceContext::WorkspaceContext(const QString &workspaceRoot, QObject *parent)
    : QObject(parent)
    , m_workspaceRoot(QFileInfo(workspaceRoot).canonicalFilePath())
{
    // Infer project name from directory
    QFileInfo info(m_workspaceRoot);
    m_projectName = info.fileName();
    
    // Build initial project index
    buildProjectIndex();
}

void WorkspaceContext::setCurrentFile(const QString &filePath, const QString &content)
{
    if (m_currentFilePath != filePath) {
        m_currentFilePath = filePath;
        emit currentFileChanged(filePath);
    }
    m_currentFileContent = content;
    
    // Cache the content
    if (m_fileContentCache.size() > 50)  // Simple LRU: evict when > 50 files
        m_fileContentCache.clear();
    m_fileContentCache[filePath] = content;
}

void WorkspaceContext::addOpenTab(const QString &filePath)
{
    if (!m_openTabs.contains(filePath)) {
        m_openTabs.append(filePath);
        emit openTabsChanged();
    }
}

void WorkspaceContext::removeOpenTab(const QString &filePath)
{
    if (m_openTabs.removeOne(filePath))
        emit openTabsChanged();
}

void WorkspaceContext::buildProjectIndex()
{
    cacheProjectFiles();
}

void WorkspaceContext::cacheProjectFiles()
{
    m_projectFiles.clear();
    
    // Include common code file extensions
    static const QStringList extensions = {
        QStringLiteral("*.cpp"),
        QStringLiteral("*.h"),
        QStringLiteral("*.qml"),
        QStringLiteral("*.py"),
        QStringLiteral("*.js"),
        QStringLiteral("*.ts"),
        QStringLiteral("*.hpp"),
        QStringLiteral("*.c"),
        QStringLiteral("*.cc"),
        QStringLiteral("*.cxx")
    };
    
    QDir dir(m_workspaceRoot);
    dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    // Recursively find files
    std::function<void(const QString &)> walk = [&](const QString &path) {
        QDir currentDir(path);
        currentDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        
        // Skip common ignore patterns
        const QString dirName = currentDir.dirName();
        if (dirName == QLatin1String("build") 
            || dirName == QLatin1String(".git") 
            || dirName == QLatin1String("node_modules")
            || dirName == QLatin1String(".vscode")
            || dirName == QLatin1String("__pycache__")
            || dirName == QLatin1String("dist")
            || dirName == QLatin1String("CMakeFiles")) {
            return;
        }
        
        const auto entries = currentDir.entryList();
        for (const QString &entry : entries) {
            const QString fullPath = currentDir.absoluteFilePath(entry);
            const QFileInfo info(fullPath);
            
            if (info.isDir()) {
                walk(fullPath);
            } else if (info.isFile()) {
                // Check if file matches one of our extensions
                for (const QString &ext : extensions) {
                    if (QDir::match(ext, info.fileName())) {
                        m_projectFiles.append(fullPath);
                        break;
                    }
                }
            }
        }
    };
    
    walk(m_workspaceRoot);
}

QStringList WorkspaceContext::projectFiles(const QString &extension) const
{
    if (extension.isEmpty())
        return m_projectFiles;
    
    QStringList filtered;
    for (const QString &file : m_projectFiles) {
        if (file.endsWith(extension))
            filtered.append(file);
    }
    return filtered;
}

int WorkspaceContext::projectFileCount() const
{
    return m_projectFiles.size();
}

QString WorkspaceContext::getFileContent(const QString &filePath) const
{
    // Check cache first
    if (m_fileContentCache.contains(filePath))
        return m_fileContentCache.value(filePath);
    
    // Read from disk
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.read(256 * 1024));  // Max 256KB
    }
    return {};
}

QString WorkspaceContext::contextSummary() const
{
    const int cppCount = projectFiles(QStringLiteral(".cpp")).count();
    const int headerCount = projectFiles(QStringLiteral(".h")).count();
    const int qmlCount = projectFiles(QStringLiteral(".qml")).count();
    
    return QString(
        "Project: %1\n"
        "Root: %2\n"
        "Files: %3 total (%4 .cpp, %5 .h, %6 .qml)\n"
        "Current file: %7\n"
        "Open tabs: %8")
        .arg(m_projectName,
             m_workspaceRoot,
             QString::number(m_projectFiles.size()),
             QString::number(cppCount),
             QString::number(headerCount),
             QString::number(qmlCount),
             m_currentFilePath.isEmpty() ? QStringLiteral("(none)") : m_currentFilePath,
             QString::number(m_openTabs.size()));
}

QVariantMap WorkspaceContext::contextMap() const
{
    return {
        {QStringLiteral("projectName"), m_projectName},
        {QStringLiteral("workspaceRoot"), m_workspaceRoot},
        {QStringLiteral("fileCount"), m_projectFiles.size()},
        {QStringLiteral("cppFileCount"), projectFiles(QStringLiteral(".cpp")).count()},
        {QStringLiteral("headerFileCount"), projectFiles(QStringLiteral(".h")).count()},
        {QStringLiteral("qmlFileCount"), projectFiles(QStringLiteral(".qml")).count()},
        {QStringLiteral("currentFile"), m_currentFilePath},
        {QStringLiteral("openTabCount"), m_openTabs.size()}
    };
}

} // namespace neurx

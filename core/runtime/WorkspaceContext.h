#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QHash>

namespace neurx {

/// Represents metadata about the workspace and current editing session.
class WorkspaceContext : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceContext(const QString &workspaceRoot, QObject *parent = nullptr);

    // Basic properties
    QString workspaceRoot() const { return m_workspaceRoot; }
    QString projectName() const { return m_projectName; }
    
    // File management
    void setCurrentFile(const QString &filePath, const QString &content);
    QString currentFilePath() const { return m_currentFilePath; }
    QString currentFileContent() const { return m_currentFileContent; }
    
    void addOpenTab(const QString &filePath);
    void removeOpenTab(const QString &filePath);
    QStringList openTabs() const { return m_openTabs; }
    
    // Project indexing (lightweight, on-demand)
    QStringList projectFiles(const QString &extension = {}) const;
    int projectFileCount() const;
    QString getFileContent(const QString &filePath) const;
    
    // Context summary for LLM
    QString contextSummary() const;
    QVariantMap contextMap() const;

signals:
    void currentFileChanged(const QString &filePath);
    void openTabsChanged();

private:
    void buildProjectIndex();
    void cacheProjectFiles();
    
    QString     m_workspaceRoot;
    QString     m_projectName;
    QString     m_currentFilePath;
    QString     m_currentFileContent;
    QStringList m_openTabs;
    QStringList m_projectFiles;  // Cached list of all .cpp, .h, .qml, etc.
    QHash<QString, QString> m_fileContentCache;  // LRU cache of file contents
};

} // namespace neurx

#include "RuntimeBridge.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QStringList>
#include <QVariantMap>

namespace neurx {

namespace {
constexpr auto kLastOpenedEditorFileKey = "editor/lastOpenedFile";
}

RuntimeBridge::RuntimeBridge(AgentRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime)
{
    connect(m_runtime, &AgentRuntime::runningChanged,
            this, &RuntimeBridge::runningChanged);
    connect(&m_runtime->registry(), &AgentRegistry::countChanged,
            this, &RuntimeBridge::agentCountChanged);

    m_codexAgent = new CodexAgent;
    m_codexAgent->setWorkspaceRoot(QStringLiteral("/Users/feifei/neurx"));
    m_runtime->registry().registerAgent(m_codexAgent);

    connect(m_codexAgent, &CodexAgent::chunkReceived,
            this, &RuntimeBridge::chatChunkReceived);
    connect(m_codexAgent, &CodexAgent::responseFinished,
            this, &RuntimeBridge::chatStreamFinished);
    connect(m_codexAgent, &CodexAgent::errorOccurred,
            this, &RuntimeBridge::chatErrorOccurred);

    refreshLocalModels();
}

bool RuntimeBridge::isRunning() const { return m_runtime->isRunning(); }

int RuntimeBridge::agentCount() const
{
    return m_runtime->registry().count();
}

QVariantList RuntimeBridge::localModels() const
{
    return m_localModels;
}

QString RuntimeBridge::localModelStatus() const
{
    return m_localModelStatus;
}

QString RuntimeBridge::localModelSource() const
{
    return m_localModelSource;
}

void RuntimeBridge::startRuntime()    { m_runtime->start();    }
void RuntimeBridge::shutdownRuntime() { m_runtime->shutdown(); }

QString RuntimeBridge::chatApiKey() const { return m_chatApiKey; }
void RuntimeBridge::setChatApiKey(const QString &key)
{
    if (m_chatApiKey == key) return;
    m_chatApiKey = key;
    emit chatApiKeyChanged();
}

QString RuntimeBridge::chatEndpoint() const { return m_chatEndpoint; }
void RuntimeBridge::setChatEndpoint(const QString &ep)
{
    if (m_chatEndpoint == ep) return;
    m_chatEndpoint = ep;
    emit chatEndpointChanged();
}

QString RuntimeBridge::resolveEndpointForModel(const QString &modelId) const
{
    // Ollama model IDs use the form "name:tag" (e.g. "qwen3:8b")
    if (modelId.contains(QLatin1Char(':')))
        return QStringLiteral("http://localhost:11434/v1/chat/completions");
    return m_chatEndpoint.isEmpty()
        ? QStringLiteral("https://api.openai.com/v1/chat/completions")
        : m_chatEndpoint;
}

LlmConfig RuntimeBridge::buildLlmConfig(const QString &modelId) const
{
    LlmConfig cfg;
    cfg.endpoint    = resolveEndpointForModel(modelId);
    cfg.apiKey      = modelId.contains(QLatin1Char(':'))
                      ? QStringLiteral("ollama") : m_chatApiKey;
    cfg.model       = modelId;
    cfg.temperature = 0.7;
    cfg.maxTokens   = 4096;
    return cfg;
}

void RuntimeBridge::sendChatMessage(const QString &modelId,
                                    const QString &text,
                                    const QString &currentFilePath,
                                    const QString &currentFileContent,
                                    const QString &effort,
                                    const QString &permissions,
                                    bool ideContext)
{
    if (!m_codexAgent) {
        emit chatErrorOccurred(QStringLiteral("Codex agent is not available."));
        return;
    }

    QVariantMap options{
        {QStringLiteral("effort"), effort},
        {QStringLiteral("permissions"), permissions},
        {QStringLiteral("ideContext"), ideContext}
    };

    m_codexAgent->setConfig(buildLlmConfig(modelId));
    m_codexAgent->setCurrentFile(currentFilePath, currentFileContent);
    m_codexAgent->submitPrompt(text, options);
}

void RuntimeBridge::clearChatContext()
{
    if (m_codexAgent)
        m_codexAgent->clearContext();
}

QString RuntimeBridge::readLocalFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists())
        return QStringLiteral("Unable to open file: not found");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("Unable to open file: %1").arg(file.errorString());

    static constexpr qint64 kMaxPreviewBytes = 512 * 1024;
    QByteArray bytes = file.read(kMaxPreviewBytes + 1);
    bool truncated = bytes.size() > kMaxPreviewBytes;
    if (truncated)
        bytes.truncate(kMaxPreviewBytes);

    QString content = QString::fromUtf8(bytes);
    if (truncated)
        content.append(QStringLiteral("\n\n... [preview truncated]") );
    return content;
}

QString RuntimeBridge::lastOpenedEditorFile() const
{
    const QString filePath = QSettings().value(QLatin1String(kLastOpenedEditorFileKey)).toString();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return {};

    return filePath;
}

void RuntimeBridge::rememberOpenedEditorFile(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    QSettings().setValue(QLatin1String(kLastOpenedEditorFileKey), filePath);
}

void RuntimeBridge::copyToClipboard(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void RuntimeBridge::revealInFinder(const QString &filePath)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return;

#if defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), filePath});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
#endif
}

void RuntimeBridge::refreshLocalModels()
{
    const QString executable = resolveOllamaExecutable();
    const QVariantList detected = detectOllamaModels();
    QString nextStatus;
    if (executable.isEmpty())
        nextStatus = QStringLiteral("Ollama not found");
    else if (detected.isEmpty())
        nextStatus = QStringLiteral("No local models found");
    else
        nextStatus = QStringLiteral("%1 local models ready").arg(detected.size());

    if (detected == m_localModels
        && nextStatus == m_localModelStatus
        && executable == m_localModelSource) {
        return;
    }

    m_localModels = detected;
    m_localModelStatus = nextStatus;
    m_localModelSource = executable;
    emit localModelsChanged();
}

QString RuntimeBridge::resolveOllamaExecutable() const
{
    QString ollamaExecutable = QStandardPaths::findExecutable(QStringLiteral("ollama"));
    if (ollamaExecutable.isEmpty()) {
        const QStringList fallbackPaths = {
            QStringLiteral("/opt/homebrew/bin/ollama"),
            QStringLiteral("/usr/local/bin/ollama")
        };

        for (const QString &candidate : fallbackPaths) {
            if (QFileInfo::exists(candidate)) {
                ollamaExecutable = candidate;
                break;
            }
        }
    }

    return ollamaExecutable;
}

QVariantList RuntimeBridge::detectOllamaModels() const
{
    const QString ollamaExecutable = resolveOllamaExecutable();

    if (ollamaExecutable.isEmpty())
        return {};

    QProcess process;
    process.start(ollamaExecutable, {QStringLiteral("list")});

    if (!process.waitForStarted(1000))
        return {};

    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(500);
        return {};
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    QVariantList models;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("NAME")))
            continue;

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        const QString name = parts.first();
        models.append(QVariantMap{
            {QStringLiteral("idValue"), name},
            {QStringLiteral("label"), name},
            {QStringLiteral("provider"), QStringLiteral("Ollama")},
            {QStringLiteral("detail"), QStringLiteral("Local model installed")}
        });
    }

    return models;
}

} // namespace neurx

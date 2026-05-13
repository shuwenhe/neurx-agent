#include "RuntimeBridge.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

namespace neurx {

RuntimeBridge::RuntimeBridge(AgentRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime)
{
    connect(m_runtime, &AgentRuntime::runningChanged,
            this, &RuntimeBridge::runningChanged);
    connect(&m_runtime->registry(), &AgentRegistry::countChanged,
            this, &RuntimeBridge::agentCountChanged);

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

void RuntimeBridge::sendChatMessage(const QString &modelId, const QString &text)
{
    LlmConfig cfg;
    cfg.endpoint    = resolveEndpointForModel(modelId);
    cfg.apiKey      = modelId.contains(QLatin1Char(':'))
                      ? QStringLiteral("ollama") : m_chatApiKey;
    cfg.model       = modelId;
    cfg.temperature = 0.7;
    cfg.maxTokens   = 4096;

    if (!m_chatClient) {
        m_chatClient = new LlmClient(cfg, this);
        connect(m_chatClient, &LlmClient::chunkReceived,
                this, &RuntimeBridge::chatChunkReceived);
        connect(m_chatClient, &LlmClient::streamFinished,
                this, [this](const QString &content) {
            m_chatContext.append({QStringLiteral("assistant"), content});
            emit chatStreamFinished(content);
        });
        connect(m_chatClient, &LlmClient::errorOccurred,
                this, &RuntimeBridge::chatErrorOccurred);
    } else {
        m_chatClient->setConfig(cfg);
    }

    m_chatContext.append({QStringLiteral("user"), text});
    m_chatClient->chatStream(m_chatContext);
}

void RuntimeBridge::clearChatContext()
{
    m_chatContext.clear();
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

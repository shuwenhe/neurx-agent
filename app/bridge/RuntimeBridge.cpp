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

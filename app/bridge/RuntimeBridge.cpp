#include "RuntimeBridge.h"
#include "PluginLoader.h"
#include "tools/ToolRegistry.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QStringList>
#include <QVariantMap>

namespace neurx {

namespace {
constexpr auto kLastOpenedEditorFileKey = "editor/lastOpenedFile";
constexpr auto kAddressHistoryKey = "editor/addressHistory";
constexpr auto kPinnedAddressHistoryKey = "editor/addressPinnedHistory";
constexpr auto kWorkspaceRoot = "/Users/feifei/neurx";
constexpr int kMaxAddressHistoryEntries = 20;

QString workspaceRoot()
{
    return QFileInfo(QString::fromUtf8(kWorkspaceRoot)).canonicalFilePath();
}

QString resolveWorkspacePath(const QString &path)
{
    const QString root = workspaceRoot();
    if (root.isEmpty())
        return {};

    const QString candidate = QDir::isAbsolutePath(path)
        ? path
        : QDir(root).absoluteFilePath(path);

    const QFileInfo info(candidate);
    const QString canonical = info.exists()
        ? info.canonicalFilePath()
        : QFileInfo(info.absolutePath()).canonicalFilePath()
              + QLatin1Char('/') + info.fileName();

    if (canonical == root || canonical.startsWith(root + QLatin1Char('/')))
        return canonical;

    return {};
}

QString stripDiffPathPrefix(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"')))
        path = path.mid(1, path.size() - 2);
    if (path == QLatin1String("/dev/null"))
        return {};
    if (path.startsWith(QStringLiteral("a/")) || path.startsWith(QStringLiteral("b/")))
        path = path.mid(2);
    return path;
}

bool patchTargetsOnlyPath(const QString &patch, const QString &targetPath, QString *error)
{
    const QString root = workspaceRoot();
    const QString target = resolveWorkspacePath(targetPath);
    if (root.isEmpty() || target.isEmpty()) {
        if (error)
            *error = QStringLiteral("Patch target must stay inside workspace.");
        return false;
    }

    bool foundPath = false;
    const QStringList lines = patch.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        QStringList paths;
        if (line.startsWith(QStringLiteral("diff --git "))) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 4)
                paths << stripDiffPathPrefix(parts.at(2)) << stripDiffPathPrefix(parts.at(3));
        } else if (line.startsWith(QStringLiteral("--- "))
                   || line.startsWith(QStringLiteral("+++ "))) {
            paths << stripDiffPathPrefix(line.mid(4).section(QLatin1Char('\t'), 0, 0)
                                             .section(QLatin1Char(' '), 0, 0));
        }

        for (const QString &path : paths) {
            if (path.isEmpty())
                continue;
            if (QDir::isAbsolutePath(path) || path.contains(QStringLiteral(".."))) {
                if (error)
                    *error = QStringLiteral("Patch contains an unsafe file path: %1").arg(path);
                return false;
            }

            const QString resolved = QFileInfo(QDir(root).absoluteFilePath(path)).absoluteFilePath();
            if (resolved != target) {
                if (error)
                    *error = QStringLiteral("Patch is only approved for %1. Found %2.")
                                 .arg(target, resolved);
                return false;
            }
            foundPath = true;
        }
    }

    if (!foundPath && error)
        *error = QStringLiteral("Patch must be a unified diff with file headers.");
    return foundPath;
}

QVariantMap runGitApply(const QStringList &arguments, const QString &patch)
{
    QProcess process;
    process.setWorkingDirectory(workspaceRoot());
    process.start(QStringLiteral("git"), arguments);
    if (!process.waitForStarted(1000))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Unable to start git apply.")}};

    process.write(patch.toUtf8());
    process.closeWriteChannel();

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(500);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("git apply timed out.")}};
    }

    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    const bool ok = process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
    return {{QStringLiteral("ok"), ok},
            {QStringLiteral("stdout"), stdoutText},
            {QStringLiteral("stderr"), stderrText},
            {QStringLiteral("error"), ok ? QString() : stderrText.trimmed()}};
}
}

RuntimeBridge::RuntimeBridge(AgentRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime)
{
    static bool s_builtinPluginsRegistered = false;
    if (!s_builtinPluginsRegistered) {
        PluginLoader loader;
        loader.registerBuiltins();
        s_builtinPluginsRegistered = true;
    }

    connect(m_runtime, &AgentRuntime::runningChanged,
            this, &RuntimeBridge::runningChanged);
    connect(&m_runtime->registry(), &AgentRegistry::countChanged,
            this, &RuntimeBridge::agentCountChanged);

    // Restore persisted LLM settings
    QSettings settings;
    m_chatApiKey  = settings.value(QStringLiteral("llm/apiKey")).toString();
    m_chatEndpoint = settings.value(QStringLiteral("llm/endpoint")).toString();
    m_chatModel   = settings.value(QStringLiteral("llm/model"), QStringLiteral("qwen2:0.5b")).toString();

    m_codexAgent = new CodexAgent;
    m_codexAgent->setWorkspaceRoot(QString::fromUtf8(kWorkspaceRoot));
    m_runtime->registry().registerAgent(m_codexAgent);

    connect(m_codexAgent, &CodexAgent::chunkReceived,
            this, &RuntimeBridge::chatChunkReceived);
    connect(m_codexAgent, &CodexAgent::responseFinished,
            this, &RuntimeBridge::chatStreamFinished);
    connect(m_codexAgent, &CodexAgent::errorOccurred,
            this, &RuntimeBridge::chatErrorOccurred);
    connect(m_codexAgent, &CodexAgent::workspaceFileChanged,
            this, &RuntimeBridge::fileChanged);
    connect(m_codexAgent, &CodexAgent::toolExecutionStarted,
            this, &RuntimeBridge::toolExecutionStarted);
    connect(m_codexAgent, &CodexAgent::toolExecutionFinished,
            this, &RuntimeBridge::toolExecutionFinished);
    connect(m_codexAgent, &CodexAgent::operationSummaryReady,
            this, &RuntimeBridge::operationSummaryReady);
    connect(m_codexAgent, &CodexAgent::turnCancelled,
            this, &RuntimeBridge::chatCancelled);

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
    QSettings().setValue(QStringLiteral("llm/apiKey"), key);
    emit chatApiKeyChanged();
}

QString RuntimeBridge::chatEndpoint() const { return m_chatEndpoint; }
void RuntimeBridge::setChatEndpoint(const QString &ep)
{
    if (m_chatEndpoint == ep) return;
    m_chatEndpoint = ep;
    QSettings().setValue(QStringLiteral("llm/endpoint"), ep);
    emit chatEndpointChanged();
}

QString RuntimeBridge::chatModel() const { return m_chatModel; }
void RuntimeBridge::setChatModel(const QString &model)
{
    if (m_chatModel == model) return;
    m_chatModel = model;
    QSettings().setValue(QStringLiteral("llm/model"), model);
    emit chatModelChanged();
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
    cfg.maxTokens   = 1024;
    return cfg;
}

void RuntimeBridge::sendChatMessage(const QString &modelId,
                                    const QString &text,
                                    const QString &currentFilePath,
                                    const QString &currentFileContent,
                                    const QString &effort,
                                    const QString &permissions,
                                    bool ideContext,
                                    bool workLocally)
{
    if (!m_codexAgent) {
        emit chatErrorOccurred(QStringLiteral("Codex agent is not available."));
        return;
    }

    QVariantMap options{
        {QStringLiteral("effort"), effort},
        {QStringLiteral("permissions"), permissions},
        {QStringLiteral("ideContext"), ideContext},
        {QStringLiteral("workLocally"), workLocally}
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

void RuntimeBridge::cancelChat()
{
    if (m_codexAgent)
        m_codexAgent->cancelTurn();
}

QVariantList RuntimeBridge::listWorkspaceFiles(const QString &filter)
{
    // Recursively scan workspace for source files, skip build/.git
    QVariantList result;
    const QString root = QString::fromLatin1(kWorkspaceRoot);
    QStringList queue = {root};
    const QStringList skipDirs = {"build", ".git", "node_modules", ".cache"};
    const QStringList extensions = {"cpp", "h", "qml", "cmake", "txt", "md", "json", "ts", "js", "py"};
    QString filterLower = filter.toLower();

    while (!queue.isEmpty()) {
        const QString dir = queue.takeFirst();
        const QDir d(dir);
        const QString relDir = d.relativeFilePath(dir);

        // Recurse into subdirs (skip blacklist)
        for (const auto &sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!skipDirs.contains(sub))
                queue.append(dir + "/" + sub);
        }

        // Collect matching files
        for (const auto &entry : d.entryList(QDir::Files)) {
            const QString ext = QFileInfo(entry).suffix().toLower();
            if (!extensions.contains(ext))
                continue;
            if (!filterLower.isEmpty() && !entry.toLower().contains(filterLower))
                continue;
            const QString fullPath = dir + "/" + entry;
            const QString relPath = QString::fromLatin1(kWorkspaceRoot) + "/";
            const QString display = fullPath.mid(relPath.length() - 1); // strip root prefix to get relative
            result.append(QVariantMap{
                {"name", entry},
                {"path", fullPath},
                {"rel", fullPath.mid(QString::fromLatin1(kWorkspaceRoot).length() + 1)}
            });
            if (result.size() >= 300)
                return result;
        }
    }
    return result;
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

QString RuntimeBridge::readRemoteUrl(const QString &url)
{
    const QUrl target(url);
    if (!target.isValid() || !target.scheme().startsWith(QLatin1String("http")))
        return QStringLiteral("Unable to open URL: invalid http/https address");

    QNetworkAccessManager manager;
    QNetworkRequest request(target);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "NeurX/1.0");

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(7000);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        reply->deleteLater();
        return QStringLiteral("Unable to open URL: %1").arg(err);
    }

    static constexpr qint64 kMaxPreviewBytes = 512 * 1024;
    QByteArray body = reply->read(kMaxPreviewBytes + 1);
    const bool truncated = body.size() > kMaxPreviewBytes;
    if (truncated)
        body.truncate(kMaxPreviewBytes);

    reply->deleteLater();
    QString content = QString::fromUtf8(body);
    if (truncated)
        content.append(QStringLiteral("\n\n... [preview truncated]"));
    return content;
}

QString RuntimeBridge::lastOpenedEditorFile() const
{
    const QString filePath = QSettings().value(QLatin1String(kLastOpenedEditorFileKey)).toString();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return {};

    return filePath;
}

QVariantList RuntimeBridge::addressHistory() const
{
    const QStringList entries = QSettings().value(QLatin1String(kAddressHistoryKey)).toStringList();
    QVariantList out;
    for (const QString &entry : entries)
        out.append(entry);
    return out;
}

void RuntimeBridge::setAddressHistory(const QVariantList &entries)
{
    QStringList normalized;
    normalized.reserve(entries.size());
    for (const QVariant &entry : entries) {
        const QString value = entry.toString().trimmed();
        if (value.isEmpty())
            continue;
        if (!normalized.contains(value))
            normalized.append(value);
        if (normalized.size() >= kMaxAddressHistoryEntries)
            break;
    }
    QSettings().setValue(QLatin1String(kAddressHistoryKey), normalized);
}

void RuntimeBridge::clearAddressHistory()
{
    QSettings().remove(QLatin1String(kAddressHistoryKey));
}

QVariantList RuntimeBridge::pinnedAddressHistory() const
{
    const QStringList entries = QSettings().value(QLatin1String(kPinnedAddressHistoryKey)).toStringList();
    QVariantList out;
    for (const QString &entry : entries)
        out.append(entry);
    return out;
}

void RuntimeBridge::setPinnedAddressHistory(const QVariantList &entries)
{
    QStringList normalized;
    normalized.reserve(entries.size());
    for (const QVariant &entry : entries) {
        const QString value = entry.toString().trimmed();
        if (value.isEmpty())
            continue;
        if (!normalized.contains(value))
            normalized.append(value);
        if (normalized.size() >= kMaxAddressHistoryEntries)
            break;
    }
    QSettings().setValue(QLatin1String(kPinnedAddressHistoryKey), normalized);
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

QVariantMap RuntimeBridge::applyApprovedPatch(const QString &filePath, const QString &patch)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("filePath is required")}
        };
    }

    const QString trimmedPatch = patch.trimmed();
    if (trimmedPatch.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("patch is empty")}
        };
    }

    const QString resolvedTarget = resolveWorkspacePath(trimmedPath);
    if (resolvedTarget.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("filePath must stay inside workspace")},
            {QStringLiteral("filePath"), trimmedPath}
        };
    }

    QString validationError;
    if (!patchTargetsOnlyPath(trimmedPatch, resolvedTarget, &validationError)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), validationError},
            {QStringLiteral("filePath"), resolvedTarget}
        };
    }

    const QVariantMap checkResult = runGitApply(
        {QStringLiteral("apply"), QStringLiteral("--check"), QStringLiteral("--recount")},
        trimmedPatch);
    if (!checkResult.value(QStringLiteral("ok")).toBool()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), checkResult.value(QStringLiteral("error")).toString()},
            {QStringLiteral("stderr"), checkResult.value(QStringLiteral("stderr")).toString()},
            {QStringLiteral("filePath"), resolvedTarget}
        };
    }

    const QVariantMap applyResult = runGitApply(
        {QStringLiteral("apply"), QStringLiteral("--recount")},
        trimmedPatch);
    if (!applyResult.value(QStringLiteral("ok")).toBool()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), applyResult.value(QStringLiteral("error")).toString()},
            {QStringLiteral("stderr"), applyResult.value(QStringLiteral("stderr")).toString()},
            {QStringLiteral("filePath"), resolvedTarget}
        };
    }

    emit fileChanged(resolvedTarget);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("filePath"), resolvedTarget},
        {QStringLiteral("stdout"), applyResult.value(QStringLiteral("stdout")).toString()},
        {QStringLiteral("stderr"), applyResult.value(QStringLiteral("stderr")).toString()}
    };
}

QVariantMap RuntimeBridge::runApprovedCommand(const QString &command, const QString &reason)
{
    const QString trimmedCommand = command.trimmed();
    if (trimmedCommand.isEmpty()) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("command is required")}
        };
    }

    auto *tool = ToolRegistry::instance().findTool(QStringLiteral("codex_run_command"));
    if (!tool) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("codex_run_command is not available")}
        };
    }

    const QVariantMap result = tool->execute({
        {QStringLiteral("command"), trimmedCommand},
        {QStringLiteral("reason"), reason}
    });

    QVariantMap out = result;
    const int exitCode = result.value(QStringLiteral("exitCode"), -1).toInt();
    const bool ok = !result.contains(QStringLiteral("error")) && exitCode == 0;
    out.insert(QStringLiteral("ok"), ok);
    if (!ok && !out.contains(QStringLiteral("error"))) {
        out.insert(QStringLiteral("error"),
                   QStringLiteral("command exited with code %1")
                       .arg(exitCode));
    }
    return out;
}

bool RuntimeBridge::isToolAutoApprovable(const QString &toolName, const QVariantMap &args)
{
    if (!m_codexAgent)
        return false;
    return m_codexAgent->isToolCallAutoApprovable(toolName, args);
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

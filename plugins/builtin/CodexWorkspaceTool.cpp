#include "CodexWorkspaceTool.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace neurx {

namespace {
constexpr auto kWorkspaceRoot = "/Users/feifei/neurx";
constexpr qint64 kMaxReadBytes = 256 * 1024;
constexpr int kMaxSearchBytes = 64 * 1024;
constexpr int kMaxCommandBytes = 96 * 1024;

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

    QFileInfo info(candidate);
    const QString canonical = info.exists()
        ? info.canonicalFilePath()
        : QFileInfo(info.absolutePath()).canonicalFilePath()
              + QLatin1Char('/') + info.fileName();

    if (canonical == root || canonical.startsWith(root + QLatin1Char('/')))
        return canonical;

    return {};
}

QVariantMap stringProperty(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), description}};
}

bool hasShellControlToken(const QString &command)
{
    static const QStringList tokens = {
        QStringLiteral(";"),
        QStringLiteral("&&"),
        QStringLiteral("||"),
        QStringLiteral("|"),
        QStringLiteral(">"),
        QStringLiteral("<"),
        QStringLiteral("$("),
        QStringLiteral("`")
    };

    for (const QString &token : tokens) {
        if (command.contains(token))
            return true;
    }
    return false;
}

bool isAllowedCommand(const QString &program, const QStringList &args)
{
    const QString exe = QFileInfo(program).fileName();
    if (exe == QLatin1String("pwd") || exe == QLatin1String("ls")
        || exe == QLatin1String("cat") || exe == QLatin1String("head")
        || exe == QLatin1String("tail") || exe == QLatin1String("wc")
        || exe == QLatin1String("rg")) {
        return true;
    }

    if (exe == QLatin1String("git")) {
        if (args.isEmpty())
            return false;
        const QString subcommand = args.first();
        return subcommand == QLatin1String("status")
            || subcommand == QLatin1String("diff")
            || subcommand == QLatin1String("log")
            || subcommand == QLatin1String("show");
    }

    if (exe == QLatin1String("cmake"))
        return args.contains(QStringLiteral("--build"));
    if (exe == QLatin1String("make") || exe == QLatin1String("ctest"))
        return true;

    return false;
}

QString escapeRegexLiteral(const QString &value)
{
    return QRegularExpression::escape(value);
}

QVariantMap runRgSearch(const QString &root,
                        const QString &pattern,
                        const QString &searchPath,
                        int maxCount,
                        int timeoutMs)
{
    auto runGrepFallback = [&](bool fixedString) -> QVariantMap {
        QProcess grepProcess;
        grepProcess.setWorkingDirectory(root);
        QStringList grepArgs{
            QStringLiteral("-R"),
            QStringLiteral("-n"),
            QStringLiteral("--exclude-dir=.git"),
            QStringLiteral("--exclude-dir=build")
        };
        grepArgs << (fixedString ? QStringLiteral("-F") : QStringLiteral("-E"));
        grepArgs << pattern << searchPath;
        grepProcess.start(QStringLiteral("grep"), grepArgs);
        if (!grepProcess.waitForStarted(1000))
            return {{QStringLiteral("error"), QStringLiteral("search backend unavailable (rg/grep)")}};
        if (!grepProcess.waitForFinished(timeoutMs)) {
            grepProcess.kill();
            grepProcess.waitForFinished(500);
            return {{QStringLiteral("error"), QStringLiteral("search timed out")}};
        }
        QByteArray output = grepProcess.readAllStandardOutput();
        const bool truncated = output.size() > kMaxSearchBytes;
        if (truncated)
            output.truncate(kMaxSearchBytes);
        return {{QStringLiteral("results"), QString::fromUtf8(output)},
                {QStringLiteral("exitCode"), grepProcess.exitCode()},
                {QStringLiteral("truncated"), truncated},
                {QStringLiteral("backend"), QStringLiteral("grep")}};
    };

    QProcess process;
    process.setWorkingDirectory(root);
    process.start(QStringLiteral("rg"), {
        QStringLiteral("--line-number"),
        QStringLiteral("--column"),
        QStringLiteral("--max-count"), QString::number(maxCount),
        QStringLiteral("--pcre2"),
        QStringLiteral("--no-heading"),
        pattern,
        searchPath
    });

    if (!process.waitForStarted(1000))
        return runGrepFallback(false);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(500);
        return {{QStringLiteral("error"), QStringLiteral("search timed out")}};
    }

    QByteArray output = process.readAllStandardOutput();
    const bool truncated = output.size() > kMaxSearchBytes;
    if (truncated)
        output.truncate(kMaxSearchBytes);

    return {{QStringLiteral("results"), QString::fromUtf8(output)},
            {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("truncated"), truncated},
        {QStringLiteral("backend"), QStringLiteral("rg")}};
}

QVariantMap runFileNameSearch(const QString &root,
                              const QString &searchPath,
                              const QString &query,
                              int maxCount,
                              int timeoutMs)
{
    const QString queryLower = query.toLower();
    if (queryLower.trimmed().isEmpty()) {
        return {{QStringLiteral("results"), QString()},
                {QStringLiteral("exitCode"), 1},
                {QStringLiteral("truncated"), false},
                {QStringLiteral("backend"), QStringLiteral("rg-files")}};
    }

    auto formatMatches = [&](const QStringList &candidates,
                             const QString &backend,
                             bool sourceTruncated) -> QVariantMap {
        QStringList matches;
        matches.reserve(maxCount);
        const QString rootPrefix = root + QLatin1Char('/');

        for (const QString &candidateRaw : candidates) {
            const QString candidate = candidateRaw.trimmed();
            if (candidate.isEmpty())
                continue;

            const QString fullPath = QDir::isAbsolutePath(candidate)
                ? QDir::cleanPath(candidate)
                : QDir(root).absoluteFilePath(candidate);
            const QString relPath = fullPath.startsWith(rootPrefix)
                ? fullPath.mid(rootPrefix.size())
                : QFileInfo(fullPath).fileName();

            if (!fullPath.toLower().contains(queryLower)
                && !relPath.toLower().contains(queryLower)) {
                continue;
            }

            matches.append(relPath);
            if (matches.size() >= maxCount)
                break;
        }

        const QString output = matches.join(QLatin1Char('\n'));
        const QByteArray outBytes = output.toUtf8();
        bool truncated = sourceTruncated || matches.size() >= maxCount;

        QByteArray clipped = outBytes;
        if (clipped.size() > kMaxSearchBytes) {
            clipped.truncate(kMaxSearchBytes);
            truncated = true;
        }

        return {{QStringLiteral("results"), QString::fromUtf8(clipped)},
                {QStringLiteral("exitCode"), matches.isEmpty() ? 1 : 0},
                {QStringLiteral("truncated"), truncated},
                {QStringLiteral("backend"), backend}};
    };

    QProcess process;
    process.setWorkingDirectory(root);
    process.start(QStringLiteral("rg"), {
        QStringLiteral("--files"),
        searchPath
    });

    if (process.waitForStarted(1000)) {
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            process.waitForFinished(500);
            return {{QStringLiteral("error"), QStringLiteral("file name search timed out")}};
        }

        const QStringList lines = QString::fromUtf8(process.readAllStandardOutput())
                                      .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        return formatMatches(lines, QStringLiteral("rg-files"), false);
    }

    // Fallback when ripgrep is unavailable in runtime env.
    QStringList lines;
    QDirIterator it(searchPath,
                    QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString next = it.next();
        if (next.contains(QStringLiteral("/.git/"))
            || next.contains(QStringLiteral("/build/"))
            || next.contains(QStringLiteral("/node_modules/"))
            || next.contains(QStringLiteral("/.cache/"))) {
            continue;
        }
        lines.append(next);
        if (lines.size() > maxCount * 20)
            break;
    }

    return formatMatches(lines, QStringLiteral("qdiriterator"), lines.size() > maxCount * 20);
}
}

CodexReadFileTool::CodexReadFileTool()
    : BaseTool(QStringLiteral("codex_read_file"),
               QStringLiteral("Read a UTF-8 text file inside the NeurX workspace."))
{}

QVariantMap CodexReadFileTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("path"), stringProperty(QStringLiteral("Workspace-relative or absolute file path"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("path")}}};
}

QVariantMap CodexReadFileTool::execute(const QVariantMap &params)
{
    const QString path = resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (path.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{QStringLiteral("error"), file.errorString()}};

    QByteArray bytes = file.read(kMaxReadBytes + 1);
    const bool truncated = bytes.size() > kMaxReadBytes;
    if (truncated)
        bytes.truncate(kMaxReadBytes);

    return {{QStringLiteral("path"), path},
            {QStringLiteral("content"), QString::fromUtf8(bytes)},
            {QStringLiteral("truncated"), truncated}};
}

CodexSearchWorkspaceTool::CodexSearchWorkspaceTool()
    : BaseTool(QStringLiteral("codex_search_workspace"),
               QStringLiteral("Search text in the NeurX workspace using ripgrep."))
{}

QVariantMap CodexSearchWorkspaceTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("query"), stringProperty(QStringLiteral("Text or regex to search for"))},
                {QStringLiteral("path"), stringProperty(QStringLiteral("Optional workspace-relative subdirectory"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("query")}}};
}

QVariantMap CodexSearchWorkspaceTool::execute(const QVariantMap &params)
{
    const QString query = params.value(QStringLiteral("query")).toString();
    if (query.trimmed().isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("query is required")}};

    const QString root = workspaceRoot();
    const QString searchPath = params.value(QStringLiteral("path")).toString().trimmed().isEmpty()
        ? root
        : resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (searchPath.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    QVariantMap result = runRgSearch(root,
                                     query,
                                     searchPath,
                                     80,
                                     5000);
    const bool noContentHit = result.value(QStringLiteral("results")).toString().trimmed().isEmpty();
    if (noContentHit) {
        QVariantMap fileResult = runFileNameSearch(root,
                                                   searchPath,
                                                   query,
                                                   80,
                                                   5000);
        if (fileResult.value(QStringLiteral("exitCode")).toInt() == 0) {
            fileResult.insert(QStringLiteral("query"), query);
            fileResult.insert(QStringLiteral("kind"), QStringLiteral("filename"));
            return fileResult;
        }
    }

    result.insert(QStringLiteral("query"), query);
    return result;
}

CodexRunCommandTool::CodexRunCommandTool()
    : BaseTool(QStringLiteral("codex_run_command"),
               QStringLiteral("Run an allowlisted local validation or read-only command in the NeurX workspace."))
{}

QVariantMap CodexRunCommandTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("command"), stringProperty(QStringLiteral("Command line, e.g. git status or cmake --build build/Qt_6_10_3_for_macOS-Debug"))},
                {QStringLiteral("reason"), stringProperty(QStringLiteral("Short reason this command is needed"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("command")}}};
}

QVariantMap CodexRunCommandTool::execute(const QVariantMap &params)
{
    const QString root = workspaceRoot();
    const QString command = params.value(QStringLiteral("command")).toString().trimmed();
    if (root.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("workspace root is unavailable")}};
    if (command.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("command is required")}};
    if (hasShellControlToken(command))
        return {{QStringLiteral("error"), QStringLiteral("shell control operators are not allowed")}};

    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("command could not be parsed")}};

    const QString program = parts.takeFirst();
    if (!isAllowedCommand(program, parts))
        return {{QStringLiteral("error"), QStringLiteral("command is not in the Codex allowlist")}};

    QProcess process;
    process.setWorkingDirectory(root);
    process.start(program, parts);
    if (!process.waitForStarted(1000))
        return {{QStringLiteral("error"), QStringLiteral("command could not be started")}};
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(500);
        return {{QStringLiteral("error"), QStringLiteral("command timed out")}};
    }

    QByteArray stdoutBytes = process.readAllStandardOutput();
    QByteArray stderrBytes = process.readAllStandardError();
    const bool truncated = stdoutBytes.size() + stderrBytes.size() > kMaxCommandBytes;
    if (stdoutBytes.size() > kMaxCommandBytes)
        stdoutBytes.truncate(kMaxCommandBytes);
    if (stdoutBytes.size() + stderrBytes.size() > kMaxCommandBytes)
        stderrBytes.truncate(qMax(0, kMaxCommandBytes - stdoutBytes.size()));

    return {{QStringLiteral("command"), command},
            {QStringLiteral("exitCode"), process.exitCode()},
            {QStringLiteral("stdout"), QString::fromUtf8(stdoutBytes)},
            {QStringLiteral("stderr"), QString::fromUtf8(stderrBytes)},
            {QStringLiteral("truncated"), truncated}};
}

CodexProposePatchTool::CodexProposePatchTool()
    : BaseTool(QStringLiteral("codex_propose_patch"),
               QStringLiteral("Return a git-style unified diff for the user to review. Does not modify files."))
{}

QVariantMap CodexProposePatchTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("path"), stringProperty(QStringLiteral("Workspace-relative file path"))},
                {QStringLiteral("patch"), stringProperty(QStringLiteral("Git-style unified diff for the path"))},
                {QStringLiteral("summary"), stringProperty(QStringLiteral("Short explanation of the proposed edit"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("path"), QStringLiteral("patch")}}};
}

QVariantMap CodexProposePatchTool::execute(const QVariantMap &params)
{
    const QString path = resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (path.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    return {{QStringLiteral("path"), path},
            {QStringLiteral("summary"), params.value(QStringLiteral("summary")).toString()},
            {QStringLiteral("patch"), params.value(QStringLiteral("patch")).toString()},
            {QStringLiteral("applied"), false},
            {QStringLiteral("requiresApproval"), true}};
}

CodexCreateFileTool::CodexCreateFileTool()
    : BaseTool(QStringLiteral("codex_create_file"),
               QStringLiteral("Create a UTF-8 text file inside the NeurX workspace."))
{}

QVariantMap CodexCreateFileTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("path"), stringProperty(QStringLiteral("Workspace-relative file path to create"))},
                {QStringLiteral("content"), stringProperty(QStringLiteral("Initial UTF-8 file content"))},
                {QStringLiteral("overwrite"), QVariantMap{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"), QStringLiteral("Allow overwrite when file already exists")}
                }}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("path")}}};
}

QVariantMap CodexCreateFileTool::execute(const QVariantMap &params)
{
    const QString path = resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (path.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    const bool overwrite = params.value(QStringLiteral("overwrite"), false).toBool();
    QFileInfo info(path);
    if (info.exists() && !overwrite) {
        return {{QStringLiteral("error"), QStringLiteral("file already exists; set overwrite=true to replace")}};
    }

    QDir parentDir = info.dir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral("."))) {
        return {{QStringLiteral("error"), QStringLiteral("failed to create parent directory")}};
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {{QStringLiteral("error"), file.errorString()}};

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << params.value(QStringLiteral("content")).toString();
    file.close();

    return {{QStringLiteral("path"), path},
            {QStringLiteral("created"), !info.exists()},
            {QStringLiteral("overwritten"), info.exists()},
            {QStringLiteral("ok"), true}};
}

CodexEditFileTool::CodexEditFileTool()
    : BaseTool(QStringLiteral("codex_edit_file"),
               QStringLiteral("Edit a UTF-8 text file by replacing text inside the NeurX workspace."))
{}

QVariantMap CodexEditFileTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("path"), stringProperty(QStringLiteral("Workspace-relative file path to edit"))},
                {QStringLiteral("find"), stringProperty(QStringLiteral("Exact text to find"))},
                {QStringLiteral("replace"), stringProperty(QStringLiteral("Replacement text"))},
                {QStringLiteral("replaceAll"), QVariantMap{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"), QStringLiteral("Replace all occurrences when true")}
                }}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("path"), QStringLiteral("find"), QStringLiteral("replace")}}};
}

QVariantMap CodexEditFileTool::execute(const QVariantMap &params)
{
    const QString path = resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (path.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    const QString findText = params.value(QStringLiteral("find")).toString();
    const QString replaceText = params.value(QStringLiteral("replace")).toString();
    if (findText.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("find must not be empty")}};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{QStringLiteral("error"), file.errorString()}};

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString original = in.readAll();
    file.close();

    const bool replaceAll = params.value(QStringLiteral("replaceAll"), false).toBool();
    const int occurrences = original.count(findText);
    if (occurrences == 0)
        return {{QStringLiteral("error"), QStringLiteral("find text not found")}};

    QString updated = original;
    int replacedCount = 0;
    if (replaceAll) {
        updated.replace(findText, replaceText);
        replacedCount = occurrences;
    } else {
        const int pos = updated.indexOf(findText);
        if (pos >= 0) {
            updated.replace(pos, findText.size(), replaceText);
            replacedCount = 1;
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {{QStringLiteral("error"), file.errorString()}};

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << updated;
    file.close();

    return {{QStringLiteral("path"), path},
            {QStringLiteral("replaced"), replacedCount},
            {QStringLiteral("ok"), true}};
}

CodexFindSymbolTool::CodexFindSymbolTool()
    : BaseTool(QStringLiteral("codex_find_symbol"),
               QStringLiteral("Find likely symbol definitions in the NeurX workspace."))
{}

QVariantMap CodexFindSymbolTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("symbol"), stringProperty(QStringLiteral("Symbol name to locate"))},
                {QStringLiteral("path"), stringProperty(QStringLiteral("Optional workspace-relative subdirectory"))},
                {QStringLiteral("language"), stringProperty(QStringLiteral("Optional language hint: cpp or qml"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("symbol")}}};
}

QVariantMap CodexFindSymbolTool::execute(const QVariantMap &params)
{
    const QString symbol = params.value(QStringLiteral("symbol")).toString().trimmed();
    if (symbol.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("symbol is required")}};

    const QString root = workspaceRoot();
    const QString searchPath = params.value(QStringLiteral("path")).toString().trimmed().isEmpty()
        ? root
        : resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (searchPath.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    const QString escaped = escapeRegexLiteral(symbol);
    const QString language = params.value(QStringLiteral("language")).toString().trimmed().toLower();

    QString pattern;
    if (language == QLatin1String("qml")) {
        pattern = QStringLiteral("\\b(function\\s+%1\\s*\\(|property\\s+\\w+\\s+%1\\b|id\\s*:\\s*%1\\b)")
            .arg(escaped);
    } else {
        pattern = QStringLiteral("\\b(class|struct|enum|namespace)\\s+%1\\b|\\b%1\\s*\\(")
            .arg(escaped);
    }

    QVariantMap rgResult = runRgSearch(root, pattern, searchPath, 120, 8000);
    rgResult.insert(QStringLiteral("symbol"), symbol);
    rgResult.insert(QStringLiteral("kind"), QStringLiteral("definition"));
    return rgResult;
}

CodexFindReferencesTool::CodexFindReferencesTool()
    : BaseTool(QStringLiteral("codex_find_references"),
               QStringLiteral("Find references/usages of a symbol in the NeurX workspace."))
{}

QVariantMap CodexFindReferencesTool::inputSchema() const
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QVariantMap{
                {QStringLiteral("symbol"), stringProperty(QStringLiteral("Symbol name to search references for"))},
                {QStringLiteral("path"), stringProperty(QStringLiteral("Optional workspace-relative subdirectory"))}
            }},
            {QStringLiteral("required"), QVariantList{QStringLiteral("symbol")}}};
}

QVariantMap CodexFindReferencesTool::execute(const QVariantMap &params)
{
    const QString symbol = params.value(QStringLiteral("symbol")).toString().trimmed();
    if (symbol.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("symbol is required")}};

    const QString root = workspaceRoot();
    const QString searchPath = params.value(QStringLiteral("path")).toString().trimmed().isEmpty()
        ? root
        : resolveWorkspacePath(params.value(QStringLiteral("path")).toString());
    if (searchPath.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("path must stay inside workspace")}};

    const QString escaped = escapeRegexLiteral(symbol);
    const QString pattern = QStringLiteral("\\b%1\\b").arg(escaped);
    QVariantMap rgResult = runRgSearch(root, pattern, searchPath, 250, 8000);
    rgResult.insert(QStringLiteral("symbol"), symbol);
    rgResult.insert(QStringLiteral("kind"), QStringLiteral("reference"));
    return rgResult;
}

} // namespace neurx

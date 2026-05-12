#include "WebSearchTool.h"
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace neurx {

// ─── WebSearchTool ──────────────────────────────────────────────────────────

WebSearchTool::WebSearchTool()
    : BaseTool("web_search",
               "Search the web using DuckDuckGo Instant Answer API.")
{}

QVariantMap WebSearchTool::inputSchema() const
{
    return {{"type", "object"},
            {"properties", QVariantMap{
                {"query", QVariantMap{{"type","string"},
                                      {"description","Search query"}}}
            }},
            {"required", QVariantList{"query"}}};
}

QVariantMap WebSearchTool::execute(const QVariantMap &params)
{
    const QString query = params.value("query").toString().trimmed();
    if (query.isEmpty())
        return {{"error", "query is required"}};

    QNetworkAccessManager nam;
    const QUrl url(QStringLiteral("https://api.duckduckgo.com/?q=%1&format=json&no_html=1")
                       .arg(QString::fromUtf8(QUrl::toPercentEncoding(query))));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "NeurX/1.0");
    QEventLoop loop;
    auto *reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        reply->deleteLater();
        return {{"error", err}};
    }
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();

    return {{"abstract",  doc["Abstract"].toString()},
            {"source",    doc["AbstractSource"].toString()},
            {"url",       doc["AbstractURL"].toString()},
            {"answer",    doc["Answer"].toString()}};
}

// ─── FileSystemTool ─────────────────────────────────────────────────────────

FileSystemTool::FileSystemTool()
    : BaseTool("file_system",
               "Read, write, list, or delete files on the local filesystem.")
{}

QVariantMap FileSystemTool::inputSchema() const
{
    return {{"type", "object"},
            {"properties", QVariantMap{
                {"action", QVariantMap{{"type","string"},
                    {"enum", QVariantList{"read","write","list","delete","exists"}}}},
                {"path",    QVariantMap{{"type","string"}}},
                {"content", QVariantMap{{"type","string"}}}
            }},
            {"required", QVariantList{"action","path"}}};
}

QVariantMap FileSystemTool::execute(const QVariantMap &params)
{
    const QString action = params.value("action").toString();
    const QString path   = params.value("path").toString();

    if (action == "read") {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {{"error", f.errorString()}};
        return {{"content", QString::fromUtf8(f.readAll())}};
    }
    if (action == "write") {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {{"error", f.errorString()}};
        f.write(params.value("content").toString().toUtf8());
        return {{"ok", true}};
    }
    if (action == "list") {
        QDir dir(path);
        return {{"entries", dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)}};
    }
    if (action == "delete") {
        return {{"ok", QFile::remove(path)}};
    }
    if (action == "exists") {
        return {{"exists", QFileInfo::exists(path)}};
    }
    return {{"error", "unknown action"}};
}

// ─── ShellTool ──────────────────────────────────────────────────────────────

ShellTool::ShellTool()
    : BaseTool("shell",
               "Execute a shell command and return stdout/stderr.")
{}

QVariantMap ShellTool::inputSchema() const
{
    return {{"type", "object"},
            {"properties", QVariantMap{
                {"command",    QVariantMap{{"type","string"}}},
                {"workingDir", QVariantMap{{"type","string"}}},
                {"timeoutMs",  QVariantMap{{"type","integer"}}}
            }},
            {"required", QVariantList{"command"}}};
}

QVariantMap ShellTool::execute(const QVariantMap &params)
{
    const QString command    = params.value("command").toString();
    const QString workingDir = params.value("workingDir").toString();
    const int     timeoutMs  = params.value("timeoutMs", 30000).toInt();

    QProcess proc;
    if (!workingDir.isEmpty())
        proc.setWorkingDirectory(workingDir);

#ifdef Q_OS_WIN
    proc.start("cmd.exe", {"/C", command});
#else
    proc.start("/bin/sh", {"-c", command});
#endif
    if (!proc.waitForFinished(timeoutMs))
        return {{"error", "timeout"}, {"exitCode", -1}};

    return {
        {"stdout",   QString::fromUtf8(proc.readAllStandardOutput())},
        {"stderr",   QString::fromUtf8(proc.readAllStandardError())},
        {"exitCode", proc.exitCode()}
    };
}

// ─── HttpTool ───────────────────────────────────────────────────────────────

HttpTool::HttpTool()
    : BaseTool("http_request",
               "Perform an HTTP GET or POST request.")
{}

QVariantMap HttpTool::inputSchema() const
{
    return {{"type", "object"},
            {"properties", QVariantMap{
                {"method",  QVariantMap{{"type","string"}, {"enum", QVariantList{"GET","POST","PUT","DELETE"}}}},
                {"url",     QVariantMap{{"type","string"}}},
                {"headers", QVariantMap{{"type","object"}}},
                {"body",    QVariantMap{{"type","string"}}}
            }},
            {"required", QVariantList{"method","url"}}};
}

QVariantMap HttpTool::execute(const QVariantMap &params)
{
    const QString method = params.value("method", "GET").toString().toUpper();
    const QUrl    url(params.value("url").toString());
    if (!url.isValid())
        return {{"error", "invalid URL"}};

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    const auto headers = params.value("headers").toMap();
    for (auto it = headers.begin(); it != headers.end(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());

    QEventLoop loop;
    QNetworkReply *reply = nullptr;
    const QByteArray body = params.value("body").toString().toUtf8();

    if (method == "GET")
        reply = nam.get(req);
    else if (method == "POST")
        reply = nam.post(req, body);
    else if (method == "PUT")
        reply = nam.put(req, body);
    else if (method == "DELETE")
        reply = nam.deleteResource(req);
    else
        return {{"error", "unsupported method"}};

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int    statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString responseBody = QString::fromUtf8(reply->readAll());
    reply->deleteLater();

    return {{"statusCode", statusCode}, {"body", responseBody}};
}

} // namespace neurx

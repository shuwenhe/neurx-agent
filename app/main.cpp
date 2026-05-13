#include <QGuiApplication>
#include <QByteArray>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#ifdef NEURX_HAS_WEBENGINEQUICK
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

#include "runtime/AgentRuntime.h"
#include "bridge/RuntimeBridge.h"
#include "bridge/AgentListModel.h"
#include "bridge/LogModel.h"

namespace {

void enableWebContentsAutoDarkMode()
{
#ifdef NEURX_HAS_WEBENGINEQUICK
    constexpr auto kAutoDarkFlag = "--enable-features=WebContentsForceDark";
    const QByteArray existingFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (existingFlags.contains(kAutoDarkFlag))
        return;

    QByteArray flags = existingFlags.trimmed();
    if (!flags.isEmpty())
        flags.append(' ');
    flags.append(kAutoDarkFlag);
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    enableWebContentsAutoDarkMode();

#ifdef NEURX_HAS_WEBENGINEQUICK
    QtWebEngineQuick::initialize();
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName("NeurX");
    app.setApplicationVersion(QStringLiteral("%1.%2.%3")
        .arg(NEURX_VERSION_MAJOR)
        .arg(NEURX_VERSION_MINOR)
        .arg(NEURX_VERSION_PATCH));
    app.setOrganizationName("NeurX");
    app.setOrganizationDomain("io.neurx");

    QQuickStyle::setStyle("Basic");

    // Expose C++ objects to QML
    auto &runtime  = neurx::AgentRuntime::instance();
    auto *bridge   = new neurx::RuntimeBridge(&runtime);
    auto *agentModel = new neurx::AgentListModel(&runtime);
    auto *logModel   = new neurx::LogModel;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Runtime",    bridge);
    engine.rootContext()->setContextProperty("AgentModel", agentModel);
    engine.rootContext()->setContextProperty("LogModel",   logModel);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("neurx", "Main");

    runtime.start();

    int ret = app.exec();
    runtime.shutdown();
    return ret;
}

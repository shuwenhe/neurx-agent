#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "runtime/AgentRuntime.h"
#include "bridge/RuntimeBridge.h"
#include "bridge/AgentListModel.h"
#include "bridge/LogModel.h"

int main(int argc, char *argv[])
{
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

#pragma once
#include "../../core/tools/BaseTool.h"

namespace neurx {

class WebSearchTool : public BaseTool
{
    Q_OBJECT
public:
    WebSearchTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class FileSystemTool : public BaseTool
{
    Q_OBJECT
public:
    FileSystemTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class ShellTool : public BaseTool
{
    Q_OBJECT
public:
    ShellTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class HttpTool : public BaseTool
{
    Q_OBJECT
public:
    HttpTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

} // namespace neurx

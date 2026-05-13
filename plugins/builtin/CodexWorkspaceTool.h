#pragma once

#include "../../core/tools/BaseTool.h"

namespace neurx {

class CodexReadFileTool : public BaseTool
{
    Q_OBJECT
public:
    CodexReadFileTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexSearchWorkspaceTool : public BaseTool
{
    Q_OBJECT
public:
    CodexSearchWorkspaceTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexRunCommandTool : public BaseTool
{
    Q_OBJECT
public:
    CodexRunCommandTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexProposePatchTool : public BaseTool
{
    Q_OBJECT
public:
    CodexProposePatchTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexCreateFileTool : public BaseTool
{
    Q_OBJECT
public:
    CodexCreateFileTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexEditFileTool : public BaseTool
{
    Q_OBJECT
public:
    CodexEditFileTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexFindSymbolTool : public BaseTool
{
    Q_OBJECT
public:
    CodexFindSymbolTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

class CodexFindReferencesTool : public BaseTool
{
    Q_OBJECT
public:
    CodexFindReferencesTool();
    QVariantMap execute(const QVariantMap &params) override;
    QVariantMap inputSchema() const override;
};

} // namespace neurx

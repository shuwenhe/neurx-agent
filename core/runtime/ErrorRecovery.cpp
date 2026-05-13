#include "ErrorRecovery.h"

namespace neurx {

bool ErrorRecovery::isRecoverable(ErrorType type)
{
    switch (type) {
    case ErrorType::ToolNotFound:
    case ErrorType::ToolArgumentError:
    case ErrorType::PermissionDenied:
        return false;  // Agent logic error or policy violation

    case ErrorType::ToolTimeout:
    case ErrorType::ToolExecutionFailed:
    case ErrorType::LlmTimeout:
    case ErrorType::NetworkError:
        return true;   // Transient, can retry or skip

    case ErrorType::LlmRequestFailed:
    case ErrorType::ContextTooLarge:
    case ErrorType::Unknown:
        return false;  // Unrecoverable for this turn

    default:
        return false;
    }
}

QString ErrorRecovery::userMessage(const ErrorContext &ctx)
{
    switch (ctx.type) {
    case ErrorType::ToolNotFound:
        return QStringLiteral("Tool '%1' not available. Check tool name.").arg(ctx.toolName);

    case ErrorType::ToolTimeout:
        return QStringLiteral("Tool '%1' timed out (30s limit). Skipping.").arg(ctx.toolName);

    case ErrorType::ToolArgumentError:
        return QStringLiteral("Tool '%1' received invalid arguments: %2")
            .arg(ctx.toolName, ctx.message);

    case ErrorType::ToolExecutionFailed:
        return QStringLiteral("Tool '%1' execution failed: %2").arg(ctx.toolName, ctx.message);

    case ErrorType::PermissionDenied:
        return QStringLiteral("Tool '%1' blocked by permissions: %2")
            .arg(ctx.toolName, ctx.message);

    case ErrorType::LlmRequestFailed:
        return QStringLiteral("LLM request failed: %1").arg(ctx.message);

    case ErrorType::LlmTimeout:
        return QStringLiteral("LLM request timed out (30s limit). Generating fallback response.");

    case ErrorType::ContextTooLarge:
        return QStringLiteral("Context too large. Summarizing history.");

    case ErrorType::NetworkError:
        return QStringLiteral("Network error: %1. Check your connection.").arg(ctx.message);

    default:
        return QStringLiteral("Error: %1").arg(ctx.message);
    }
}

QString ErrorRecovery::diagnosticMessage(const ErrorContext &ctx)
{
    return QString(QLatin1String("[ERROR] type=%1 tool=%2 msg=%3 retries=%4"))
        .arg(QString::number(static_cast<int>(ctx.type)),
             ctx.toolName.isEmpty() ? QLatin1String("(none)") : ctx.toolName,
             ctx.message,
             QString::number(ctx.retryCount));
}

QString ErrorRecovery::fallbackResponse(const QString &toolName, const ErrorContext &ctx)
{
    if (ctx.type == ErrorType::ToolTimeout) {
        return QStringLiteral(
            "Tool '%1' timed out. This typically means the operation took too long. "
            "Try breaking your request into smaller steps or running the command manually.")
            .arg(toolName);
    }

    if (ctx.type == ErrorType::ToolExecutionFailed) {
        return QStringLiteral(
            "Tool '%1' failed: %2\n"
            "Possible solutions:\n"
            "1. Check if the command/path is correct\n"
            "2. Verify permissions\n"
            "3. Try a simpler operation first")
            .arg(toolName, ctx.message);
    }

    if (ctx.type == ErrorType::PermissionDenied) {
        return QStringLiteral(
            "Tool '%1' is blocked by your permission settings (%2).\n"
            "To allow this operation, check Settings → Permissions.")
            .arg(toolName, ctx.message);
    }

    return QStringLiteral("Tool '%1' encountered an error. Please try again or contact support.")
        .arg(toolName);
}

} // namespace neurx

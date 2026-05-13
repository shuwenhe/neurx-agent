#pragma once

#include <QString>
#include <QVariantMap>

namespace neurx {

/// Error recovery strategies and utilities for agent operations.
class ErrorRecovery
{
public:
    enum class ErrorType {
        ToolNotFound,
        ToolTimeout,
        ToolArgumentError,
        ToolExecutionFailed,
        PermissionDenied,
        LlmRequestFailed,
        LlmTimeout,
        ContextTooLarge,
        NetworkError,
        Unknown
    };

    struct ErrorContext {
        ErrorType type;
        QString message;
        QString toolName;
        QVariantMap toolArgs;
        int retryCount{0};
        bool isRecoverable{true};
    };

    /// Determine if an error is recoverable
    static bool isRecoverable(ErrorType type);

    /// Generate user-friendly error message
    static QString userMessage(const ErrorContext &ctx);

    /// Generate diagnostic message for logging
    static QString diagnosticMessage(const ErrorContext &ctx);

    /// Create fallback response when tool fails
    static QString fallbackResponse(const QString &toolName, const ErrorContext &ctx);
};

} // namespace neurx

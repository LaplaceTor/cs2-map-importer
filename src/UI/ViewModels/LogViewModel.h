#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <memory>
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogManager.h"

namespace UI::ViewModels {

class LogViewModel : public QObject, public Core::Logging::ILogSink, public std::enable_shared_from_this<LogViewModel> {
    Q_OBJECT

    Q_PROPERTY(QString formattedLogText READ formattedLogText NOTIFY logTextChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY logTextChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)

public:
    explicit LogViewModel(QObject* parent = nullptr);
    ~LogViewModel() override;

    void registerWithLogManager();
    void unregisterFromLogManager();

    QString formattedLogText() const;
    int lineCount() const;
    bool autoScroll() const noexcept { return m_autoScroll; }
    void setAutoScroll(bool enabled);

    // ILogSink implementation
    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override;
    bool flush() override;

public slots:
    void clear();
    void copyToClipboard();
    void appendLog(const QString& message, int level = 0);

signals:
    void logTextChanged();
    void autoScrollChanged();

private:
    void appendRawLogLine(const QString& rawMessage, Core::Logging::LogLevel level);
    QString formatHtmlLine(const QString& rawMessage, Core::Logging::LogLevel level) const;

    mutable QMutex m_mutex;
    QStringList m_logLines;
    QStringList m_plainLines;
    bool m_autoScroll = true;
    static constexpr int MaxLogLines = 2000;
};

} // namespace UI::ViewModels


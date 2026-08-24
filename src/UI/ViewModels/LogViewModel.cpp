#include "UI/ViewModels/LogViewModel.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDateTime>
#include <QMetaObject>

namespace UI::ViewModels {

LogViewModel::LogViewModel(QObject* parent)
    : QObject(parent)
{
}

LogViewModel::~LogViewModel() {
    unregisterFromLogManager();
}

void LogViewModel::registerWithLogManager() {
    try {
        Core::Logging::LogManager::instance().addSink(shared_from_this());
    } catch (...) {
        // Handle standalone/unregistered instances
    }
}

void LogViewModel::unregisterFromLogManager() {
    try {
        Core::Logging::LogManager::instance().removeSink(shared_from_this());
    } catch (...) {
    }
}

QString LogViewModel::formattedLogText() const {
    QMutexLocker locker(&m_mutex);
    return m_logLines.join(QStringLiteral("<br>"));
}

int LogViewModel::lineCount() const {
    QMutexLocker locker(&m_mutex);
    return m_logLines.size();
}

void LogViewModel::setAutoScroll(bool enabled) {
    if (m_autoScroll != enabled) {
        m_autoScroll = enabled;
        emit autoScrollChanged();
    }
}

bool LogViewModel::writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) {
    Q_UNUSED(taskName);
    const auto entries = block.entries();
    for (const auto& entry : entries) {
        QString msg = entry.message;
        Core::Logging::LogLevel level = entry.level;
        QMetaObject::invokeMethod(this, [this, msg, level]() {
            appendRawLogLine(msg, level);
        }, Qt::QueuedConnection);
    }
    return true;
}

bool LogViewModel::flush() {
    return true;
}

void LogViewModel::clear() {
    {
        QMutexLocker locker(&m_mutex);
        m_logLines.clear();
        m_plainLines.clear();
    }
    emit logTextChanged();
}

void LogViewModel::copyToClipboard() {
    QString fullText;
    {
        QMutexLocker locker(&m_mutex);
        fullText = m_plainLines.join(QLatin1Char('\n'));
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(fullText);
    }
}

void LogViewModel::appendLog(const QString& message, int level) {
    appendRawLogLine(message, static_cast<Core::Logging::LogLevel>(level));
}

void LogViewModel::appendRawLogLine(const QString& rawMessage, Core::Logging::LogLevel level) {
    if (rawMessage.isEmpty()) {
        return;
    }

    QString htmlLine = formatHtmlLine(rawMessage, level);

    {
        QMutexLocker locker(&m_mutex);
        m_plainLines.append(rawMessage);
        m_logLines.append(htmlLine);

        if (m_logLines.size() > MaxLogLines) {
            m_logLines.removeFirst();
            m_plainLines.removeFirst();
        }
    }

    emit logTextChanged();
}

QString LogViewModel::formatHtmlLine(const QString& rawMessage, Core::Logging::LogLevel level) const {
    QString safeMsg = rawMessage;
    safeMsg.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
           .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
           .replace(QLatin1Char('>'), QStringLiteral("&gt;"));

    // Check for level or keyword based coloring
    if (level == Core::Logging::LogLevel::Critical || level == Core::Logging::LogLevel::Error || safeMsg.contains(QStringLiteral("ERROR"), Qt::CaseInsensitive)) {
        return QStringLiteral("<font color='#FF5252'>") + safeMsg + QStringLiteral("</font>");
    }
    if (level == Core::Logging::LogLevel::Warning || safeMsg.contains(QStringLiteral("WARN"), Qt::CaseInsensitive) || safeMsg.contains(QStringLiteral("Skip"), Qt::CaseInsensitive) || safeMsg.contains(QStringLiteral("Unable"), Qt::CaseInsensitive)) {
        return QStringLiteral("<font color='#FFD740'>") + safeMsg + QStringLiteral("</font>");
    }
    if (safeMsg.contains(QStringLiteral("Finished"), Qt::CaseInsensitive) || safeMsg.contains(QStringLiteral("Success"), Qt::CaseInsensitive)) {
        return QStringLiteral("<font color='#69F0AE'>") + safeMsg + QStringLiteral("</font>");
    }
    if (level == Core::Logging::LogLevel::Debug) {
        return QStringLiteral("<font color='#9E9E9E'>") + safeMsg + QStringLiteral("</font>");
    }

    return QStringLiteral("<font color='#E0E0E0'>") + safeMsg + QStringLiteral("</font>");
}

} // namespace UI::ViewModels

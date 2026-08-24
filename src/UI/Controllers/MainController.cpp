#include "UI/Controllers/MainController.h"
#include <QGuiApplication>
#include <QStyleHints>

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

namespace UI::Controllers {

MainController::MainController(QObject* parent)
    : QObject(parent)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (m_theme == QStringLiteral("system")) {
                applyTheme(QStringLiteral("system"));
            }
        });
    }
#endif
}

void MainController::setActiveTab(int tab) {
    if (m_isProcessing) {
        emit alertRequested(
            QStringLiteral("Task in Progress"),
            QStringLiteral("An operation is currently executing. You cannot change tabs until the current operation completes.")
        );
        return;
    }

    if (m_activeTab != tab) {
        m_activeTab = tab;
        emit activeTabChanged();
    }
}

void MainController::setTheme(const QString& themeName) {
    if (m_theme != themeName) {
        m_theme = themeName;
        applyTheme(m_theme);
        emit themeChanged();
    }
}

void MainController::cycleTheme() {
    if (m_theme == QStringLiteral("system")) {
        setTheme(QStringLiteral("light"));
    } else if (m_theme == QStringLiteral("light")) {
        setTheme(QStringLiteral("dark"));
    } else {
        setTheme(QStringLiteral("system"));
    }
}

QString MainController::appVersion() const {
    return QStringLiteral(APP_VERSION);
}

void MainController::applyTheme(const QString& themeName) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto* hints = QGuiApplication::styleHints()) {
        if (themeName == QStringLiteral("light")) {
            hints->setColorScheme(Qt::ColorScheme::Light);
        } else if (themeName == QStringLiteral("dark")) {
            hints->setColorScheme(Qt::ColorScheme::Dark);
        } else {
            hints->setColorScheme(Qt::ColorScheme::Unknown);
        }
    }
#else
    Q_UNUSED(themeName);
#endif
}

void MainController::startImport() {
    if (m_isProcessing) {
        return;
    }
    // Placeholder for WorkflowRunner integration in Stage 4
    emit alertRequested(
        QStringLiteral("Feature in Development"),
        QStringLiteral("Import execution will be connected in the upcoming Workflow integration stage.")
    );
}

void MainController::stopImport() {
    if (!m_isProcessing) {
        return;
    }
    // Placeholder for cancellation in Stage 4
}

void MainController::checkForUpdates() {
    // Placeholder for UpdateService integration in Stage 4
    emit alertRequested(
        QStringLiteral("Check for Updates"),
        QStringLiteral("You are currently running the latest development version (v%1).").arg(appVersion())
    );
}

void MainController::toggleLogWindow() {
    emit logWindowToggleRequested();
}

} // namespace UI::Controllers


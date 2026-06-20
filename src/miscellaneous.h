#pragma once

#include <QString>
#include <QStringList>
#include <functional>
#include <QAtomicInt>
#include <QException>

class AppException : public QException {
public:
    explicit AppException(const QString& msg) : msg_(msg) {}

    void raise() const override { throw *this; }
    AppException *clone() const override { return new AppException(*this); }

    QString message() const { return msg_; }
private:
    QString msg_;
};

class Miscellaneous {
public:
    using LogCallback = std::function<void(const QString&)>;

    struct Options {
        QString cs2_basefolder;
        QString s1game_basefolder;
        QString csgogamedir;
        QString s1_game_type; // "css" or "csgo"
        QString content_folder;
        QString map_name;
        QString bsp_file;
        QString app_dir;
        QString addon_name;

        bool usebsp;
        bool usebsp_nomergeinstances;
        bool skipdeps;

    };

    static LogCallback global_logger;
    static void Log(const QString& msg);

    static bool CheckJava();
    static void MoveVpkSignatures(const QString& cs2_basefolder, bool& vpk_signatures_moved);
    static void RestoreVpkSignatures(const QString& cs2_basefolder);

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir

    static int RunCommandSync(const QString& cmd);
    static void CancelAll();

    static QAtomicInt cancel_import;


private:
};

bool CopyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir);

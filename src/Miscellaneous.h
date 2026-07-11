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
        QString cs2Basefolder;
        QString s1gameBasefolder;
        QString csgogamedir;
        QString s1GameType; // "css" or "csgo"
        QString contentFolder;
        QString mapName;
        QString bspFile;
        QString appDir;
        QString addonName;

        QString s1gamedir;
        QString s1contentdir;
        QString s2contentdir;

        bool keepFuncDetailAsBrush;
        bool usebsp;
        bool usebspNomergeinstances;
        bool skipdeps;
    };

    static const Options& GetOptions();
    static void SetOptions(const Options& options);

    static LogCallback GlobaLLogger;
    static void Log(const QString& msg);

    static bool CheckJava();
    static void MoveVpkSignatures(const QString& cs2Basefolder, bool& vpkSignaturesMoved);
    static void RestoreVpkSignatures(const QString& cs2Basefolder);

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir

    static int RunCommandSync(const QString& cmd);
    static void CancelAll();

    static QAtomicInt CanceLImport;


private:
};

bool CopyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir);

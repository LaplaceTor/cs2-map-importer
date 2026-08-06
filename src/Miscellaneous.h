#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
#include <QAtomicInt>
#include <QException>

struct KeyMapping {
    QString newKey;
    bool appendAlpha1;
};

extern const QMap<QString, KeyMapping> legacyKeyMap;

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
    enum Program {
        PROGRAM_SOURCE1IMPORT = 1,
        PROGRAM_CS_MDL_IMPORT = 2,
        PROGRAM_RESOURCECOMPILER = 3,
        PROGRAM_VPKEDITCLI = 4,
        PROGRAM_VTFCMD = 5,
        PROGRAM_BSPSRC = 6
    };

    using LogCallback = std::function<void(const QString&)>;

    struct SearchTarget {
        bool isVpk;
        QString path;
    };

    struct Options {
        QString cs2Basefolder;
        QString s1gameBasefolder;
        QString csgogamedir;
        QString s1GameType; // "css" or "csgo"
        QString othergamedir;
        QString contentFolder;
        QString mapName;
        QString bspFile;
        QString appDir;
        QString addonName;

        QString s1gamedir;
        QString s1contentdir;
        QString s2contentdir;

        bool cmdLogOut = false;
        bool keepFuncDetailAsBrush = false;
        bool usebsp = false;
        bool usebspNomergeinstances = false;
        bool skipdeps = false;

        bool modelSkipAnimation = false;
        bool modelChangeBindpose = false;
        bool modelOverrideLean = false;
        bool modelHeaderHullBounds = false;
        bool modelImportLods = false;
        bool modelWriteWeaponPrefab = false;

        bool particleAllowDepthBlend = false;
        bool particleDisableDiffuse = false;

        QList<SearchTarget> searchTargets;
    };

    static Options GetOptions();
    static bool ParseGameInfo(const QString& gameinfoPath, QList<SearchTarget>& targets);
    static QString GetBaseFolderFromGameInfo(const QString& gameinfoPath);
    static void SetOptions(const Options& options);
    static QString GetS1Subfolder(const QString& gameType);

    static LogCallback GlobaLLogger;
    static void Log(const QString& msg);

    static void MoveVpkSignatures(const QString& cs2Basefolder, bool& vpkSignaturesMoved);
    static void RestoreVpkSignatures(const QString& cs2Basefolder);

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir

    static int RunCommandSync(int program, const QStringList& arguments, QStringList* logOutString = nullptr, bool isMap = false, bool isCSGO = false);
    static void CancelAll();

    static bool RunParallelTasks(const QList<std::function<void()>>& tasks, int threads = 8);

    static QAtomicInt CanceLImport;

    static QString CleanRefPath(QString input);
    static QStringList ReadTextFile(const QString& filepath);
    static void EnsureFileWritable(const QString& filepath);

    static bool IsCorrectSymlink(const QString& linkPath, const QString& targetPath);
    static bool CreateSymlink(const QString& linkPath, const QString& targetPath);

private:
};

bool CopyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir);

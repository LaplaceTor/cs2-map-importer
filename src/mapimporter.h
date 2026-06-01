#ifndef MAPIMPORTER_H
#define MAPIMPORTER_H

#include <QObject>
#include <QFile>
#include <QString>
#include <QSet>
#include <QProcess>

class MapImporter : public QObject
{
    Q_OBJECT
public:
    explicit MapImporter(QObject *parent = nullptr);

    void setPaths(const QString& s1gamecsgo,
                  const QString& s1contentcsgo,
                  const QString& s2gamecsgo,
                  const QString& s2addon,
                  const QString& mapname);

    void setOptions(bool usebsp, bool nomergeinstances, bool skipdeps);
    void setBinPath(const QString& binPath);

public slots:
    void run();

signals:
    void logMessage(const QString& msg);
    void finished();
    void error(const QString& errorMsg);

private:
    QString s1gamecsgo;
    QString s1contentcsgo;
    QString s2gamecsgo;
    QString s2addon;
    QString mapname;
    QString binPath;

    bool usebsp;
    bool nomergeinstances;
    bool skipdeps;

    QString s2gameaddon;
    QString s2contentcsgo;
    QString s2contentcsgoimported;

    int runCommand(const QString& program, const QStringList& args, const QString& workingDirectory = QString());

    void stripMDLsFromRefs(const QString& filename);
    bool force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials, QFile& global2UVMaterialsFile);
    void forceUV2ForVMAT(const QString& mtlfile);

    void importAndCompileMapMDLs(const QString& filename);
    void importAndCompileMapRefs(const QString& refsFile);
};

#endif // MAPIMPORTER_H

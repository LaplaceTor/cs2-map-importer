#ifndef MAPIMPORTER_H
#define MAPIMPORTER_H

#include <QObject>
#include <QString>
#include <string>
#include <vector>
#include <set>

class MapImporter : public QObject
{
    Q_OBJECT
public:
    explicit MapImporter(QObject *parent = nullptr);

    // Keep QString in the public API for Qt compatibility from the UI thread,
    // but internally convert to std::string.
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
    std::string s1gamecsgo;
    std::string s1contentcsgo;
    std::string s2gamecsgo;
    std::string s2addon;
    std::string mapname;
    std::string binPath;

    bool usebsp;
    bool nomergeinstances;
    bool skipdeps;

    std::string s2gamedir;
    std::string s2contentcsgo;
    std::string s2contentdir;

    int runCommand(const std::string& program, const std::vector<std::string>& args);

    void stripMDLsFromRefs(const std::string& filename);
    bool force2UVsIfRequired(const std::string& refsName, std::set<std::string>& global2UVMaterials, const std::string& global2UVMaterialsFile);
    void forceUV2ForVMAT(const std::string& mtlfile);

    void importAndCompileMapMDLs(const std::string& filename);
    void importAndCompileMapRefs(const std::string& refsFile);

    void emitLog(const std::string& msg);
};

#endif // MAPIMPORTER_H

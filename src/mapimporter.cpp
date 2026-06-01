#include "mapimporter.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QProcessEnvironment>
#include <QThread>

MapImporter::MapImporter(QObject *parent) : QObject(parent),
    usebsp(false), nomergeinstances(false), skipdeps(false)
{
}

void MapImporter::setPaths(const QString& s1game, const QString& s1content,
                           const QString& s2game, const QString& addon, const QString& map)
{
    s1gamecsgo = s1game;
    s1contentcsgo = s1content;
    s2gamecsgo = s2game;
    s2addon = addon;
    mapname = map;

    QString s2gameaddondir = "game\\csgo_addons\\" + s2addon;
    s2gameaddon = s2gamecsgo;
    s2gameaddon.replace("game\\csgo", s2gameaddondir);

    s2contentcsgo = s2gameaddon;
    s2contentcsgo.replace("game\\csgo_addons", "content\\csgo_addons");
    s2contentcsgoimported = s2contentcsgo;
}

void MapImporter::setOptions(bool bsp, bool nomerge, bool skip)
{
    usebsp = bsp;
    nomergeinstances = nomerge;
    skipdeps = skip;
}

void MapImporter::setBinPath(const QString& bp)
{
    binPath = bp;
}

int MapImporter::runCommand(const QString& program, const QStringList& args, const QString& workingDirectory)
{
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString currentPath = env.value("PATH");
    env.insert("PATH", binPath + ";" + currentPath);
    proc.setProcessEnvironment(env);

    if (!workingDirectory.isEmpty()) {
        proc.setWorkingDirectory(workingDirectory);
    }

    emit logMessage(QString("> %1 %2").arg(program).arg(args.join(" ")));

    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);

    if (!proc.waitForStarted(-1)) {
        emit logMessage("Failed to start " + program);
        return -1;
    }

    while (proc.state() == QProcess::Running) {
        if (proc.waitForReadyRead(100)) {
            QString out = QString::fromLocal8Bit(proc.readAll());
            emit logMessage(out.trimmed());
        }
        // Instead of QCoreApplication::processEvents() which could be unsafe depending on thread context,
        // since we are in a worker thread, we can just block wait or let it run.
    }

    // Read remaining output
    QString out = QString::fromLocal8Bit(proc.readAll());
    if (!out.isEmpty()) {
        emit logMessage(out.trimmed());
    }

    return proc.exitCode();
}

void MapImporter::run()
{
    emit logMessage("Starting Map Import...");

    try {
        QString mapImportCmd = "source1import";
        QStringList mapImportArgs;
        mapImportArgs << "-retail" << "-nop4" << "-nop4sync";
        if (usebsp) mapImportArgs << "-usebsp";
        if (nomergeinstances) mapImportArgs << "-usebsp_nomergeinstances";
        mapImportArgs << "-src1gameinfodir" << s1gamecsgo;
        mapImportArgs << "-src1contentdir" << s1contentcsgo;
        mapImportArgs << "-s2addon" << s2addon;
        mapImportArgs << "-game" << "csgo";
        mapImportArgs << "maps\\" + mapname + ".vmf";

        runCommand("source1import", mapImportArgs);

        QString prefabMapname = mapname;
        prefabMapname.replace("instances", "prefabs");

        if (!skipdeps) {
            QString prefabRefsPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_refs.txt";
            stripMDLsFromRefs(prefabRefsPath);

            QString prefabMdlLstPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_mdl_lst.txt";
            importAndCompileMapMDLs(prefabMdlLstPath);

            QString prefabNewRefsPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_new_refs.txt";
            importAndCompileMapRefs(prefabNewRefsPath);

            // Quick import vmf again to pick up dependencies
            runCommand("source1import", mapImportArgs);
        }

        // Explicit copy of main .vmap to game\csgo\maps if not already there
        QString finalVmapPath = s2contentcsgo + "\\maps\\" + prefabMapname + ".vmap";
        if (!QFile::exists(finalVmapPath)) {
            QString srcVmap = s2contentcsgoimported + "\\maps\\" + prefabMapname + ".vmap";
            QDir().mkpath(s2contentcsgo + "\\maps");
            emit logMessage("Copying " + srcVmap + " to " + s2contentcsgo + "\\maps\\");
            // xcopy in C++: just copy file directly
            QFile::copy(srcVmap, finalVmapPath);
        }

        emit logMessage("Map Import Finished.");
        emit finished();

    } catch (const std::exception& e) {
        emit error(QString(e.what()));
    }
}

void MapImporter::stripMDLsFromRefs(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList mdls;
    QStringList others;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.endsWith(".mdl", Qt::CaseInsensitive)) {
            mdls.append(line);
        } else {
            others.append(line);
        }
    }
    file.close();

    QString mdlfilename = filename;
    mdlfilename.replace("_refs.txt", "_mdl_lst.txt");
    QFile mdlFile(mdlfilename);
    if (mdlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&mdlFile);
        for (const QString& mdl : mdls) {
            out << mdl << "\n";
        }
    }

    QString refsfilename = filename;
    refsfilename.replace("_refs.txt", "_new_refs.txt");
    QFile newRefsFile(refsfilename);
    if (newRefsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&newRefsFile);
        for (const QString& other : others) {
            out << other << "\n";
        }
    }
}

void MapImporter::forceUV2ForVMAT(const QString& mtlfile)
{
    QString vmatfilename = s2contentcsgoimported + "\\" + mtlfile;
    vmatfilename.replace(".vmt", ".vmat");

    QFile file(vmatfilename);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    bool modified = false;
    for (int i = 0; i < lines.size(); ++i) {
        QString txt = lines[i].trimmed().toLower();
        if (txt.startsWith("\"shader\"")) {
            if (i + 1 < lines.size()) {
                QString txtNext = lines[i + 1].trimmed().replace("\t", "");
                if (!txtNext.startsWith("\"F_FORCE_UV2\"", Qt::CaseInsensitive)) {
                    lines.insert(i + 1, "\t\"F_FORCE_UV2\" \"1\"");
                    modified = true;
                    break;
                }
            }
        }
    }

    if (modified) {
        emit logMessage("Added F_FORCE_UV2 to " + vmatfilename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            for (const QString& line : lines) {
                out << line << "\n";
            }
        }
    }
}

bool MapImporter::force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials, QFile& global2UVMaterialsFile)
{
    QString meshinfofilename = refsName;
    meshinfofilename.replace("_refs.txt", "_refs\\mesh\\meshinfo.txt");
    meshinfofilename.replace("/", "\\");

    QFile meshFile(meshinfofilename);
    if (!meshFile.exists()) return false;

    if (!meshFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString meshinfoStr = meshFile.readAll();
    meshFile.close();

    bool is2UV = false;
    QRegularExpression rx("'numuvs'\\s*:\\s*2");
    if (rx.match(meshinfoStr).hasMatch()) {
        is2UV = true;
    }

    bool b2UV = false;
    QFile refsFile(refsName);
    if (!refsFile.exists() || !refsFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QStringList refsList;
    QTextStream in(&refsFile);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            refsList.append(line);
        }
    }
    refsFile.close();

    QSet<QString> uvsUpdated;

    for (const QString& mtlfile : refsList) {
        if (uvsUpdated.contains(mtlfile)) continue;

        if (global2UVMaterials.contains(mtlfile)) {
            b2UV = true;
            uvsUpdated.insert(mtlfile);
        } else {
            if (is2UV) {
                b2UV = true;
                emit logMessage("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                uvsUpdated.insert(mtlfile);

                if (!global2UVMaterials.contains(mtlfile)) {
                    QTextStream out(&global2UVMaterialsFile);
                    out << mtlfile << "\n";
                    global2UVMaterialsFile.flush();
                    global2UVMaterials.insert(mtlfile);
                }

                forceUV2ForVMAT(mtlfile);
            }
        }
    }

    return b2UV;
}

void MapImporter::importAndCompileMapMDLs(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage("No MDLs to import (file not found)");
        return;
    }

    QStringList mdlfiles;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            mdlfiles.append(line);
        }
    }
    file.close();

    if (mdlfiles.isEmpty()) {
        emit logMessage("No MDLs to import");
        return;
    }

    emit logMessage("Importing models");
    emit logMessage("--------------------------------");
    for (const QString& x : mdlfiles) {
        if (!x.startsWith("-")) {
            emit logMessage(x);
        }
    }
    emit logMessage("--------------------------------");

    QStringList force2UVListFiles;
    QSet<QString> mdlmtls;
    QString extraoptions = "";

    for (QString mdlfile : mdlfiles) {
        if (mdlfile.startsWith("-")) {
            if (mdlfile == "-" || mdlfile == "-nooptions") {
                extraoptions = "";
            } else {
                extraoptions = mdlfile;
            }
        } else {
            mdlfile.replace("/", "\\");
            QString infile = mdlfile;
            QString outName = s2contentcsgoimported + "\\" + mdlfile;
            outName.replace(".mdl", ".vmdl");
            QString refsName = s2contentcsgoimported + "\\" + mdlfile;
            refsName.replace(".mdl", "_refs.txt");

            QStringList importArgs;
            importArgs << "-nop4";
            if (!extraoptions.isEmpty()) {
                importArgs << extraoptions;
            }
            importArgs << "-i" << s1gamecsgo << "-o" << s2contentcsgoimported << infile;
            runCommand("cs_mdl_import", importArgs);

            if (QFile::exists(refsName)) {
                QFile refFile(refsName);
                if (refFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream inRef(&refFile);
                    while (!inRef.atEnd()) {
                        QString line = inRef.readLine().trimmed();
                        if (!line.isEmpty()) {
                            mdlmtls.insert(line);
                        }
                    }
                    refFile.close();
                }
                force2UVListFiles.append(refsName);
            }
        }
    }

    QString temp_refs = filename;
    temp_refs.replace("mdl_lst", "mtl_lst");
    QFile tempRefsFile(temp_refs);
    if (tempRefsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tempRefsFile);
        for (const QString& mtl : mdlmtls) {
            out << mtl << "\n";
        }
        tempRefsFile.close();
    }

    QStringList importRefsArgs;
    importRefsArgs << "-retail" << "-nop4" << "-nop4sync"
                   << "-src1gameinfodir" << s1gamecsgo
                   << "-s2addon" << s2addon
                   << "-game" << "csgo"
                   << "-usefilelist" << temp_refs;
    runCommand("source1import", importRefsArgs);

    QSet<QString> global2UVMaterials;
    QFile listFile("source1import_2uvmateriallist.txt");
    if (listFile.exists() && listFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream inList(&listFile);
        while (!inList.atEnd()) {
            QString mtl = inList.readLine().trimmed();
            if (!mtl.isEmpty()) {
                global2UVMaterials.insert(mtl);
                forceUV2ForVMAT(mtl);
            }
        }
        listFile.close();
    }

    listFile.open(QIODevice::Append | QIODevice::Text);

    // compile materials
    for (QString mtlfile : mdlmtls) {
        if (mtlfile.startsWith("-") || mtlfile.isEmpty()) continue;
        mtlfile.replace("/", "\\");
        QString outName = s2contentcsgoimported + "\\" + mtlfile;
        outName.replace(".vmt", ".vmat");

        QStringList resCompArgs;
        resCompArgs << "-retail" << "-nop4" << "-game" << "csgo" << outName;
        runCommand("resourcecompiler", resCompArgs);
    }

    // compile models
    for (QString mdlfile : mdlfiles) {
        if (mdlfile.startsWith("-")) continue;
        mdlfile.replace("/", "\\");
        QString outName = s2contentcsgoimported + "\\" + mdlfile;
        outName.replace(".mdl", ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = s2contentcsgoimported + "\\" + mdlfile;
        refsName.replace(".mdl", "_refs.txt");

        bool bForceCompile = force2UVsIfRequired(refsName, global2UVMaterials, listFile);

        QStringList resCompArgs;
        resCompArgs << "-retail" << "-nop4";
        if (bForceCompile) {
            resCompArgs << "-f";
        }
        resCompArgs << "-game" << "csgo" << outName;
        runCommand("resourcecompiler", resCompArgs);
    }

    listFile.close();
}

void MapImporter::importAndCompileMapRefs(const QString& refsFile)
{
    QStringList importCmdArgs;
    importCmdArgs << "-retail" << "-nop4" << "-nop4sync"
                  << "-src1gameinfodir" << s1gamecsgo
                  << "-s2addon" << s2addon
                  << "-game" << "csgo"
                  << "-usefilelist" << refsFile;
    runCommand("source1import", importCmdArgs);

    QFile file(refsFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString newList = "";
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            line.replace(".vmt", ".vmat");
            line.replace(" ", "_");
            line.replace("/", "\\");
            newList += s2contentcsgoimported + "\\" + line + "\n";
        }
    }
    file.close();

    QString prefabMapname = mapname;
    prefabMapname.replace("instances", "prefabs");

    QString tmpFile = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_compile_new_refs.txt";
    QFile tempFile(tmpFile);
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tempFile);
        out << newList;
        tempFile.close();
    }

    QStringList compArgs;
    compArgs << "-retail" << "-nop4" << "-game" << "csgo" << "-f" << "-filelist" << tmpFile;
    runCommand("resourcecompiler", compArgs);
}

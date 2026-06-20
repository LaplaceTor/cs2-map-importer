#pragma once

#include "Miscellaneous.h"
#include <QStringList>

class VmfBspProcess {
public:
    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void ProcessBsp(Miscellaneous::Options& options);

    static void FixVmfFromBsp(const QString& vmf_path);
    static void FixSpecialTargetnames(const QString& vmf_path);

private:
    static QString ParseMapversion(const QStringList& lines, bool& found);
    static QStringList ExtractVisgroups(const QStringList& lines, QStringList& remaining_lines);
    static QStringList InsertRequiredBlocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups);
    static QStringList PatchDispinfo(const QStringList& lines);
};

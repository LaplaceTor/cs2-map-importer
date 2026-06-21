#pragma once

#include "Miscellaneous.h"
#include <QStringList>

class VmfBspProcess {
public:
    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void ProcessBsp(Miscellaneous::Options& options);

    static void FixVmfFromBsp(const QString& vmfPath);
    static void FixSpecialTargetnames(const QString& vmfPath);
    static void FixLightColor(const QString& vmfPath);
    static void FixEntities(const QString& vmfPath);

private:
    static QString ParseMapversion(const QStringList& lines, bool& found);
    static QStringList ExtractVisgroups(const QStringList& lines, QStringList& remainingLines);
    static QStringList InsertRequiredBlocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups);
    static QStringList PatchDispinfo(const QStringList& lines);
};

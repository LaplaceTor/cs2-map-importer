#pragma once

#include "appcore.h"
#include <QStringList>

class VmfBspProcess {
public:
    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void process_bsp(AppCore::Options& options);

    static void fix_vmf_from_bsp(const QString& vmf_path, AppCore::LogCallback log);
    static void fix_special_targetnames(const QString& vmf_path, AppCore::LogCallback log);

private:
    static QString parse_mapversion(const QStringList& lines, bool& found);
    static QStringList extract_visgroups(const QStringList& lines, QStringList& remaining_lines);
    static QStringList insert_required_blocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups);
    static QStringList patch_dispinfo(const QStringList& lines);
};

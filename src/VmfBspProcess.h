#pragma once

#include "Miscellaneous.h"
#include <QStringList>

class VmfBspProcess {
public:
    struct FolderInfo {
        QString path;
        QStringList subfolders;
        QStringList files;
    };

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void ProcessBsp();

    // Extracts embedded files from BSP using detailed logging
    static void ExtractEmbeddedFiles(const QString& vpkeditcli_exe, const QString& bspFile, const QString& targetUnpackedDir);

    static void FixVmfFromBsp(const QString& vmfPath);
    static void FixSpecialTargetnames(const QString& vmfPath);
    static void FixLightColor(const QString& vmfPath);
    static void FixBrush(const QString& vmfPath);
    static void FixRender(const QString& vmfPath);
    static void FixDynamicProp(const QString& vmfPath);
    static void FixPerformanceMode(const QString& vmfPath);
    static void SkinKVFix(const QString& vmfPath);
    static void FixEntities(const QString& vmfPath);
    static void OldParticleFix(const QString& vmfPath);
    static void FixPhysboxMultiplayer(const QString& vmfPath);
    static void RemoveSkipAndHintSolids(const QString& vmfPath);

private:
    struct VmfNode {
        bool isBlock = false;
        QString name;
        QString rawLine;
        QList<VmfNode*> children;
        QString openBrace;
        QString closeBrace;

        ~VmfNode() {
            qDeleteAll(children);
        }
    };

    enum VmfContext {
        ContextRoot,
        ContextWorld,
        ContextFuncDetail,
        ContextOtherEntity
    };

    static VmfNode* ParseVmfTree(const QStringList& lines);
    static QString GetVmfKeyValue(const QString& rawLine, const QString& key);
    static bool IsSkipOrHintMaterial(const QString& material);
    static bool HasSkipOrHint(const VmfNode* node);
    static QString ReplaceMaterialLine(const QString& rawLine, const QString& newMaterial);
    static void ModifySolidMaterials(VmfNode* node);
    static void ProcessVmfTree(VmfNode* node, VmfContext context);
    static void SerializeVmfTree(const VmfNode* node, QStringList& out_lines);

    static QString ParseMapversion(const QStringList& lines, bool& found);
    static QStringList ExtractVisgroups(const QStringList& lines, QStringList& remainingLines);
    static QStringList InsertRequiredBlocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups);
    static QStringList PatchDispinfo(const QStringList& lines);
};

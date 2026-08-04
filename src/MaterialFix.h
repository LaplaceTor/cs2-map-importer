#ifndef MATERIALFIX_H
#define MATERIALFIX_H

#include <QString>
#include <QSet>
class MaterialFix {
public:
    static void Force2UVsIfRequired(const QStringList& force2UVMaterials);
    static void SkyboxFix();
    static void ColorFix(QStringList& lines, int layer0StartIdx, int& layer0EndIdx, const QMap<QString, QString>& foundLegacyKeys, bool& fileModified);
    static void ShaderFix(QStringList& lines, bool& fileModified);
    static void ComplexShaderVariablesFix(QStringList& lines, bool& fileModified);
    static void MissingKVFix(QStringList& lines, bool& fileModified);
    static void TranslucentAlphaTestConflictFix(QStringList& lines, bool& fileModified);
    static void OverlayFix();
    static void FixMaterials();
    static void OldParticleMtlFix();
};

#endif // MATERIALFIX_H

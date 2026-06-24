#ifndef MATERIALFIX_H
#define MATERIALFIX_H

#include <QString>
#include <QSet>
class MaterialFix {
public:
    static bool Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials);
    static void SkyboxFix();
};

#endif // MATERIALFIX_H

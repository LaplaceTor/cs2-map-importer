#ifndef MATERIALFIX_H
#define MATERIALFIX_H

#include <QString>
#include <QSet>
#include "MapImporter.h"

class MaterialFix {
public:
    static bool Force2UVsIfRequired(const MapImporter::Options& options, const QString& refsName, QSet<QString>& global2UVMaterials);
    static void SkyboxFix(const MapImporter::Options& options);
};

#endif // MATERIALFIX_H

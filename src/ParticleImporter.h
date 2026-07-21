#ifndef PARTICLEIMPORTER_H
#define PARTICLEIMPORTER_H

#include <QString>

class ParticleImporter {
public:
    ParticleImporter() {}

    bool Run(const QString& pcfPath);
};

#endif // PARTICLEIMPORTER_H

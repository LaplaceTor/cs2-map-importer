#pragma once

#include <cstddef>
#include <QByteArray>
#include <QLatin1StringView>
#include <QString>
#include <QStringList>

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Bsp/BspDecompileConfig.h"

namespace Domain::Bsp {

struct BspDecompileResult {
    Core::Path::FilesystemPath outputVmfPath;
    int bspVersion = 0;
    int detectedAppId = 0;
    QString detectedGameName;
    bool isProtected = false;
    QStringList protectionMethods;
    std::size_t extractedEmbeddedFiles = 0;
};

class BspDecompiler {
public:
    static constexpr QLatin1StringView kVersion{"1.4.9-cpp"};

    static Core::Result<BspDecompileResult> decompile(
        const Core::Path::FilesystemPath& bspPath,
        const Core::Path::FilesystemPath& outputVmfPath,
        const BspDecompileConfig& config = {},
        const Core::Path::FilesystemPath& expectedBaseDir = {});

    static Core::Result<QByteArray> decompileToBuffer(
        const Core::Path::FilesystemPath& bspPath,
        const BspDecompileConfig& config = {},
        BspDecompileResult* outMetadata = nullptr);
};

} // namespace Domain::Bsp

#pragma once

#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

class QImage;

namespace Domain::Material {

/**
 * @brief Minimal TGA (Truevision Targa v2.0) reader.
 *
 * Self-contained implementation of the public TGA 2.0 file specification
 * (no third-party codec): uncompressed and RLE-compressed true-color
 * (24/32 bpp) and grayscale (8 bpp) images. Read-only by design: texture
 * import accepts TGA while image export is restricted to PNG.
 */
class TgaCodec {
public:
    /**
     * @brief Reads a TGA file into a QImage (RGBA8888, or Grayscale8 for
     *        grayscale sources). Bottom-up sources are flipped to top-down.
     */
    static Core::Result<QImage> read(const Core::Path::FilesystemPath& path);

    static bool isTgaExtension(const QString& lowerCaseExtension);
};

} // namespace Domain::Material

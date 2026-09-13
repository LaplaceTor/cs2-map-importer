#pragma once

#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

class QImage;

namespace Domain::Material {

/**
 * @brief Minimal TGA (Truevision Targa v2.0) reader/writer.
 *
 * Self-contained implementation of the public TGA 2.0 file specification
 * (no third-party codec): uncompressed and RLE-compressed true-color
 * (24/32 bpp) and grayscale (8 bpp) images. Files are written top-down,
 * 32-bit uncompressed, matching what downstream game tooling expects.
 */
class TgaCodec {
public:
    /**
     * @brief Reads a TGA file into a QImage (RGBA8888, or Grayscale8 for
     *        grayscale sources). Bottom-up sources are flipped to top-down.
     */
    static Core::Result<QImage> read(const Core::Path::FilesystemPath& path);

    /**
     * @brief Writes a QImage (RGBA8888 / RGB888 / Grayscale8) as an
     *        uncompressed top-down TGA file.
     */
    static Core::Result<void> write(const Core::Path::FilesystemPath& path, const QImage& image);

    static bool isTgaExtension(const QString& lowerCaseExtension);

private:
    static Core::Error::ErrorCode tgaErrorForIoFailure();
};

} // namespace Domain::Material

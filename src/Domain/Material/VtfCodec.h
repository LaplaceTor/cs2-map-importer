#pragma once

#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

class QImage;

namespace Domain::Material {

/**
 * @brief Native C++20 read-only VTF (Valve Texture Format v7.0-v7.6) decoder.
 *
 * Decodes mip 0 (highest resolution), first frame, first face, and first depth
 * slice of the container; compressed storage formats (DXT/BC1-BC5) and HDR
 * formats are decoded to RGBA8888 natively. VTF output is intentionally out
 * of scope: image export is PNG-only.
 */
class VtfCodec {
public:
    /**
     * @brief Reads a VTF file into a QImage (RGBA8888).
     */
    static Core::Result<QImage> read(const Core::Path::FilesystemPath& path);

    static bool isVtfExtension(const QString& lowerCaseExtension);
};

} // namespace Domain::Material

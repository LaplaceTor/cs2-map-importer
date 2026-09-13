#pragma once

#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

class QImage;

namespace Domain::Material {

/**
 * @brief Read-only VTF (Valve Texture Format) decoder backed by the vendored
 *        sourcepp vtfpp library.
 *
 * Decodes mip 0 (highest resolution), first frame and first face of the
 * container; compressed storage formats (DXT/BCn) are decoded to RGBA8888 by
 * vtfpp. VTF output is intentionally out of scope: image export is PNG-only.
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

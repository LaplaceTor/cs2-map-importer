#pragma once

#include <vector>

#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace Domain::Material {

/**
 * @brief Output image file formats supported for VTF decoding.
 */
enum class ImageFileFormat {
    Png,
    Tga,
    Jpg,
    Bmp,
    Hdr,
};

/**
 * @brief Decodes Valve Texture Format (VTF) files into standard image data.
 *
 * Wraps vtfpp for VTF parsing and image encoding. The highest resolution mip
 * (mip 0) of the first frame/face is converted to the requested image format
 * (e.g. PNG, TGA, JPG, BMP, HDR). All results follow the Core::Result contract.
 */
class VtfConverter {
public:
    /**
     * @brief Returns the standard file extension for the given image format (without dot).
     */
    static QString formatExtension(ImageFileFormat format);

    /**
     * @brief Decodes a VTF file and encodes the image into memory using the target format.
     */
    static Core::Result<std::vector<std::byte>> convertToImageBuffer(
        const Core::Path::FilesystemPath& vtfPath,
        ImageFileFormat format = ImageFileFormat::Png);

    /**
     * @brief Decodes a VTF file and writes the converted image to the destination path.
     */
    static Core::Result<void> convertToImageFile(
        const Core::Path::FilesystemPath& vtfPath,
        const Core::Path::FilesystemPath& destImagePath,
        ImageFileFormat format = ImageFileFormat::Png);

};

} // namespace Domain::Material


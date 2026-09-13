#pragma once

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material {

/**
 * @brief Loads and writes plain image files (png / jpg / bmp / tga) as
 *        TextureImage working buffers.
 *
 * PNG/JPG/BMP decoding uses Qt's built-in image plugins; TGA uses the
 * self-contained TgaCodec. Color-space handling is explicit at the call site:
 * color inputs are loaded sRGB-decoded into linear floats (matching GPU
 * sampling behavior), while data maps (height, normal, AO...) travel raw.
 */
class TextureIO {
public:
    /**
     * @brief Loads an image file into a 4-channel planar float buffer.
     *
     * @param srgbDecode when true, applies the sRGB transfer function to the
     *        RGB channels (source files are sRGB-encoded containers); when
     *        false, byte values are mapped to [0, 1] verbatim.
     */
    static Core::Result<TextureImage> loadTexture(const Core::Path::FilesystemPath& path,
                                                  bool srgbDecode = true);

    /**
     * @brief Writes a TextureImage to a png or tga file (extension decides).
     *
     * @param srgbEncode when true, applies the inverse sRGB transfer function
     *        before quantization (for color-like outputs); data maps are
     *        written with false so stored bytes equal the raw values.
     */
    static Core::Result<void> writeTexture(const Core::Path::FilesystemPath& path,
                                           const TextureImage& image,
                                           bool srgbEncode = true);

    static bool isSupportedLoadExtension(const QString& lowerCaseExtension);
    static bool isSupportedWriteExtension(const QString& lowerCaseExtension);
};

} // namespace Domain::Material

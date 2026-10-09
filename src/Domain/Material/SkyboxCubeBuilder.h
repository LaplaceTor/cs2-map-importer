#pragma once

#include <array>
#include <vector>

#include <QImage>
#include <QString>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

#include "Domain/Material/SkyboxTypes.h"

namespace Domain::Material {

/**
 * @brief Constructs a Source 2 4x3 horizontal cross cubemap and .vmat definition
 *        from Source 1 skybox face textures using in-process native processing,
 *        FT-centered topology, and a two-pass edge pixel alignment algorithm with
 *        12-seam closed-loop fault isolation.
 */
class SkyboxCubeBuilder {
public:
    /**
     * @brief Stitches the given 6 faces into a 4x3 horizontal cross image and
     *        generates the corresponding .vmat KeyValues string.
     */
    static Core::Result<SkyboxBuildResult> build(
        const SkyboxFaces& faces,
        const SkyboxBuildOptions& options = {});

    /**
     * @brief Loads skybox face textures matching the given baseName from directory.
     *        e.g. baseName "cs_baggage_skybox_" looks for *ft.vtf, *bk.vtf, etc.
     */
    static Core::Result<SkyboxFaces> loadFacesFromDirectory(
        const Core::Path::FilesystemPath& directory,
        const QString& baseName);

    /**
     * @brief Formats a valid Source 2 .vmat KeyValues string pointing to sky.vfx
     *        and the specified SkyTexture path.
     */
    static QString generateVmat(const QString& skyTexturePath);

    /**
     * @brief Rotates an image clockwise by an orthogonal angle (0, 90, 180, 270 deg).
     */
    static QImage rotateImage(const QImage& src, Rotation rotation);

    /**
     * @brief Extracts boundary pixels along a specified edge of a face image.
     */
    static std::vector<QRgb> extractEdgePixels(const QImage& img, FaceEdge edge);

    /**
     * @brief Computes luminance variance along a sequence of edge pixels.
     */
    static double computeEdgeVariance(const std::vector<QRgb>& edgePixels);

    /**
     * @brief Computes Sum of Absolute Differences (SAD) across RGB channels for two edges.
     */
    static double computeSeamSad(
        const std::vector<QRgb>& pixelsA,
        const std::vector<QRgb>& pixelsB,
        bool reversed = false);

    /**
     * @brief Computes mean absolute error per pixel per channel (0..255) for two edges.
     */
    static double computeSeamMeanError(
        const std::vector<QRgb>& pixelsA,
        const std::vector<QRgb>& pixelsB,
        bool reversed = false);

    /**
     * @brief Computes normalized ColorCost in [0, 1] using RGB absolute differences.
     */
    static double computeColorCost(
        const std::vector<QRgb>& pixelsA,
        const std::vector<QRgb>& pixelsB,
        bool reversed = false);

    /**
     * @brief Computes normalized GradientCost in [0, 1] using 1D discrete forward differences.
     */
    static double computeGradientCost(
        const std::vector<QRgb>& pixelsA,
        const std::vector<QRgb>& pixelsB,
        bool reversed = false);

    /**
     * @brief Computes weighted combined SeamCost (w_c * ColorCost + w_g * GradientCost).
     */
    static double computeSeamCost(
        const std::vector<QRgb>& pixelsA,
        const std::vector<QRgb>& pixelsB,
        bool reversed = false,
        double colorWeight = 0.8,
        double gradientWeight = 0.2);

    /**
     * @brief Evaluates all 12 physical cube seams against current candidate rotations.
     */
    static std::vector<SeamEvaluation> evaluateAllSeams(
        const SkyboxFaces& faces,
        const std::array<Rotation, CubeFaceCount>& rotations,
        double mismatchThreshold);

    /**
     * @brief Performs global 1024-state exhaustive optimization anchored at FT = Deg0.
     */
    static std::array<Rotation, CubeFaceCount> optimizeRotations(
        const SkyboxFaces& faces,
        const SkyboxBuildOptions& options,
        double& outBestCost,
        double& outSecondBestCost,
        double& outMargin,
        bool& outAmbiguous);

    /**
     * @brief Normalizes a face image to resolution x resolution square in Format_RGBA8888.
     *        Rejects non-square, invalid, or null images by returning an empty QImage when
     *        allowNonSquare is false. When true, non-square images are scaled to square.
     */
    static QImage ensureNormalizedFace(const QImage& img, int resolution, bool allowNonSquare = false);

    /**
     * @brief Blits a face image onto the 4x3 canvas with strict bounds and format checks.
     */
    static void copyFaceToCanvas(QImage& canvas, const QImage& face, int col, int row, int resolution);

    /**
     * @brief Composes the final 4x3 horizontal cross image (4S x 3S).
     */
    static QImage composeCubeImage(
        const SkyboxFaces& faces,
        const std::array<Rotation, CubeFaceCount>& rotations,
        int faceResolution,
        bool allowNonSquare = false);
};

} // namespace Domain::Material

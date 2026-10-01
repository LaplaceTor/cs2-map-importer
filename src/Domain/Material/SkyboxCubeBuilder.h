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
     * @brief Pass 1: Initial local alignment with FT anchored at 0 deg.
     */
    static std::array<Rotation, CubeFaceCount> alignPass1(
        const SkyboxFaces& faces,
        const SkyboxBuildOptions& options,
        bool& fallbackUsed);

    /**
     * @brief Evaluates all 12 physical cube seams against current candidate rotations.
     */
    static std::vector<SeamEvaluation> evaluateAllSeams(
        const SkyboxFaces& faces,
        const std::array<Rotation, CubeFaceCount>& rotations,
        double mismatchThreshold);

    /**
     * @brief Analyzes fault status for each face according to mismatched edge count M_F:
     *        M_F >= 2: misoriented face;
     *        M_F == 1: correctly oriented face, neighbor connected to failing edge isolated;
     *        M_F == 0: ground truth.
     */
    static std::array<FaceFaultStatus, CubeFaceCount> analyzeFaultStatus(
        const std::vector<SeamEvaluation>& seamEvals,
        const SkyboxFaces* faces = nullptr);

    /**
     * @brief Pass 2: Global seam consistency check & fault isolation loop.
     */
    static std::array<Rotation, CubeFaceCount> alignPass2(
        const SkyboxFaces& faces,
        const std::array<Rotation, CubeFaceCount>& pass1Rotations,
        const SkyboxBuildOptions& options,
        std::vector<SeamEvaluation>& outSeamEvals,
        std::array<FaceFaultStatus, CubeFaceCount>& outFaultStatus,
        bool* outFallbackUsed = nullptr);

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

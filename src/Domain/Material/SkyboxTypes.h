#pragma once

#include <array>
#include <optional>
#include <vector>

#include <QImage>
#include <QString>

namespace Domain::Material {

/**
 * @brief Identifies the six canonical faces of a 3D cubemap/skybox.
 */
enum class CubeFace : int {
    Front = 0,
    Back,
    Left,
    Right,
    Up,
    Down
};

constexpr int CubeFaceCount = 6;

/**
 * @brief Discrete rigid-body clockwise orthogonal rotations.
 * Mirroring and non-orthogonal rotations are prohibited due to chirality constraints.
 */
enum class Rotation : int {
    Deg0 = 0,
    Deg90 = 90,
    Deg180 = 180,
    Deg270 = 270
};

inline Rotation combineRotations(Rotation a, Rotation b) noexcept {
    return static_cast<Rotation>((static_cast<int>(a) + static_cast<int>(b)) % 360);
}

inline int rotationDegrees(Rotation r) noexcept {
    return static_cast<int>(r);
}

inline QString cubeFaceSuffix(CubeFace face) {
    switch (face) {
    case CubeFace::Front: return QStringLiteral("ft");
    case CubeFace::Back:  return QStringLiteral("bk");
    case CubeFace::Left:  return QStringLiteral("lf");
    case CubeFace::Right: return QStringLiteral("rt");
    case CubeFace::Up:    return QStringLiteral("up");
    case CubeFace::Down:  return QStringLiteral("dn");
    }
    return QString();
}

inline QString cubeFaceName(CubeFace face) {
    switch (face) {
    case CubeFace::Front: return QStringLiteral("Front (ft)");
    case CubeFace::Back:  return QStringLiteral("Back (bk)");
    case CubeFace::Left:  return QStringLiteral("Left (lf)");
    case CubeFace::Right: return QStringLiteral("Right (rt)");
    case CubeFace::Up:    return QStringLiteral("Up (up)");
    case CubeFace::Down:  return QStringLiteral("Down (dn)");
    }
    return QString();
}

inline std::optional<CubeFace> cubeFaceFromSuffix(const QString& suffix) {
    const QString s = suffix.toLower();
    if (s == QLatin1String("ft") || s == QLatin1String("front")) return CubeFace::Front;
    if (s == QLatin1String("bk") || s == QLatin1String("back"))  return CubeFace::Back;
    if (s == QLatin1String("lf") || s == QLatin1String("left"))  return CubeFace::Left;
    if (s == QLatin1String("rt") || s == QLatin1String("right")) return CubeFace::Right;
    if (s == QLatin1String("up")) return CubeFace::Up;
    if (s == QLatin1String("dn") || s == QLatin1String("down"))  return CubeFace::Down;
    return std::nullopt;
}

/**
 * @brief 2D boundary edges of a square face image.
 */
enum class FaceEdge : int {
    Top = 0,
    Bottom,
    Left,
    Right
};

/**
 * @brief Mathematical seam connecting two cube faces in 3D space.
 */
struct SeamDefinition {
    CubeFace faceA;
    FaceEdge edgeA;
    CubeFace faceB;
    FaceEdge edgeB;
    bool reversed = false;
};

constexpr int CubeSeamCount = 12;

/**
 * @brief Returns the 12 canonical seams of a 3D cube mesh.
 */
inline const std::array<SeamDefinition, CubeSeamCount>& getCubeSeams() noexcept {
    static const std::array<SeamDefinition, CubeSeamCount> seams = {{
        // 1. FT - UP: FT Top <-> UP Bottom
        { CubeFace::Front, FaceEdge::Top,    CubeFace::Up,    FaceEdge::Bottom, false },
        // 2. FT - DN: FT Bottom <-> DN Top
        { CubeFace::Front, FaceEdge::Bottom, CubeFace::Down,  FaceEdge::Top,    false },
        // 3. FT - RT: FT Left <-> RT Right (Inside-out: RT is left of FT)
        { CubeFace::Front, FaceEdge::Left,   CubeFace::Right, FaceEdge::Right,  false },
        // 4. FT - LF: FT Right <-> LF Left (Inside-out: LF is right of FT)
        { CubeFace::Front, FaceEdge::Right,  CubeFace::Left,  FaceEdge::Left,   false },
        // 5. RT - BK: RT Left <-> BK Right
        { CubeFace::Right, FaceEdge::Left,   CubeFace::Back,  FaceEdge::Right,  false },
        // 6. LF - BK: LF Right <-> BK Left
        { CubeFace::Left,  FaceEdge::Right,  CubeFace::Back,  FaceEdge::Left,   false },
        // 7. UP - RT: UP Left <-> RT Top
        { CubeFace::Up,    FaceEdge::Left,   CubeFace::Right, FaceEdge::Top,    false },
        // 8. UP - LF: UP Right <-> LF Top (reversed)
        { CubeFace::Up,    FaceEdge::Right,  CubeFace::Left,  FaceEdge::Top,    true  },
        // 9. UP - BK: UP Top <-> BK Top (reversed)
        { CubeFace::Up,    FaceEdge::Top,    CubeFace::Back,  FaceEdge::Top,    true  },
        // 10. DN - RT: DN Left <-> RT Bottom (reversed)
        { CubeFace::Down,  FaceEdge::Left,   CubeFace::Right, FaceEdge::Bottom, true  },
        // 11. DN - LF: DN Right <-> LF Bottom
        { CubeFace::Down,  FaceEdge::Right,  CubeFace::Left,  FaceEdge::Bottom, false },
        // 12. DN - BK: DN Bottom <-> BK Bottom (reversed)
        { CubeFace::Down,  FaceEdge::Bottom, CubeFace::Back,  FaceEdge::Bottom, true  }
    }};
    return seams;
}

/**
 * @brief Container holding the 6 face images for skybox conversion.
 */
struct SkyboxFaces {
    QImage ft;
    QImage bk;
    QImage lf;
    QImage rt;
    QImage up;
    QImage dn;

    const QImage& face(CubeFace f) const noexcept {
        switch (f) {
        case CubeFace::Front: return ft;
        case CubeFace::Back:  return bk;
        case CubeFace::Left:  return lf;
        case CubeFace::Right: return rt;
        case CubeFace::Up:    return up;
        case CubeFace::Down:  return dn;
        }
        return ft;
    }

    QImage& face(CubeFace f) noexcept {
        switch (f) {
        case CubeFace::Front: return ft;
        case CubeFace::Back:  return bk;
        case CubeFace::Left:  return lf;
        case CubeFace::Right: return rt;
        case CubeFace::Up:    return up;
        case CubeFace::Down:  return dn;
        }
        return ft;
    }

    bool hasFace(CubeFace f) const noexcept {
        const QImage& img = face(f);
        return !img.isNull() && img.width() > 0 && img.height() > 0;
    }

    bool isFaceSquare(CubeFace f) const noexcept {
        const QImage& img = face(f);
        return hasFace(f) && img.width() == img.height();
    }

    bool hasAnyFace() const noexcept {
        return hasFace(CubeFace::Front) || hasFace(CubeFace::Back) ||
               hasFace(CubeFace::Left)  || hasFace(CubeFace::Right) ||
               hasFace(CubeFace::Up)    || hasFace(CubeFace::Down);
    }

    bool hasAllFaces() const noexcept {
        return hasFace(CubeFace::Front) && hasFace(CubeFace::Back) &&
               hasFace(CubeFace::Left)  && hasFace(CubeFace::Right) &&
               hasFace(CubeFace::Up)    && hasFace(CubeFace::Down);
    }

    int faceCount() const noexcept {
        int count = 0;
        if (hasFace(CubeFace::Front)) count++;
        if (hasFace(CubeFace::Back))  count++;
        if (hasFace(CubeFace::Left))  count++;
        if (hasFace(CubeFace::Right)) count++;
        if (hasFace(CubeFace::Up))    count++;
        if (hasFace(CubeFace::Down))  count++;
        return count;
    }

    void setFace(CubeFace f, QImage img) {
        face(f) = std::move(img);
    }
};

/**
 * @brief Build options for the skybox cube generator.
 */
struct SkyboxBuildOptions {
    int faceResolution = 0; // 0 = auto-detect native resolution from input faces, defaults to 1024
    double confidenceThreshold = 0.15;
    double varianceThreshold = 10.0;
    double seamMismatchThreshold = 25.0; // Mean absolute difference per pixel per channel
    bool allowNonSquareFaces = false; // When true, non-square faces (e.g. Source 1 512x256 side faces) are automatically scaled to square resolution
    QString skyboxName;
    QString skyTextureRelativePath;
};

/**
 * @brief Evaluation detail of an individual seam.
 */
struct SeamEvaluation {
    int seamIndex = -1;
    CubeFace faceA = CubeFace::Front;
    FaceEdge edgeA = FaceEdge::Top;
    CubeFace faceB = CubeFace::Up;
    FaceEdge edgeB = FaceEdge::Bottom;
    double error = 0.0; // Mean difference per pixel per channel (0..255)
    bool mismatched = false;
    bool evaluated = false; // False if either face is missing
};

/**
 * @brief Fault status for an individual face in Pass 2.
 */
struct FaceFaultStatus {
    CubeFace face = CubeFace::Front;
    int mismatchedEdgeCount = 0; // M_F
    std::vector<CubeFace> connectedFailingNeighbors;
    bool isMisoriented = false;   // M_F >= 2
    bool isFaultIsolated = false; // M_F == 1
    bool isGroundTruth = false;   // M_F == 0
};

/**
 * @brief Result containing the 4x3 stitched cube image and metadata.
 */
struct SkyboxBuildResult {
    QImage cubeImage;
    QString vmatContent;
    std::array<Rotation, CubeFaceCount> faceRotations = {
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0,
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0
    };
    std::array<FaceFaultStatus, CubeFaceCount> faultStatuses;
    std::vector<SeamEvaluation> seamEvaluations;
    bool pass1Success = false;
    bool pass2Success = false;
    bool fallbackUsed = false;
};

} // namespace Domain::Material

#include "Domain/Material/SkyboxCubeBuilder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include <QCoreApplication>
#include <QFileInfo>

#include "Domain/Material/TextureIO.h"

namespace Domain::Material {

QImage SkyboxCubeBuilder::ensureNormalizedFace(const QImage& img, int resolution, bool allowNonSquare)
{
    if (img.isNull() || img.width() <= 0 || img.height() <= 0 || resolution <= 0 || resolution > 8192) {
        return QImage();
    }
    if (!allowNonSquare && img.width() != img.height()) {
        return QImage();
    }
    QImage formatted = img.format() == QImage::Format_RGBA8888
                           ? img
                           : img.convertToFormat(QImage::Format_RGBA8888);
    if (formatted.isNull()) {
        return QImage();
    }
    if (formatted.width() != resolution || formatted.height() != resolution) {
        return formatted.scaled(resolution, resolution, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return formatted;
}

void SkyboxCubeBuilder::copyFaceToCanvas(QImage& canvas, const QImage& face, int col, int row, int resolution)
{
    if (resolution <= 0 || face.isNull() || face.width() != resolution || face.height() != resolution ||
        face.format() != QImage::Format_RGBA8888 || canvas.format() != QImage::Format_RGBA8888) {
        return;
    }
    const int startX = col * resolution;
    const int startY = row * resolution;
    if (startX < 0 || startY < 0 || startX + resolution > canvas.width() || startY + resolution > canvas.height()) {
        return;
    }
    for (int y = 0; y < resolution; ++y) {
        const auto* srcRow = reinterpret_cast<const quint32*>(face.constScanLine(y));
        auto* dstRow = reinterpret_cast<quint32*>(canvas.scanLine(startY + y));
        std::copy_n(srcRow, resolution, dstRow + startX);
    }
}

Core::Result<SkyboxBuildResult> SkyboxCubeBuilder::build(
    const SkyboxFaces& faces,
    const SkyboxBuildOptions& options)
{
    if (!faces.hasAnyFace()) {
        return Core::Result<SkyboxBuildResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SkyboxCubeBuilder", "no skybox faces provided"));
    }

    const std::array<CubeFace, CubeFaceCount> allFaces = {
        CubeFace::Front, CubeFace::Back, CubeFace::Left,
        CubeFace::Right, CubeFace::Up,   CubeFace::Down
    };

    // Validate that all present faces are square to prevent aspect ratio distortion (unless allowNonSquareFaces is enabled)
    if (!options.allowNonSquareFaces) {
        for (CubeFace f : allFaces) {
            if (faces.hasFace(f)) {
                const auto& img = faces.face(f);
                if (img.width() != img.height()) {
                    return Core::Result<SkyboxBuildResult>::failure(
                        Core::Error::ErrorCode::InvalidArgument,
                        QCoreApplication::translate("SkyboxCubeBuilder", "skybox face image must be square"),
                        QStringLiteral("%1 (%2x%3)").arg(cubeFaceName(f)).arg(img.width()).arg(img.height()));
                }
            }
        }
    }

    int res = options.faceResolution;
    if (res < 0) {
        return Core::Result<SkyboxBuildResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SkyboxCubeBuilder", "face resolution cannot be negative"));
    }

    if (res == 0) {
        for (CubeFace f : allFaces) {
            if (faces.hasFace(f)) {
                res = std::max(res, std::max(faces.face(f).width(), faces.face(f).height()));
            }
        }
    }
    if (res <= 0) {
        res = 1024;
    }

    if (res > 8192) {
        return Core::Result<SkyboxBuildResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SkyboxCubeBuilder", "face resolution exceeds maximum supported size (8192)"));
    }

    // Normalize present faces to resolution x resolution, Format_RGBA8888
    SkyboxFaces normalizedFaces;
    for (CubeFace f : allFaces) {
        if (faces.hasFace(f)) {
            QImage norm = ensureNormalizedFace(faces.face(f), res, options.allowNonSquareFaces);
            if (norm.isNull()) {
                return Core::Result<SkyboxBuildResult>::failure(
                    Core::Error::ErrorCode::OperationFailed,
                    QCoreApplication::translate("SkyboxCubeBuilder", "failed to normalize skybox face image"),
                    cubeFaceName(f));
            }
            normalizedFaces.setFace(f, std::move(norm));
        }
    }

    SkyboxBuildResult result;
    bool fallbackUsed = false;

    // Pass 1: Initial local alignment
    result.faceRotations = alignPass1(normalizedFaces, options, fallbackUsed);
    result.pass1Success = true;
    result.fallbackUsed = fallbackUsed;

    if (!normalizedFaces.hasFace(CubeFace::Front)) {
        // R2: If FT is missing, fall back to geometric priors
        result.seamEvaluations = evaluateAllSeams(
            normalizedFaces, result.faceRotations, options.seamMismatchThreshold);
        result.faultStatuses = analyzeFaultStatus(result.seamEvaluations, &normalizedFaces);
        result.pass2Success = true;
        result.fallbackUsed = true;
    } else {
        // Pass 2: Global seam consistency check and fault isolation
        result.faceRotations = alignPass2(
            normalizedFaces,
            result.faceRotations,
            options,
            result.seamEvaluations,
            result.faultStatuses,
            &result.fallbackUsed);
        result.pass2Success = true;
    }

    // Canvas composition (4S x 3S)
    result.cubeImage = composeCubeImage(normalizedFaces, result.faceRotations, res, options.allowNonSquareFaces);
    if (result.cubeImage.isNull()) {
        return Core::Result<SkyboxBuildResult>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("SkyboxCubeBuilder", "failed to allocate cube canvas"));
    }

    // .vmat generation
    QString skyTexture = options.skyTextureRelativePath;
    if (skyTexture.isEmpty()) {
        const QString name = options.skyboxName.isEmpty()
                                 ? QStringLiteral("skybox_")
                                 : options.skyboxName;
        skyTexture = QStringLiteral("materials/skybox/%1cube.png").arg(name);
    }
    result.vmatContent = generateVmat(skyTexture);

    return Core::Result<SkyboxBuildResult>::success(std::move(result));
}

Core::Result<SkyboxFaces> SkyboxCubeBuilder::loadFacesFromDirectory(
    const Core::Path::FilesystemPath& directory,
    const QString& baseName)
{
    if (directory.isEmpty() || !directory.isValid()) {
        return Core::Result<SkyboxFaces>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("SkyboxCubeBuilder", "directory path is invalid"));
    }
    if (!directory.exists() || !directory.isDirectory()) {
        return Core::Result<SkyboxFaces>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("SkyboxCubeBuilder", "directory does not exist"),
            directory.toString());
    }
    const QFileInfo dirInfo(directory.toString());
    if (!dirInfo.isReadable()) {
        return Core::Result<SkyboxFaces>::failure(
            Core::Error::ErrorCode::PermissionDenied,
            QCoreApplication::translate("SkyboxCubeBuilder", "directory permission denied"),
            directory.toString());
    }
    if (baseName.trimmed().isEmpty()) {
        return Core::Result<SkyboxFaces>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SkyboxCubeBuilder", "skybox base name cannot be empty"));
    }

    SkyboxFaces faces;
    const std::array<CubeFace, CubeFaceCount> allFaces = {
        CubeFace::Front, CubeFace::Back, CubeFace::Left,
        CubeFace::Right, CubeFace::Up,   CubeFace::Down
    };

    const QStringList extensions = {
        QStringLiteral("vtf"),
        QStringLiteral("hdr.vtf"),
        QStringLiteral("png"),
        QStringLiteral("tga"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("bmp")
    };

    QStringList prefixes;
    prefixes.append(baseName);
    if (!baseName.endsWith(QLatin1Char('_'))) {
        prefixes.append(baseName + QLatin1Char('_'));
    }

    Core::Error::Error lastReadError(Core::Error::ErrorCode::Success, QString());
    bool fileFoundOnDisk = false;
    bool anyFaceCorrupted = false;

    for (CubeFace face : allFaces) {
        const QString suffix = cubeFaceSuffix(face);
        bool faceLoaded = false;
        bool faceFileFoundOnDisk = false;
        for (const QString& prefix : prefixes) {
            if (faceLoaded) break;
            for (const QString& ext : extensions) {
                const QString filename = prefix + suffix + QStringLiteral(".") + ext;
                const auto resolvedPath = directory.resolveBelow(filename);
                if (!resolvedPath.has_value()) {
                    continue;
                }
                const Core::Path::FilesystemPath& filePath = *resolvedPath;
                if (filePath.exists() && filePath.isFile()) {
                    fileFoundOnDisk = true;
                    faceFileFoundOnDisk = true;
                    auto readRes = TextureIO::readImage(filePath);
                    if (readRes.isSuccess()) {
                        faces.setFace(face, readRes.value());
                        faceLoaded = true;
                        break;
                    } else {
                        lastReadError = readRes.error();
                    }
                }
            }
        }
        if (faceFileFoundOnDisk && !faceLoaded) {
            anyFaceCorrupted = true;
        }
    }

    if (anyFaceCorrupted && lastReadError.code() != Core::Error::ErrorCode::Success) {
        return Core::Result<SkyboxFaces>::failure(
            lastReadError.code(),
            QCoreApplication::translate("SkyboxCubeBuilder", "failed to read skybox face texture"),
            lastReadError.details().isEmpty() ? lastReadError.message() : lastReadError.details());
    }

    if (!faces.hasAnyFace()) {
        return Core::Result<SkyboxFaces>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("SkyboxCubeBuilder", "no skybox face textures found for base name"),
            baseName);
    }

    return Core::Result<SkyboxFaces>::success(std::move(faces));
}

QString SkyboxCubeBuilder::generateVmat(const QString& skyTexturePath)
{
    // Formats standard Source 2 sky.vfx KeyValues material
    return QStringLiteral("Layer0\n{\n\tshader \"sky.vfx\"\n\tSkyTexture \"%1\"\n}\n")
        .arg(skyTexturePath);
}

QImage SkyboxCubeBuilder::rotateImage(const QImage& src, Rotation rotation)
{
    if (src.isNull() || src.width() <= 0 || src.height() <= 0 || rotation == Rotation::Deg0) {
        return src;
    }
    QImage formatted = src.format() == QImage::Format_RGBA8888
                           ? src
                           : src.convertToFormat(QImage::Format_RGBA8888);
    if (formatted.isNull()) {
        return QImage();
    }
    const int w = formatted.width();
    const int h = formatted.height();
    const int dstW = (rotation == Rotation::Deg180) ? w : h;
    const int dstH = (rotation == Rotation::Deg180) ? h : w;
    QImage dst(dstW, dstH, QImage::Format_RGBA8888);
    if (dst.isNull()) {
        return QImage();
    }

    if (rotation == Rotation::Deg90) {
        for (int y = 0; y < h; ++y) {
            const auto* srcRow = reinterpret_cast<const quint32*>(formatted.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                auto* dstRow = reinterpret_cast<quint32*>(dst.scanLine(x));
                dstRow[h - 1 - y] = srcRow[x];
            }
        }
    } else if (rotation == Rotation::Deg180) {
        for (int y = 0; y < h; ++y) {
            const auto* srcRow = reinterpret_cast<const quint32*>(formatted.constScanLine(y));
            auto* dstRow = reinterpret_cast<quint32*>(dst.scanLine(h - 1 - y));
            for (int x = 0; x < w; ++x) {
                dstRow[w - 1 - x] = srcRow[x];
            }
        }
    } else if (rotation == Rotation::Deg270) {
        for (int y = 0; y < h; ++y) {
            const auto* srcRow = reinterpret_cast<const quint32*>(formatted.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                auto* dstRow = reinterpret_cast<quint32*>(dst.scanLine(w - 1 - x));
                dstRow[y] = srcRow[x];
            }
        }
    }
    return dst;
}

std::vector<QRgb> SkyboxCubeBuilder::extractEdgePixels(const QImage& img, FaceEdge edge)
{
    const int w = img.width();
    const int h = img.height();
    std::vector<QRgb> pixels;
    if (img.isNull() || w <= 0 || h <= 0) {
        return pixels;
    }
    switch (edge) {
    case FaceEdge::Top:
        pixels.reserve(w);
        for (int x = 0; x < w; ++x) {
            pixels.push_back(img.pixel(x, 0));
        }
        break;
    case FaceEdge::Bottom:
        pixels.reserve(w);
        for (int x = 0; x < w; ++x) {
            pixels.push_back(img.pixel(x, h - 1));
        }
        break;
    case FaceEdge::Left:
        pixels.reserve(h);
        for (int y = 0; y < h; ++y) {
            pixels.push_back(img.pixel(0, y));
        }
        break;
    case FaceEdge::Right:
        pixels.reserve(h);
        for (int y = 0; y < h; ++y) {
            pixels.push_back(img.pixel(w - 1, y));
        }
        break;
    }
    return pixels;
}

double SkyboxCubeBuilder::computeEdgeVariance(const std::vector<QRgb>& edgePixels)
{
    if (edgePixels.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    double sumSq = 0.0;
    const double n = static_cast<double>(edgePixels.size());
    for (QRgb p : edgePixels) {
        const double lum = 0.299 * qRed(p) + 0.587 * qGreen(p) + 0.114 * qBlue(p);
        sum += lum;
        sumSq += lum * lum;
    }
    const double mean = sum / n;
    const double variance = (sumSq / n) - (mean * mean);
    return std::max(0.0, variance);
}

double SkyboxCubeBuilder::computeSeamSad(
    const std::vector<QRgb>& pixelsA,
    const std::vector<QRgb>& pixelsB,
    bool reversed)
{
    if (pixelsA.empty() || pixelsB.empty() || pixelsA.size() != pixelsB.size()) {
        return std::numeric_limits<double>::infinity();
    }
    const std::size_t n = pixelsA.size();
    double totalSad = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const QRgb pA = pixelsA[i];
        const QRgb pB = reversed ? pixelsB[n - 1 - i] : pixelsB[i];
        totalSad += std::abs(qRed(pA) - qRed(pB))
                  + std::abs(qGreen(pA) - qGreen(pB))
                  + std::abs(qBlue(pA) - qBlue(pB));
    }
    return totalSad;
}

double SkyboxCubeBuilder::computeSeamMeanError(
    const std::vector<QRgb>& pixelsA,
    const std::vector<QRgb>& pixelsB,
    bool reversed)
{
    if (pixelsA.empty() || pixelsB.empty() || pixelsA.size() != pixelsB.size()) {
        return 255.0;
    }
    const double sad = computeSeamSad(pixelsA, pixelsB, reversed);
    return sad / (static_cast<double>(pixelsA.size()) * 3.0);
}

std::array<Rotation, CubeFaceCount> SkyboxCubeBuilder::alignPass1(
    const SkyboxFaces& faces,
    const SkyboxBuildOptions& options,
    bool& fallbackUsed)
{
    std::array<Rotation, CubeFaceCount> rotations = {
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0,
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0
    };

    // Front face is fixed anchor at 0 deg
    rotations[static_cast<int>(CubeFace::Front)] = Rotation::Deg0;

    if (!faces.hasFace(CubeFace::Front)) {
        fallbackUsed = true;
        return rotations;
    }

    const std::array<Rotation, 4> candidateRotations = {
        Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270
    };
    const auto& seams = cubeSeams();

    auto alignFaceAgainstNeighbors = [&](CubeFace faceId, const std::vector<int>& candidateSeamIndices) {
        if (!faces.hasFace(faceId)) {
            return;
        }
        const QImage& targetFace = faces.face(faceId);

        struct AlignedNeighborSeam {
            int seamIndex;
            bool isFaceA;
            std::vector<QRgb> neighborEdge;
        };
        std::vector<AlignedNeighborSeam> validSeams;
        double maxNeighborVariance = 0.0;

        for (int sIdx : candidateSeamIndices) {
            const auto& seam = seams[sIdx];
            const bool isFaceA = (seam.faceA == faceId);
            const CubeFace neighborFace = isFaceA ? seam.faceB : seam.faceA;
            const int neighborIdx = static_cast<int>(neighborFace);

            if (!faces.hasFace(neighborFace)) {
                continue;
            }

            const QImage rotNeighbor = rotateImage(faces.face(neighborFace), rotations[neighborIdx]);
            const auto neighborEdge = extractEdgePixels(
                rotNeighbor,
                isFaceA ? seam.edgeB : seam.edgeA);

            if (!neighborEdge.empty()) {
                maxNeighborVariance = std::max(maxNeighborVariance, computeEdgeVariance(neighborEdge));
                validSeams.push_back({sIdx, isFaceA, std::move(neighborEdge)});
            }
        }

        if (validSeams.empty() || maxNeighborVariance < options.varianceThreshold) {
            rotations[static_cast<int>(faceId)] = Rotation::Deg0;
            fallbackUsed = true;
            return;
        }

        std::array<double, 4> sads{};
        for (std::size_t i = 0; i < 4; ++i) {
            const QImage rotTarget = rotateImage(targetFace, candidateRotations[i]);
            double totalErr = 0.0;
            for (const auto& item : validSeams) {
                const auto& seam = seams[item.seamIndex];
                const auto targetEdge = extractEdgePixels(
                    rotTarget,
                    item.isFaceA ? seam.edgeA : seam.edgeB);
                totalErr += computeSeamMeanError(targetEdge, item.neighborEdge, seam.reversed);
            }
            sads[i] = totalErr / validSeams.size();
        }

        // Find best and second best
        std::size_t bestIdx = 0;
        for (std::size_t i = 1; i < 4; ++i) {
            if (sads[i] < sads[bestIdx]) {
                bestIdx = i;
            }
        }
        std::size_t secondIdx = (bestIdx == 0) ? 1 : 0;
        for (std::size_t i = 0; i < 4; ++i) {
            if (i != bestIdx && sads[i] < sads[secondIdx]) {
                secondIdx = i;
            }
        }

        const double eBest = sads[bestIdx];
        const double eSecond = sads[secondIdx];
        const double confidence = (eSecond - eBest) / (eBest + 1.0);

        if (confidence < options.confidenceThreshold) {
            rotations[static_cast<int>(faceId)] = Rotation::Deg0;
            fallbackUsed = true;
        } else {
            rotations[static_cast<int>(faceId)] = candidateRotations[bestIdx];
        }
    };

    // 1. RT (Left of FT): FT Left <-> RT Right (Seam 2)
    alignFaceAgainstNeighbors(CubeFace::Right, {2});

    // 2. LF (Right of FT): FT Right <-> LF Left (Seam 3)
    alignFaceAgainstNeighbors(CubeFace::Left, {3});

    // 3. BK: Evaluated against horizontal neighbors LF and RT (Seams 4, 5)
    alignFaceAgainstNeighbors(CubeFace::Back, {4, 5});

    // 4. UP: Evaluated against all 4 horizontal neighbors (Seams 0, 6, 7, 8)
    alignFaceAgainstNeighbors(CubeFace::Up, {0, 6, 7, 8});

    // 5. DN: Evaluated against all 4 horizontal neighbors (Seams 1, 9, 10, 11)
    alignFaceAgainstNeighbors(CubeFace::Down, {1, 9, 10, 11});

    // Degenerate fallback for BK if horizontal neighbors were both missing
    if (faces.hasFace(CubeFace::Back) && !faces.hasFace(CubeFace::Left) && !faces.hasFace(CubeFace::Right)) {
        alignFaceAgainstNeighbors(CubeFace::Back, {8, 11});
    }

    return rotations;
}

std::vector<SeamEvaluation> SkyboxCubeBuilder::evaluateAllSeams(
    const SkyboxFaces& faces,
    const std::array<Rotation, CubeFaceCount>& rotations,
    double mismatchThreshold)
{
    const auto& seams = cubeSeams();
    std::vector<SeamEvaluation> evaluations;
    evaluations.reserve(CubeSeamCount);

    for (int i = 0; i < CubeSeamCount; ++i) {
        const auto& seam = seams[i];
        SeamEvaluation eval;
        eval.seamIndex = i;
        eval.faceA = seam.faceA;
        eval.edgeA = seam.edgeA;
        eval.faceB = seam.faceB;
        eval.edgeB = seam.edgeB;

        if (faces.hasFace(seam.faceA) && faces.hasFace(seam.faceB)) {
            const QImage imgA = rotateImage(faces.face(seam.faceA), rotations[static_cast<int>(seam.faceA)]);
            const QImage imgB = rotateImage(faces.face(seam.faceB), rotations[static_cast<int>(seam.faceB)]);
            const auto edgePixelsA = extractEdgePixels(imgA, seam.edgeA);
            const auto edgePixelsB = extractEdgePixels(imgB, seam.edgeB);

            eval.error = computeSeamMeanError(edgePixelsA, edgePixelsB, seam.reversed);
            eval.mismatched = (eval.error > mismatchThreshold);
            eval.evaluated = true;
        } else {
            eval.error = 0.0;
            eval.mismatched = false;
            eval.evaluated = false;
        }
        evaluations.push_back(eval);
    }

    return evaluations;
}

std::array<FaceFaultStatus, CubeFaceCount> SkyboxCubeBuilder::analyzeFaultStatus(
    const std::vector<SeamEvaluation>& seamEvals,
    const SkyboxFaces* faces)
{
    std::array<FaceFaultStatus, CubeFaceCount> statuses;
    const std::array<CubeFace, CubeFaceCount> allFaces = {
        CubeFace::Front, CubeFace::Back, CubeFace::Left,
        CubeFace::Right, CubeFace::Up,   CubeFace::Down
    };

    for (int i = 0; i < CubeFaceCount; ++i) {
        statuses[i].face = allFaces[i];
        statuses[i].mismatchedEdgeCount = 0;
        statuses[i].connectedFailingNeighbors.clear();
    }

    for (const auto& eval : seamEvals) {
        if (eval.evaluated && eval.mismatched) {
            const int idxA = static_cast<int>(eval.faceA);
            const int idxB = static_cast<int>(eval.faceB);
            statuses[idxA].mismatchedEdgeCount++;
            statuses[idxA].connectedFailingNeighbors.push_back(eval.faceB);
            statuses[idxB].mismatchedEdgeCount++;
            statuses[idxB].connectedFailingNeighbors.push_back(eval.faceA);
        }
    }

    for (int i = 0; i < CubeFaceCount; ++i) {
        auto& st = statuses[i];
        const bool isPresent = (faces != nullptr) ? faces->hasFace(allFaces[i]) : true;
        bool hasEvaluatedSeams = false;
        for (const auto& eval : seamEvals) {
            if (eval.evaluated && (eval.faceA == allFaces[i] || eval.faceB == allFaces[i])) {
                hasEvaluatedSeams = true;
                break;
            }
        }

        if (!isPresent || !hasEvaluatedSeams) {
            st.isGroundTruth = false;
            st.isFaultIsolated = false;
            st.isMisoriented = false;
        } else if (st.mismatchedEdgeCount == 0) {
            st.isGroundTruth = true;
            st.isFaultIsolated = false;
            st.isMisoriented = false;
        } else if (st.mismatchedEdgeCount == 1) {
            st.isGroundTruth = false;
            st.isFaultIsolated = true;
            st.isMisoriented = false;
        } else { // M_F >= 2
            st.isGroundTruth = false;
            st.isFaultIsolated = false;
            st.isMisoriented = true;
        }
    }

    return statuses;
}

std::array<Rotation, CubeFaceCount> SkyboxCubeBuilder::alignPass2(
    const SkyboxFaces& faces,
    const std::array<Rotation, CubeFaceCount>& pass1Rotations,
    const SkyboxBuildOptions& options,
    std::vector<SeamEvaluation>& outSeamEvals,
    std::array<FaceFaultStatus, CubeFaceCount>& outFaultStatus,
    bool* outFallbackUsed)
{
    std::array<Rotation, CubeFaceCount> rotations = pass1Rotations;
    outSeamEvals = evaluateAllSeams(faces, rotations, options.seamMismatchThreshold);
    outFaultStatus = analyzeFaultStatus(outSeamEvals, &faces);

    if (!faces.hasFace(CubeFace::Front)) {
        // R2: If FT is missing, fall back to geometric priors
        return rotations;
    }

    const std::array<Rotation, 4> candidateRotations = {
        Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270
    };
    const auto& seams = cubeSeams();

    // Iterate up to CubeFaceCount passes to resolve multi-edge misorientations (M_F >= 2)
    for (int iter = 0; iter < CubeFaceCount; ++iter) {
        // Collect misoriented faces (M_F >= 2, excluding FT anchor)
        std::vector<CubeFace> misorientedFaces;
        for (int i = 0; i < CubeFaceCount; ++i) {
            const auto face = static_cast<CubeFace>(i);
            if (face != CubeFace::Front && faces.hasFace(face) && outFaultStatus[i].isMisoriented) {
                misorientedFaces.push_back(face);
            }
        }

        if (misorientedFaces.empty()) {
            break;
        }

        // Sort by mismatchedEdgeCount descending (highest fault first)
        std::sort(misorientedFaces.begin(), misorientedFaces.end(),
                  [&](CubeFace a, CubeFace b) {
                      return outFaultStatus[static_cast<int>(a)].mismatchedEdgeCount >
                             outFaultStatus[static_cast<int>(b)].mismatchedEdgeCount;
                  });

        bool anyAdjusted = false;
        for (CubeFace face : misorientedFaces) {
            const int faceIdx = static_cast<int>(face);
            if (!faces.hasFace(face)) {
                continue;
            }

            // Find seams connecting this face to neighbors
            std::vector<int> incidentSeamIndices;
            for (int s = 0; s < CubeSeamCount; ++s) {
                if (seams[s].faceA == face || seams[s].faceB == face) {
                    incidentSeamIndices.push_back(s);
                }
            }

            // Evaluate boundary variance of aligned neighbors
            double maxNeighborVariance = 0.0;
            int alignedNeighborCount = 0;
            for (int sIdx : incidentSeamIndices) {
                const auto& seam = seams[sIdx];
                const bool isFaceA = (seam.faceA == face);
                const CubeFace neighborFace = isFaceA ? seam.faceB : seam.faceA;
                const int neighborIdx = static_cast<int>(neighborFace);

                if (!faces.hasFace(neighborFace)) {
                    continue;
                }

                if (neighborFace == CubeFace::Front || !outFaultStatus[neighborIdx].isMisoriented) {
                    const QImage neighborImg = rotateImage(
                        faces.face(neighborFace),
                        rotations[neighborIdx]);
                    const auto edgeNeighbor = extractEdgePixels(
                        neighborImg,
                        isFaceA ? seam.edgeB : seam.edgeA);
                    maxNeighborVariance = std::max(maxNeighborVariance, computeEdgeVariance(edgeNeighbor));
                    alignedNeighborCount++;
                }
            }

            if (alignedNeighborCount == 0) {
                // No aligned neighbors available; do NOT mutate rotation
                continue;
            }

            // R2: Low-contrast boundary variance < threshold falls back smoothly to Deg0 prior
            if (maxNeighborVariance < options.varianceThreshold) {
                if (outFallbackUsed) {
                    *outFallbackUsed = true;
                }
                if (rotations[faceIdx] != Rotation::Deg0) {
                    rotations[faceIdx] = Rotation::Deg0;
                    outSeamEvals = evaluateAllSeams(faces, rotations, options.seamMismatchThreshold);
                    outFaultStatus = analyzeFaultStatus(outSeamEvals, &faces);
                    anyAdjusted = true;
                    break;
                }
                continue;
            }

            // Test 4 rotations against aligned neighbors (M_F <= 1 or FT)
            std::array<double, 4> totalErrors{};
            for (std::size_t r = 0; r < 4; ++r) {
                const QImage candidateFaceImg = rotateImage(faces.face(face), candidateRotations[r]);
                double totalErr = 0.0;
                int count = 0;

                for (int sIdx : incidentSeamIndices) {
                    const auto& seam = seams[sIdx];
                    const bool isFaceA = (seam.faceA == face);
                    const CubeFace neighborFace = isFaceA ? seam.faceB : seam.faceA;
                    const int neighborIdx = static_cast<int>(neighborFace);

                    if (!faces.hasFace(neighborFace)) {
                        continue;
                    }

                    if (neighborFace == CubeFace::Front || !outFaultStatus[neighborIdx].isMisoriented) {
                        const QImage neighborImg = rotateImage(
                            faces.face(neighborFace),
                            rotations[neighborIdx]);

                        const auto edgeSelf = extractEdgePixels(
                            candidateFaceImg,
                            isFaceA ? seam.edgeA : seam.edgeB);
                        const auto edgeNeighbor = extractEdgePixels(
                            neighborImg,
                            isFaceA ? seam.edgeB : seam.edgeA);

                        totalErr += computeSeamMeanError(edgeSelf, edgeNeighbor, seam.reversed);
                        count++;
                    }
                }

                totalErrors[r] = count > 0 ? (totalErr / count) : 255.0;
            }

            std::size_t bestR = 0;
            for (std::size_t r = 1; r < 4; ++r) {
                if (totalErrors[r] < totalErrors[bestR]) {
                    bestR = r;
                }
            }

            std::size_t secondR = (bestR == 0) ? 1 : 0;
            for (std::size_t r = 0; r < 4; ++r) {
                if (r != bestR && totalErrors[r] < totalErrors[secondR]) {
                    secondR = r;
                }
            }

            const double eBest = totalErrors[bestR];
            const double eSecond = totalErrors[secondR];
            const double confidence = (eSecond - eBest) / (eBest + 1.0);

            // R2: Low-contrast / confidence margin < 0.15 falls back smoothly to Deg0 prior
            const bool isLowConfidence = (confidence < options.confidenceThreshold);
            if (isLowConfidence && outFallbackUsed) {
                *outFallbackUsed = true;
            }
            const Rotation chosenRotation = isLowConfidence
                                                ? Rotation::Deg0
                                                : candidateRotations[bestR];

            if (rotations[faceIdx] != chosenRotation) {
                rotations[faceIdx] = chosenRotation;
                outSeamEvals = evaluateAllSeams(faces, rotations, options.seamMismatchThreshold);
                outFaultStatus = analyzeFaultStatus(outSeamEvals, &faces);
                anyAdjusted = true;
                break; // Break inner loop to re-sort and re-evaluate misoriented faces
            }
        }

        if (!anyAdjusted) {
            break;
        }
    }

    // Step 2B: False Consensus & Anchor Authority Resolution
    // Resolves degenerate edge cases where faces with identical edges form false-positive mutual seams
    // while conflicting with anchored neighbors (causing M_F == 1 on each face without any M_F >= 2).
    auto evalSingleSeam = [&](int seamIdx, const std::array<Rotation, CubeFaceCount>& rots) -> double {
        const auto& seam = seams[seamIdx];
        if (!faces.hasFace(seam.faceA) || !faces.hasFace(seam.faceB)) {
            return 0.0;
        }
        const QImage imgA = rotateImage(faces.face(seam.faceA), rots[static_cast<int>(seam.faceA)]);
        const QImage imgB = rotateImage(faces.face(seam.faceB), rots[static_cast<int>(seam.faceB)]);
        const auto edgePixelsA = extractEdgePixels(imgA, seam.edgeA);
        const auto edgePixelsB = extractEdgePixels(imgB, seam.edgeB);
        return computeSeamMeanError(edgePixelsA, edgePixelsB, seam.reversed);
    };

    for (int passB = 0; passB < 2; ++passB) {
        std::vector<int> mismatchedSeamIndices;
        for (int s = 0; s < CubeSeamCount; ++s) {
            if (outSeamEvals[s].evaluated && outSeamEvals[s].mismatched) {
                mismatchedSeamIndices.push_back(s);
            }
        }
        if (mismatchedSeamIndices.empty()) {
            break;
        }

        // Determine ground truth anchors: Front is always anchor; faces matching Front are anchors.
        std::array<bool, CubeFaceCount> isAnchor{};
        isAnchor[static_cast<int>(CubeFace::Front)] = true;
        for (const auto& eval : outSeamEvals) {
            if (eval.evaluated && !eval.mismatched) {
                if (eval.faceA == CubeFace::Front) isAnchor[static_cast<int>(eval.faceB)] = true;
                if (eval.faceB == CubeFace::Front) isAnchor[static_cast<int>(eval.faceA)] = true;
            }
        }

        // Collect suspect non-anchor faces that have mismatched seams with anchors
        std::set<CubeFace> suspects;
        for (int sIdx : mismatchedSeamIndices) {
            const auto& seam = seams[sIdx];
            const bool aIsAnchor = isAnchor[static_cast<int>(seam.faceA)];
            const bool bIsAnchor = isAnchor[static_cast<int>(seam.faceB)];
            if (aIsAnchor && !bIsAnchor && faces.hasFace(seam.faceB)) suspects.insert(seam.faceB);
            else if (bIsAnchor && !aIsAnchor && faces.hasFace(seam.faceA)) suspects.insert(seam.faceA);
            else {
                if (!aIsAnchor && faces.hasFace(seam.faceA)) suspects.insert(seam.faceA);
                if (!bIsAnchor && faces.hasFace(seam.faceB)) suspects.insert(seam.faceB);
            }
        }

        if (suspects.empty()) {
            break;
        }

        // Look for adjacent pairs in suspects that share a currently-matched seam (False Consensus Pair)
        bool pairResolved = false;
        std::vector<CubeFace> suspectList(suspects.begin(), suspects.end());
        for (std::size_t i = 0; i < suspectList.size() && !pairResolved; ++i) {
            for (std::size_t j = i + 1; j < suspectList.size() && !pairResolved; ++j) {
                const CubeFace faceA = suspectList[i];
                const CubeFace faceB = suspectList[j];

                int sharedSeamIdx = -1;
                for (int s = 0; s < CubeSeamCount; ++s) {
                    if ((seams[s].faceA == faceA && seams[s].faceB == faceB) ||
                        (seams[s].faceA == faceB && seams[s].faceB == faceA)) {
                        sharedSeamIdx = s;
                        break;
                    }
                }

                if (sharedSeamIdx >= 0 && outSeamEvals[sharedSeamIdx].evaluated && !outSeamEvals[sharedSeamIdx].mismatched) {
                    // False consensus pair detected! Jointly optimize rotations of faceA and faceB
                    std::vector<int> relevantSeams;
                    for (int s = 0; s < CubeSeamCount; ++s) {
                        if (seams[s].faceA == faceA || seams[s].faceB == faceA ||
                            seams[s].faceA == faceB || seams[s].faceB == faceB) {
                            relevantSeams.push_back(s);
                        }
                    }

                    double bestJointError = std::numeric_limits<double>::max();
                    Rotation bestRotA = rotations[static_cast<int>(faceA)];
                    Rotation bestRotB = rotations[static_cast<int>(faceB)];

                    for (Rotation rA : candidateRotations) {
                        for (Rotation rB : candidateRotations) {
                            auto testRots = rotations;
                            testRots[static_cast<int>(faceA)] = rA;
                            testRots[static_cast<int>(faceB)] = rB;

                            double totalErr = 0.0;
                            int count = 0;
                            for (int sIdx : relevantSeams) {
                                totalErr += evalSingleSeam(sIdx, testRots);
                                count++;
                            }
                            const double meanErr = (count > 0) ? (totalErr / count) : 255.0;
                            if (meanErr < bestJointError) {
                                bestJointError = meanErr;
                                bestRotA = rA;
                                bestRotB = rB;
                            }
                        }
                    }

                    if (rotations[static_cast<int>(faceA)] != bestRotA || rotations[static_cast<int>(faceB)] != bestRotB) {
                        rotations[static_cast<int>(faceA)] = bestRotA;
                        rotations[static_cast<int>(faceB)] = bestRotB;
                        outSeamEvals = evaluateAllSeams(faces, rotations, options.seamMismatchThreshold);
                        outFaultStatus = analyzeFaultStatus(outSeamEvals, &faces);
                        pairResolved = true;
                    }
                }
            }
        }

        // If no pair was resolved, optimize individual suspects against their incident seams
        if (!pairResolved) {
            bool singleResolved = false;
            for (CubeFace face : suspects) {
                std::vector<int> incidentSeams;
                for (int s = 0; s < CubeSeamCount; ++s) {
                    if (seams[s].faceA == face || seams[s].faceB == face) {
                        incidentSeams.push_back(s);
                    }
                }

                std::array<double, 4> singleErrors{};
                for (std::size_t r = 0; r < 4; ++r) {
                    auto testRots = rotations;
                    testRots[static_cast<int>(face)] = candidateRotations[r];
                    double totalErr = 0.0;
                    int count = 0;
                    for (int sIdx : incidentSeams) {
                        totalErr += evalSingleSeam(sIdx, testRots);
                        count++;
                    }
                    singleErrors[r] = (count > 0) ? (totalErr / count) : 255.0;
                }

                std::size_t bestR = 0;
                for (std::size_t r = 1; r < 4; ++r) {
                    if (singleErrors[r] < singleErrors[bestR]) {
                        bestR = r;
                    }
                }
                std::size_t secondR = (bestR == 0) ? 1 : 0;
                for (std::size_t r = 0; r < 4; ++r) {
                    if (r != bestR && singleErrors[r] < singleErrors[secondR]) {
                        secondR = r;
                    }
                }

                const double eBest = singleErrors[bestR];
                const double eSecond = singleErrors[secondR];
                const double confidence = (eSecond - eBest) / (eBest + 1.0);
                const bool isLowConf = (confidence < options.confidenceThreshold);
                if (isLowConf && outFallbackUsed) {
                    *outFallbackUsed = true;
                }
                const Rotation chosenRot = isLowConf ? Rotation::Deg0 : candidateRotations[bestR];

                if (rotations[static_cast<int>(face)] != chosenRot) {
                    rotations[static_cast<int>(face)] = chosenRot;
                    outSeamEvals = evaluateAllSeams(faces, rotations, options.seamMismatchThreshold);
                    outFaultStatus = analyzeFaultStatus(outSeamEvals, &faces);
                    singleResolved = true;
                    break;
                }
            }
            if (!singleResolved) {
                break;
            }
        }
    }

    return rotations;
}

QImage SkyboxCubeBuilder::composeCubeImage(
    const SkyboxFaces& faces,
    const std::array<Rotation, CubeFaceCount>& rotations,
    int faceResolution,
    bool allowNonSquare)
{
    const int S = (faceResolution > 0 && faceResolution <= 8192) ? faceResolution : 1024;
    const int totalWidth = 4 * S;
    const int totalHeight = 3 * S;

    QImage canvas(totalWidth, totalHeight, QImage::Format_RGBA8888);
    if (canvas.isNull()) {
        return QImage();
    }
    canvas.fill(qRgba(0, 0, 0, 255));

    auto drawSlot = [&](CubeFace face, int col, int row) {
        if (!faces.hasFace(face)) {
            return;
        }
        const auto& rawFace = faces.face(face);
        if (!allowNonSquare && rawFace.width() != rawFace.height()) {
            return; // Skip non-square face to prevent aspect ratio distortion
        }
        QImage rotated = rotateImage(rawFace, rotations[static_cast<int>(face)]);
        if (rotated.format() != QImage::Format_RGBA8888) {
            rotated = rotated.convertToFormat(QImage::Format_RGBA8888);
        }
        if (rotated.width() != S || rotated.height() != S) {
            rotated = rotated.scaled(S, S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        copyFaceToCanvas(canvas, rotated, col, row, S);
    };

    // Standard 4x3 Horizontal Cross Grid (FT-Centered, Inside-Out Topology):
    // Row 0: (1, 0) = UP
    drawSlot(CubeFace::Up, 1, 0);

    // Row 1: (0, 1) = RT, (1, 1) = FT (Center), (2, 1) = LF, (3, 1) = BK
    drawSlot(CubeFace::Right, 0, 1);
    drawSlot(CubeFace::Front, 1, 1);
    drawSlot(CubeFace::Left, 2, 1);
    drawSlot(CubeFace::Back, 3, 1);

    // Row 2: (1, 2) = DN
    drawSlot(CubeFace::Down, 1, 2);

    return canvas;
}

} // namespace Domain::Material

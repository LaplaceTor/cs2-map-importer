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

    if (!normalizedFaces.hasFace(CubeFace::Front)) {
        // Fallback: If FT is missing, fall back to geometric default rotations (all Deg0) without analyzing image data
        result.faceRotations = {
            Rotation::Deg0, Rotation::Deg0, Rotation::Deg0,
            Rotation::Deg0, Rotation::Deg0, Rotation::Deg0
        };
        result.fallbackUsed = true;
        result.seamEvaluations = evaluateAllSeams(
            normalizedFaces, result.faceRotations, options.seamMismatchThreshold);
    } else {
        // Global 1024 exhaustive optimization with FT anchored at Deg0
        result.faceRotations = optimizeRotations(
            normalizedFaces,
            options,
            result.totalCost,
            result.secondBestCost,
            result.costMargin,
            result.isAmbiguous);
        result.fallbackUsed = false;
        result.seamEvaluations = evaluateAllSeams(
            normalizedFaces, result.faceRotations, options.seamMismatchThreshold);
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

double SkyboxCubeBuilder::computeColorCost(
    const std::vector<QRgb>& pixelsA,
    const std::vector<QRgb>& pixelsB,
    bool reversed)
{
    if (pixelsA.empty() || pixelsB.empty() || pixelsA.size() != pixelsB.size()) {
        return 1.0;
    }
    const double sad = computeSeamSad(pixelsA, pixelsB, reversed);
    if (!std::isfinite(sad)) {
        return 1.0;
    }
    const double maxSad = static_cast<double>(pixelsA.size()) * 3.0 * 255.0;
    return std::clamp(sad / maxSad, 0.0, 1.0);
}

double SkyboxCubeBuilder::computeGradientCost(
    const std::vector<QRgb>& pixelsA,
    const std::vector<QRgb>& pixelsB,
    bool reversed)
{
    if (pixelsA.size() < 2 || pixelsB.size() < 2 || pixelsA.size() != pixelsB.size()) {
        return 1.0;
    }
    const std::size_t n = pixelsA.size();

    // If reversed, physically reverse the B pixel sequence first
    std::vector<QRgb> bSeq;
    if (reversed) {
        bSeq.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            bSeq[i] = pixelsB[n - 1 - i];
        }
    }
    const auto& effB = reversed ? bSeq : pixelsB;

    double gradDiffSum = 0.0;
    for (std::size_t i = 0; i < n - 1; ++i) {
        const QRgb pA0 = pixelsA[i];
        const QRgb pA1 = pixelsA[i + 1];
        const QRgb pB0 = effB[i];
        const QRgb pB1 = effB[i + 1];

        const int drA = qRed(pA1) - qRed(pA0);
        const int dgA = qGreen(pA1) - qGreen(pA0);
        const int dbA = qBlue(pA1) - qBlue(pA0);

        const int drB = qRed(pB1) - qRed(pB0);
        const int dgB = qGreen(pB1) - qGreen(pB0);
        const int dbB = qBlue(pB1) - qBlue(pB0);

        gradDiffSum += std::abs(drA - drB) + std::abs(dgA - dgB) + std::abs(dbA - dbB);
    }

    const double maxGradDiff = static_cast<double>(n - 1) * 3.0 * 510.0;
    return std::clamp(gradDiffSum / maxGradDiff, 0.0, 1.0);
}

double SkyboxCubeBuilder::computeSeamCost(
    const std::vector<QRgb>& pixelsA,
    const std::vector<QRgb>& pixelsB,
    bool reversed,
    double colorWeight,
    double gradientWeight)
{
    if (pixelsA.empty() || pixelsB.empty() || pixelsA.size() != pixelsB.size()) {
        return 1.0;
    }
    const double colorCost = computeColorCost(pixelsA, pixelsB, reversed);
    const double gradientCost = computeGradientCost(pixelsA, pixelsB, reversed);
    const double totalWeight = colorWeight + gradientWeight;
    if (totalWeight <= 0.0) {
        return colorCost;
    }
    const double cost = (colorWeight * colorCost + gradientWeight * gradientCost) / totalWeight;
    return std::clamp(cost, 0.0, 1.0);
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

std::array<Rotation, CubeFaceCount> SkyboxCubeBuilder::optimizeRotations(
    const SkyboxFaces& faces,
    const SkyboxBuildOptions& options,
    double& outBestCost,
    double& outSecondBestCost,
    double& outMargin,
    bool& outAmbiguous)
{
    const std::array<Rotation, 4> rotEnum = {
        Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270
    };
    const std::array<FaceEdge, 4> edgeEnum = {
        FaceEdge::Top, FaceEdge::Bottom, FaceEdge::Left, FaceEdge::Right
    };

    // 1. EdgeCache[face][rotIndex][edgeIndex]
    // Max 6 faces * 4 rotations * 4 edges
    std::vector<QRgb> edgeCache[CubeFaceCount][4][4];
    for (int f = 0; f < CubeFaceCount; ++f) {
        const auto face = static_cast<CubeFace>(f);
        if (!faces.hasFace(face)) {
            continue;
        }
        for (int r = 0; r < 4; ++r) {
            const QImage rotImg = rotateImage(faces.face(face), rotEnum[r]);
            for (int e = 0; e < 4; ++e) {
                edgeCache[f][r][e] = extractEdgePixels(rotImg, edgeEnum[e]);
            }
        }
    }

    // 2. SeamCostTable[seamIndex][rotA][rotB]
    const auto& seams = cubeSeams();
    double seamCostTable[CubeSeamCount][4][4];
    bool seamValid[CubeSeamCount];

    for (int s = 0; s < CubeSeamCount; ++s) {
        const auto& seam = seams[s];
        const int fA = static_cast<int>(seam.faceA);
        const int fB = static_cast<int>(seam.faceB);
        const int eA = static_cast<int>(seam.edgeA);
        const int eB = static_cast<int>(seam.edgeB);

        if (faces.hasFace(seam.faceA) && faces.hasFace(seam.faceB)) {
            seamValid[s] = true;
            for (int rA = 0; rA < 4; ++rA) {
                for (int rB = 0; rB < 4; ++rB) {
                    seamCostTable[s][rA][rB] = computeSeamCost(
                        edgeCache[fA][rA][eA],
                        edgeCache[fB][rB][eB],
                        seam.reversed,
                        options.colorWeight,
                        options.gradientWeight);
                }
            }
        } else {
            seamValid[s] = false;
            for (int rA = 0; rA < 4; ++rA) {
                for (int rB = 0; rB < 4; ++rB) {
                    seamCostTable[s][rA][rB] = 0.0;
                }
            }
        }
    }

    // 3. 1024 Combinations Exhaustive Search (FT fixed to Deg0 = rot index 0)
    // Faces indices:
    // Front = 0, Back = 1, Left = 2, Right = 3, Up = 4, Down = 5
    const int rFT = 0; // Deg0
    double bestCost = std::numeric_limits<double>::infinity();
    double secondBestCost = std::numeric_limits<double>::infinity();
    std::array<Rotation, CubeFaceCount> bestRotations = {
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0,
        Rotation::Deg0, Rotation::Deg0, Rotation::Deg0
    };

    // Candidate iteration order: Deg0 -> Deg90 -> Deg180 -> Deg270 (indices 0, 1, 2, 3)
    for (int rBK = 0; rBK < 4; ++rBK) {
        for (int rLF = 0; rLF < 4; ++rLF) {
            for (int rRT = 0; rRT < 4; ++rRT) {
                for (int rUP = 0; rUP < 4; ++rUP) {
                    for (int rDN = 0; rDN < 4; ++rDN) {
                        const int currentRotIdx[CubeFaceCount] = {
                            rFT, rBK, rLF, rRT, rUP, rDN
                        };

                        double totalCost = 0.0;
                        for (int s = 0; s < CubeSeamCount; ++s) {
                            if (seamValid[s]) {
                                const int fA = static_cast<int>(seams[s].faceA);
                                const int fB = static_cast<int>(seams[s].faceB);
                                totalCost += seamCostTable[s][currentRotIdx[fA]][currentRotIdx[fB]];
                            }
                        }

                        if (totalCost < bestCost) {
                            secondBestCost = bestCost;
                            bestCost = totalCost;
                            bestRotations[0] = rotEnum[rFT];
                            bestRotations[1] = rotEnum[rBK];
                            bestRotations[2] = rotEnum[rLF];
                            bestRotations[3] = rotEnum[rRT];
                            bestRotations[4] = rotEnum[rUP];
                            bestRotations[5] = rotEnum[rDN];
                        } else if (totalCost < secondBestCost) {
                            secondBestCost = totalCost;
                        }
                    }
                }
            }
        }
    }

    if (!std::isfinite(secondBestCost)) {
        secondBestCost = bestCost;
    }

    outBestCost = bestCost;
    outSecondBestCost = secondBestCost;
    outMargin = std::max(0.0, secondBestCost - bestCost);
    outAmbiguous = (outMargin < options.ambiguityThreshold);

    return bestRotations;
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

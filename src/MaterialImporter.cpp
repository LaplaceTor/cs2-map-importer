#include "MaterialImporter.h"
#include "Miscellaneous.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QDebug>

bool MaterialImporter::ProcessImage(const QString& imagePath, const QString& appDir, QString& outPreviewPath) {
    if (imagePath.isEmpty()) {
        Miscellaneous::Log("Error: Empty image path provided to MaterialImporter.");
        return false;
    }

    QFileInfo fileInfo(imagePath);
    QString materialName = fileInfo.baseName();
    QString suffix = fileInfo.suffix().toLower();

    // Create target directory: appDir/materials/material_name
    QString targetDir = QDir(appDir).filePath("materials/" + materialName);
    QDir().mkpath(targetDir);

    QString targetPngPath = QDir(targetDir).filePath(materialName + "_color.png");
    targetPngPath = QDir::toNativeSeparators(targetPngPath);

    Miscellaneous::Log("Processing image: " + imagePath);
    Miscellaneous::Log("Target preview path: " + targetPngPath);

    if (suffix == "vtf") {
        QString program = QDir(appDir).filePath("bin/VTFCmd.exe");
        program = QDir::toNativeSeparators(program);

        QString nativeTargetDir = QDir::toNativeSeparators(targetDir);
        QString nativeImagePath = QDir::toNativeSeparators(imagePath);

        QStringList arguments = {
            "-file",
            nativeImagePath,
            "-output",
            nativeTargetDir,
            "-exportformat",
            "png"
        };

        Miscellaneous::Log("Running VTFCmd conversion...");
        int exitCode = Miscellaneous::RunCommandSync(program, arguments);
        if (exitCode != 0) {
            Miscellaneous::Log("Error: VTFCmd failed with exit code " + QString::number(exitCode));
            return false;
        }

        // VTFCmd outputs the file with the same base name, e.g. material_name.png
        QString generatedPng = QDir(targetDir).filePath(materialName + ".png");
        generatedPng = QDir::toNativeSeparators(generatedPng);

        if (QFile::exists(generatedPng)) {
            if (QFile::exists(targetPngPath)) {
                QFile::remove(targetPngPath);
            }
            if (QFile::rename(generatedPng, targetPngPath)) {
                Miscellaneous::Log("VTF converted and saved to: " + targetPngPath);
                outPreviewPath = targetPngPath;
                return true;
            } else {
                Miscellaneous::Log("Error: Failed to rename converted PNG to " + targetPngPath);
                return false;
            }
        } else {
            Miscellaneous::Log("Error: Converted PNG not found at " + generatedPng);
            return false;
        }
    } else {
        // Non-VTF files: png, jpg, jpeg, tga, tif, tiff
        Miscellaneous::Log("Loading image via QImage...");
        QImage image;
        if (image.load(imagePath)) {
            if (QFile::exists(targetPngPath)) {
                QFile::remove(targetPngPath);
            }
            if (image.save(targetPngPath, "PNG")) {
                Miscellaneous::Log("Image successfully saved as PNG to: " + targetPngPath);
                outPreviewPath = targetPngPath;
                return true;
            } else {
                Miscellaneous::Log("Warning: QImage failed to save as PNG. Copying file directly...");
            }
        } else {
            Miscellaneous::Log("Warning: QImage failed to load image. Copying file directly...");
        }

        // Fallback: Copy directly
        if (QFile::exists(targetPngPath)) {
            QFile::remove(targetPngPath);
        }
        if (QFile::copy(imagePath, targetPngPath)) {
            Miscellaneous::Log("Copied image directly to: " + targetPngPath);
            outPreviewPath = targetPngPath;
            return true;
        } else {
            Miscellaneous::Log("Error: Failed to copy image to " + targetPngPath);
            return false;
        }
    }

    return false;
}

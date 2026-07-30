#include <QCoreApplication>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cassert>
#include "MaterialFix.h"
#include "VmfBspProcess.h"

void testComplexShaderVariablesFix() {
    qDebug() << "Running testComplexShaderVariablesFix...";
    QStringList lines = {
        "\"shader\" \"csgo_complex.vfx\"",
        "\t\"F_NOTINT\"\t\t\"1\"",
        "\t\"F_VERTEX_COLOR\"\t\t\"1\""
    };
    bool fileModified = false;
    MaterialFix::ComplexShaderVariablesFix(lines, fileModified);

    assert(fileModified == true);
    assert(lines.size() == 4);
    assert(lines[1].contains("F_TINT_MASK"));
    assert(lines[2].contains("TextureTintMask"));
    assert(lines[2].contains("[0 0 0 0]"));
    assert(lines[3].contains("F_PAINT_VERTEX_COLORS"));

    qDebug() << "testComplexShaderVariablesFix passed!";
}

void testSkinKVFix() {
    qDebug() << "Running testSkinKVFix...";
    QString vmfContent =
        "versioninfo\n"
        "{\n"
        "\t\"editorversion\" \"400\"\n"
        "}\n"
        "entity\n"
        "{\n"
        "\t\"id\" \"1\"\n"
        "\t\"classname\" \"prop_dynamic\"\n"
        "\t\"skin\" \"0\"\n"
        "}\n"
        "entity\n"
        "{\n"
        "\t\"id\" \"2\"\n"
        "\t\"classname\" \"prop_static\"\n"
        "\t\"skin\" \"1\"\n"
        "}\n"
        "entity\n"
        "{\n"
        "\t\"id\" \"3\"\n"
        "\t\"classname\" \"prop_physics\"\n"
        "\t\"skin\" \"0\"\n"
        "}\n";

    QString testVmfPath = "test_skin_fix.vmf";
    QFile file(testVmfPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << vmfContent;
        file.close();
    }

    VmfBspProcess::SkinKVFix(testVmfPath);

    QFile fileIn(testVmfPath);
    QStringList resultLines;
    if (fileIn.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&fileIn);
        while (!in.atEnd()) {
            resultLines.append(in.readLine());
        }
        fileIn.close();
    }
    QFile::remove(testVmfPath);

    QString fullResult = resultLines.join("\n");
    qDebug() << "Resulting VMF:\n" << fullResult;

    // Entity 1 skin 0 -> default
    assert(fullResult.contains("\"classname\" \"prop_dynamic\"\n\t\"skin\" \"default\""));
    // Entity 2 skin 1 -> unchanged
    assert(fullResult.contains("\"classname\" \"prop_static\"\n\t\"skin\" \"1\""));
    // Entity 3 skin 0 -> default
    assert(fullResult.contains("\"classname\" \"prop_physics\"\n\t\"skin\" \"default\""));

    qDebug() << "testSkinKVFix passed!";
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testComplexShaderVariablesFix();
    testSkinKVFix();
    qDebug() << "All tests passed successfully!";
    return 0;
}

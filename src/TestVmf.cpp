#include "VmfBspProcess.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <iostream>
#include <cassert>

// Dummy implementation of ShowMessageBox for compilation
#include "Ui.h"
bool Backend::ShowMessageBox(const QString& title, const QString& text, int iconType, bool showYesNo) {
    Q_UNUSED(title);
    Q_UNUSED(text);
    Q_UNUSED(iconType);
    Q_UNUSED(showYesNo);
    return true;
}

int main() {
    QString testVmfContent = R"vmf(versioninfo
{
	"editorversion" "400"
	"editorbuild" "9999"
	"mapversion" "2"
	"formatversion" "100"
	"prefab" "0"
}
world
{
	"id" "1"
	"classname" "worldspawn"
	"skyname" "sky_day01_01"
	"maxpropscreenwidth" "-1"
	"detailvbsp" "detail.vbsp"
	"detailmaterial" "detail/detailsprites"
	solid
	{
		"id" "10"
		side
		{
			"id" "11"
			"plane" "(0 0 0) (0 128 0) (128 128 0)"
			"material" "TOOLS/TOOLSSKIP"
		}
		side
		{
			"id" "12"
			"plane" "(0 0 0) (128 0 0) (128 0 128)"
			"material" "brick/brickwall001a"
		}
	}
	solid
	{
		"id" "20"
		side
		{
			"id" "21"
			"plane" "(0 0 0) (0 128 0) (128 128 0)"
			"material" "brick/brickwall001a"
		}
		side
		{
			"id" "22"
			"plane" "(0 0 0) (128 0 0) (128 0 128)"
			"material" "brick/brickwall001a"
		}
	}
}
entity
{
	"id" "30"
	"classname" "func_detail"
	solid
	{
		"id" "31"
		side
		{
			"id" "32"
			"material" "tools/toolshint"
		}
		side
		{
			"id" "33"
			"material" "brick/brickwall001a"
		}
	}
}
entity
{
	"id" "40"
	"classname" "trigger_multiple"
	solid
	{
		"id" "41"
		side
		{
			"id" "42"
			"material" "tools/toolshint"
		}
		side
		{
			"id" "43"
			"material" "brick/brickwall001a"
		}
	}
	solid
	{
		"id" "44"
		side
		{
			"id" "45"
			"material" "brick/brickwall001a"
		}
	}
}
)vmf";

    QString filename = "test_vmf.vmf";
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Failed to write test VMF file" << std::endl;
        return 1;
    }
    QTextStream out(&file);
    out << testVmfContent;
    file.close();

    std::cout << "Running RemoveSkipAndHintSolids..." << std::endl;
    VmfBspProcess::RemoveSkipAndHintSolids(filename);

    QFile infile(filename);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Failed to read test VMF file after processing" << std::endl;
        return 1;
    }
    QTextStream in(&infile);
    QString result = in.readAll();
    infile.close();

    QFile::remove(filename);

    std::cout << "Processed VMF content:\n" << result.toStdString() << std::endl;

    // Assertions
    // 1. Solid ID 10 is inside worldspawn and contains TOOLS/TOOLSSKIP -> Should be completely removed
    assert(!result.contains("\"id\" \"10\""));
    assert(!result.contains("\"id\" \"11\""));
    assert(!result.contains("\"id\" \"12\""));

    // 2. Solid ID 20 is inside worldspawn with no skip/hint -> Should be kept
    assert(result.contains("\"id\" \"20\""));
    assert(result.contains("\"id\" \"21\""));

    // 3. Solid ID 31 is inside func_detail (under ContextFuncDetail) and contains tools/toolshint -> Should be completely removed
    assert(!result.contains("\"id\" \"31\""));
    assert(!result.contains("\"id\" \"32\""));
    assert(!result.contains("\"id\" \"33\""));

    // 4. Solid ID 41 is inside trigger_multiple and contains tools/toolshint -> Should be kept, but tools/toolshint replaced with tools/toolsnodraw
    assert(result.contains("\"id\" \"41\""));
    assert(!result.contains("tools/toolshint"));
    assert(result.contains("tools/toolsnodraw"));

    // 5. Solid ID 44 is inside trigger_multiple and has no skip/hint -> Should be kept as-is
    assert(result.contains("\"id\" \"44\""));
    assert(result.contains("\"id\" \"45\""));

    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}

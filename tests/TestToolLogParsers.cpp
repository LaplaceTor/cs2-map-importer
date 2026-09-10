#include <QTest>
#include <QString>
#include <QStringList>
#include <QDir>

#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Tool/ToolErrors.h"
#include "Domain/Tool/Source1ImportLogParser.h"
#include "Domain/Tool/ResourceCompilerLogParser.h"
#include "Domain/Tool/Source1ImportTool.h"
#include "Domain/Tool/ResourceCompilerTool.h"

using namespace Core::Error;
using namespace Core::Path;
using namespace Core::Process;
using namespace Domain::Tool;

class TestToolLogParsers : public QObject {
    Q_OBJECT

private slots:
    void testProcessOptionsStandardInput();
    void testSource1ImportLogParser_SuccessSingleParticle();
    void testSource1ImportLogParser_SuccessMultipleParticles();
    void testSource1ImportLogParser_FailureImportError();
    void testSource1ImportLogParser_FailureNoMatchingFiles();
    void testSource1ImportLogParser_NonZeroExitCode();
    void testSource1ImportLogParser_GameInfoNotFoundError();
    void testSource1ImportLogParser_FailedToMakePathRelative();
    void testResourceCompilerLogParser_SuccessSingleCompile();
    void testResourceCompilerLogParser_SuccessSkipped();
    void testResourceCompilerLogParser_CompileErrors();
    void testResourceCompilerLogParser_NonZeroExitCode();
    void testSource1ImportTool_BuildArguments_Csgo();
    void testSource1ImportTool_BuildArguments_NonCsgo_WithFlags();
    void testResourceCompilerTool_BuildArguments_Default();
    void testResourceCompilerTool_BuildArguments_CustomOptions();
    void testToolErrors_AllFactoryMethods();
    void testSource1ImportTool_Validation();
    void testResourceCompilerTool_Validation();
};

void TestToolLogParsers::testProcessOptionsStandardInput()
{
    ProcessOptions options;
    QVERIFY(options.standardInput.isEmpty());

    options.standardInput = "y\n";
    QCOMPARE(options.standardInput, QByteArray("y\n"));
}

void TestToolLogParsers::testSource1ImportLogParser_SuccessSingleParticle()
{
    QString stdOut = QStringLiteral(
        "* Importing D:\\SteamLibrary\\steamapps\\common\\Counter-Strike Source\\cstrike\\particles\\error.pcf\n"
        "* Into folder d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\n"
        "=====================================================================\n"
        "=====================================================================\n"
        "  \terror to d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\\error.vpcf\n"
        " * Importing Particle System Definition: error\n"
        "---------------------------------------------------------------------\n"
        "Writing file \"d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\\error.vpcf\"\n"
        "\n"
        "\n"
        "-----------------------------------------------------------------\n"
        " OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = Source1ImportLogParser::parse(stdOut, QString(), 0);

    QVERIFY(result.success);
    QVERIFY(!result.hasNoMatchingFiles);
    QCOMPARE(result.importedCount, 1);
    QCOMPARE(result.failedCount, 0);
    QCOMPARE(result.skippedCount, 0);
    QCOMPARE(result.unknownCount, 0);
    QCOMPARE(result.generatedVpcfPaths.size(), 1);
    QVERIFY(result.generatedVpcfPaths.first().endsWith(QStringLiteral("particles/error/error.vpcf")));
}

void TestToolLogParsers::testSource1ImportLogParser_SuccessMultipleParticles()
{
    QString stdOut = QStringLiteral(
        "* Importing D:\\SteamLibrary\\steamapps\\common\\Counter-Strike Source\\cstrike\\particles\\lighting.pcf\n"
        "* Into folder d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\lighting\n"
        "=====================================================================\n"
        "  \tlight_gaslamp_glow to d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\lighting\\light_gaslamp_glow.vpcf\n"
        " * Importing Particle System Definition: light_gaslamp_glow\n"
        "Writing file \"d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\lighting\\light_gaslamp_glow.vpcf\"\n"
        "  \tlight_glow01 to d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\lighting\\light_glow01.vpcf\n"
        " * Importing Particle System Definition: light_glow01\n"
        "Writing file \"d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\lighting\\light_glow01.vpcf\"\n"
        "-----------------------------------------------------------------\n"
        " OK: 2 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = Source1ImportLogParser::parse(stdOut, QString(), 0);

    QVERIFY(result.success);
    QCOMPARE(result.importedCount, 2);
    QCOMPARE(result.failedCount, 0);
    QCOMPARE(result.generatedVpcfPaths.size(), 2);
    QVERIFY(result.generatedVpcfPaths.at(0).endsWith(QStringLiteral("light_gaslamp_glow.vpcf")));
    QVERIFY(result.generatedVpcfPaths.at(1).endsWith(QStringLiteral("light_glow01.vpcf")));
}

void TestToolLogParsers::testSource1ImportLogParser_FailureImportError()
{
    QString stdOut = QStringLiteral(
        "WARNING: Encountered an error reading file \"corrupt.pcf\"!\n"
        "\t*** Error Importing corrupt.pcf\n"
        "\n"
        "-----------------------------------------------------------------\n"
        "FAILED:\n"
        " corrupt.pcf\n"
        "-----------------------------------------------------------------\n"
        "\n"
        "-----------------------------------------------------------------\n"
        " ERROR: 0 imported, 1 failed, 0 skipped, 0 unknown, 0m:00s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = Source1ImportLogParser::parse(stdOut, QString(), 1);

    QVERIFY(!result.success);
    QCOMPARE(result.importedCount, 0);
    QCOMPARE(result.failedCount, 1);
    QVERIFY(!result.warnings.isEmpty());
    QVERIFY(!result.errorMessages.isEmpty());
}

void TestToolLogParsers::testSource1ImportLogParser_FailureNoMatchingFiles()
{
    QString stdOut = QStringLiteral(
        "*** Found no files matching specification \"nonexistent.pcf\" exists for this mod!\n"
        "*** Found no files matching specifications\n"
    );

    // Deceptive exit code 0
    auto result = Source1ImportLogParser::parse(stdOut, QString(), 0);

    QVERIFY(!result.success);
    QVERIFY(result.hasNoMatchingFiles);
    QCOMPARE(result.generatedVpcfPaths.size(), 0);
}

void TestToolLogParsers::testSource1ImportLogParser_NonZeroExitCode()
{
    QString stdOut = QStringLiteral("Some unexpected fatal output\n");
    auto result = Source1ImportLogParser::parse(stdOut, QString(), 2);
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessages.isEmpty());
    QCOMPARE(result.errorMessages.first(), QStringLiteral("Some unexpected fatal output"));
}

void TestToolLogParsers::testSource1ImportLogParser_GameInfoNotFoundError()
{
    QString stdOut = QStringLiteral(
        "Failed to map from d:/steamlibrary/steamapps/common/counter-strike source/ to game-path. Note this is ok for the dota localization import.\n"
        "Unable to load source 1 mod gameinfo.txt! d:\\steamlibrary\\steamapps\\common\\counter-strike source\\gameinfo.txt\n"
    );
    auto result = Source1ImportLogParser::parse(stdOut, QString(), 1);
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessages.isEmpty());
    QVERIFY(result.errorMessages.first().contains(QStringLiteral("Unable to load source 1 mod gameinfo.txt")));
}

void TestToolLogParsers::testSource1ImportLogParser_FailedToMakePathRelative()
{
    QString stdOut = QStringLiteral(
        "Importing 1 resources...\n"
        "- (1/1) C:\\external\\particles\\custom.pcf\n"
        "WARNING: Failed to make path 'C:\\external\\particles\\custom.pcf' relative!\n"
        "-----------------------------------------------------------------\n"
        "SKIPPED:\n"
        " C:\\external\\particles\\custom.pcf\n"
        "-----------------------------------------------------------------\n"
        " OK: 0 imported, 0 failed, 1 skipped, 0 unknown, 0m:00s\n"
    );
    auto result = Source1ImportLogParser::parse(stdOut, QString(), 0);
    QVERIFY(!result.success);
    QVERIFY(!result.warnings.isEmpty());
    QVERIFY(!result.errorMessages.isEmpty());
    QVERIFY(result.errorMessages.first().contains(QStringLiteral("Failed to make path")));
}

void TestToolLogParsers::testResourceCompilerLogParser_SuccessSingleCompile()
{
    QString stdOut = QStringLiteral(
        "Found 1 file(s) matching nonrecursive specification \"d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\\error.vpcf\"\n"
        "Split target filename \"d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\\error.vpcf\" into:\n"
        "  ContentRoot = \"D:/SteamLibrary/steamapps/common/Counter-Strike Global Offensive/content/\"\n"
        "  ModName = \"csgo_addons/test\"\n"
        "  RelativePath = \"particles/error/error.vpcf\"\n"
        "d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\content\\csgo_addons\\test\\particles\\error\\error.vpcf:\n"
        " - Next resource strong-references: materials/debug/particleerror.vtex)\n"
        " - Wrote to: d:\\steamlibrary\\steamapps\\common\\counter-strike global offensive\\game\\csgo_addons\\test\\particles\\error\\error.vpcf_c\n"
        "\n"
        "-----------------------------------------------------------------\n"
        " OK: 1 compiled, 0 failed, 0 skipped, 0m:01s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = ResourceCompilerLogParser::parse(stdOut, QString(), 0);

    QVERIFY(result.success);
    QCOMPARE(result.compiledCount, 1);
    QCOMPARE(result.failedCount, 0);
    QCOMPARE(result.skippedCount, 0);
    QCOMPARE(result.compiledVpcfCPaths.size(), 1);
    QVERIFY(result.compiledVpcfCPaths.first().endsWith(QStringLiteral("particles/error/error.vpcf_c")));
}

void TestToolLogParsers::testResourceCompilerLogParser_SuccessSkipped()
{
    QString stdOut = QStringLiteral(
        "-----------------------------------------------------------------\n"
        " OK: 0 compiled, 0 failed, 1 skipped, 0m:01s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = ResourceCompilerLogParser::parse(stdOut, QString(), 0);

    QVERIFY(result.success);
    QCOMPARE(result.compiledCount, 0);
    QCOMPARE(result.skippedCount, 1);
    QCOMPARE(result.failedCount, 0);
}

void TestToolLogParsers::testResourceCompilerLogParser_CompileErrors()
{
    QString stdOut = QStringLiteral(
        "- csgo_addons\\test\\particles\\test_invalid.vpcf\n"
        "D:\\test_invalid.vpcf(4,33): RESOURCE COMPILE ERROR: Expected '=' after member name 'invalid_syntax'\n"
        "                                                                      [FAIL]\n"
        "\n"
        "---------------------------------------------------------------------\n"
        " 1 Compile ERRORS\n"
        " These errors will cause the compile to fail.\n"
        " Look for \"RESOURCE COMPILE ERROR:\" in the log above.\n"
        "---------------------------------------------------------------------\n"
        "  Expected '=' after member name 'invalid_syntax_syntax_error' (before \"%%%$$$\")\n"
        "\n"
        "-----------------------------------------------------------------\n"
        " ERROR: 0 compiled, 1 failed, 0 skipped, 0m:01s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = ResourceCompilerLogParser::parse(stdOut, QString(), 1);

    QVERIFY(!result.success);
    QCOMPARE(result.compiledCount, 0);
    QCOMPARE(result.failedCount, 1);
    QCOMPARE(result.compileErrors.size(), 1);
    QVERIFY(result.compileErrors.first().contains(QStringLiteral("Expected '=' after member name")));
}

void TestToolLogParsers::testResourceCompilerLogParser_NonZeroExitCode()
{
    QString stdOut = QStringLiteral(
        "-----------------------------------------------------------------\n"
        " ERROR: 0 compiled, 0 failed, 0 skipped, 0m:01s\n"
        "-----------------------------------------------------------------\n"
    );

    auto result = ResourceCompilerLogParser::parse(stdOut, QString(), 1);
    QVERIFY(!result.success);
    QCOMPARE(result.compiledCount, 0);
}

void TestToolLogParsers::testSource1ImportTool_BuildArguments_Csgo()
{
    Source1ImportOptions options;
    options.source1GameInfoDir = FilesystemPath(QStringLiteral("C:/Steam/csgo"));
    options.addonName = QStringLiteral("test");
    options.inputPcfPath = FilesystemPath(QStringLiteral("C:/Steam/csgo/particles/test.pcf"));
    options.isCsgo = true;
    options.allowDepthBlend = false;
    options.disableDiffuse = false;

    QStringList args = Source1ImportTool::buildArguments(options);

    QVERIFY(args.contains(QStringLiteral("-retail")));
    QVERIFY(args.contains(QStringLiteral("-nop4")));
    QVERIFY(args.contains(QStringLiteral("-nop4sync")));
    QVERIFY(args.contains(QStringLiteral("-src1gameinfodir")));
    QVERIFY(args.contains(options.source1GameInfoDir.toString()));
    QVERIFY(args.contains(QStringLiteral("-s2addon")));
    QVERIFY(args.contains(QStringLiteral("test")));
    QVERIFY(args.contains(QStringLiteral("-game")));
    QVERIFY(args.contains(QStringLiteral("csgo")));
    QVERIFY(!args.contains(QStringLiteral("-particle_allow_depth_blend")));
    QVERIFY(!args.contains(QStringLiteral("-particle_disable_diffuse")));
    QCOMPARE(args.last(), options.inputPcfPath.toString());
}

void TestToolLogParsers::testSource1ImportTool_BuildArguments_NonCsgo_WithFlags()
{
    Source1ImportOptions options;
    options.source1GameInfoDir = FilesystemPath(QStringLiteral("C:/Steam/cstrike"));
    options.addonName = QStringLiteral("my_addon");
    options.inputPcfPath = FilesystemPath(QStringLiteral("C:/Steam/cstrike/particles/fire.pcf"));
    options.isCsgo = false;
    options.allowDepthBlend = true;
    options.disableDiffuse = true;

    QStringList args = Source1ImportTool::buildArguments(options);

    QVERIFY(args.contains(QStringLiteral("-particle_allow_depth_blend")));
    QVERIFY(args.contains(QStringLiteral("-particle_disable_diffuse")));
    QCOMPARE(args.last(), options.inputPcfPath.toString());
}

void TestToolLogParsers::testResourceCompilerTool_BuildArguments_Default()
{
    ResourceCompilerOptions options;
    options.gameDir = FilesystemPath(QStringLiteral("C:/Steam/CS2/game/csgo"));
    options.inputFiles = {
        QStringLiteral("C:/Steam/CS2/content/csgo_addons/test/particles/a.vpcf"),
        QStringLiteral("C:/Steam/CS2/content/csgo_addons/test/particles/b.vpcf")
    };
    options.forceCompile = true;
    options.verbose = true;

    QStringList args = ResourceCompilerTool::buildArguments(options);

    QVERIFY(args.contains(QStringLiteral("-retail")));
    QVERIFY(args.contains(QStringLiteral("-nop4")));
    QVERIFY(args.contains(QStringLiteral("-f")));
    QVERIFY(args.contains(QStringLiteral("-v")));
    QVERIFY(args.contains(QStringLiteral("-game")));
    QVERIFY(args.contains(options.gameDir.toString()));
    QVERIFY(args.contains(options.inputFiles.at(0)));
    QVERIFY(args.contains(options.inputFiles.at(1)));
}

void TestToolLogParsers::testResourceCompilerTool_BuildArguments_CustomOptions()
{
    ResourceCompilerOptions options;
    options.gameDir = FilesystemPath(QStringLiteral("C:/Steam/CS2/game/csgo"));
    options.inputFiles = { QStringLiteral("C:/path/test.vpcf") };
    options.forceCompile = false;
    options.verbose = false;

    QStringList args = ResourceCompilerTool::buildArguments(options);

    QVERIFY(!args.contains(QStringLiteral("-f")));
    QVERIFY(!args.contains(QStringLiteral("-v")));
    QVERIFY(args.contains(options.inputFiles.at(0)));
}

void TestToolLogParsers::testToolErrors_AllFactoryMethods()
{
    auto e1 = ToolErrors::executableNotFound(QStringLiteral("C:/bin/tool.exe"), QStringLiteral("detail"));
    QVERIFY(e1.isFailure());
    QVERIFY(e1.is(ToolErrorCode::ExecutableNotFound));
    QCOMPARE(e1.domain(), ToolErrors::DomainName);
    QCOMPARE(e1.code(), ErrorCode::FileNotFound);
    QVERIFY(e1.message().contains(QStringLiteral("C:/bin/tool.exe")));
    QCOMPARE(e1.details(), QStringLiteral("detail"));

    auto e2 = ToolErrors::executionFailed(QStringLiteral("tool.exe"), 42, QStringLiteral("stderr output"));
    QVERIFY(e2.is(ToolErrorCode::ExecutionFailed));
    QCOMPARE(e2.code(), ErrorCode::ProcessFailed);
    QVERIFY(e2.message().contains(QStringLiteral("42")));

    auto e3 = ToolErrors::timeout(QStringLiteral("tool.exe"), QStringLiteral("120s"));
    QVERIFY(e3.is(ToolErrorCode::Timeout));
    QCOMPARE(e3.code(), ErrorCode::ProcessTimeout);

    auto e4 = ToolErrors::crashed(QStringLiteral("tool.exe"), QStringLiteral("segfault"));
    QVERIFY(e4.is(ToolErrorCode::Crashed));
    QCOMPARE(e4.code(), ErrorCode::ProcessCrashed);

    auto e5 = ToolErrors::noMatchingFiles(QStringLiteral("test.pcf"));
    QVERIFY(e5.is(ToolErrorCode::NoMatchingFiles));
    QCOMPARE(e5.code(), ErrorCode::FileNotFound);

    auto e6 = ToolErrors::importFailed(QStringLiteral("Corrupt file"), QStringLiteral("details"));
    QVERIFY(e6.is(ToolErrorCode::ImportFailed));
    QCOMPARE(e6.code(), ErrorCode::OperationFailed);

    auto e7 = ToolErrors::compilationFailed(QStringLiteral("Syntax error"), QStringLiteral("details"));
    QVERIFY(e7.is(ToolErrorCode::CompilationFailed));
    QCOMPARE(e7.code(), ErrorCode::OperationFailed);

    auto e8 = ToolErrors::parseError(QStringLiteral("Bad log"), QStringLiteral("details"));
    QVERIFY(e8.is(ToolErrorCode::ParseError));
    QCOMPARE(e8.code(), ErrorCode::CorruptedData);
}

void TestToolLogParsers::testSource1ImportTool_Validation()
{
    // Invalid / nonexistent tool executable
    Source1ImportOptions options;
    options.source1GameInfoDir = FilesystemPath(QStringLiteral("C:/nonexistent_gameinfo"));
    options.addonName = QStringLiteral("test");
    options.inputPcfPath = FilesystemPath(QStringLiteral("C:/nonexistent_particle.pcf"));

    auto res = Source1ImportTool::convertPcf(FilesystemPath(QStringLiteral("C:/nonexistent_tool.exe")), options);
    QVERIFY(res.isFailure());
    QVERIFY(res.error().is(ToolErrorCode::ExecutableNotFound));

    // Valid executable path (fake) but empty gameinfo
    // Note: create a temp file to test nonexistent gameinfo/addon/pcf
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString fakeExePath = QDir(tempDir.path()).filePath(QStringLiteral("fake_tool.exe"));
    QFile fakeExe(fakeExePath);
    QVERIFY(fakeExe.open(QIODevice::WriteOnly));
    fakeExe.close();

    // Empty source1GameInfoDir
    options.source1GameInfoDir = FilesystemPath();
    auto res2 = Source1ImportTool::convertPcf(FilesystemPath(fakeExePath), options);
    QVERIFY(res2.isFailure());
    QCOMPARE(res2.error().code(), ErrorCode::InvalidPath);

    // Empty addon
    options.source1GameInfoDir = FilesystemPath(tempDir.path());
    options.addonName = QString();
    auto res3 = Source1ImportTool::convertPcf(FilesystemPath(fakeExePath), options);
    QVERIFY(res3.isFailure());
    QCOMPARE(res3.error().code(), ErrorCode::InvalidArgument);

    // Nonexistent input PCF
    options.addonName = QStringLiteral("test");
    options.inputPcfPath = FilesystemPath(QStringLiteral("C:/does_not_exist_xyz.pcf"));
    auto res4 = Source1ImportTool::convertPcf(FilesystemPath(fakeExePath), options);
    QVERIFY(res4.isFailure());
    QVERIFY(res4.error().is(ToolErrorCode::NoMatchingFiles));
}

void TestToolLogParsers::testResourceCompilerTool_Validation()
{
    ResourceCompilerOptions options;
    options.gameDir = FilesystemPath(QStringLiteral("C:/nonexistent_cs2"));
    options.inputFiles = { QStringLiteral("test.vpcf") };

    // Nonexistent executable
    auto res = ResourceCompilerTool::compileResources(FilesystemPath(QStringLiteral("C:/nonexistent_compiler.exe")), options);
    QVERIFY(res.isFailure());
    QVERIFY(res.error().is(ToolErrorCode::ExecutableNotFound));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString fakeExePath = QDir(tempDir.path()).filePath(QStringLiteral("fake_compiler.exe"));
    QFile fakeExe(fakeExePath);
    QVERIFY(fakeExe.open(QIODevice::WriteOnly));
    fakeExe.close();

    // Empty gameDir
    options.gameDir = FilesystemPath();
    auto res2 = ResourceCompilerTool::compileResources(FilesystemPath(fakeExePath), options);
    QVERIFY(res2.isFailure());
    QCOMPARE(res2.error().code(), ErrorCode::InvalidPath);

    // Empty inputFiles
    options.gameDir = FilesystemPath(tempDir.path());
    options.inputFiles.clear();
    auto res3 = ResourceCompilerTool::compileResources(FilesystemPath(fakeExePath), options);
    QVERIFY(res3.isFailure());
    QCOMPARE(res3.error().code(), ErrorCode::InvalidArgument);
}

QTEST_MAIN(TestToolLogParsers)
#include "TestToolLogParsers.moc"

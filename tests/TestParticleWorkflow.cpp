#include <QTest>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <thread>
#include <chrono>

#include "Workflow/Particle/ParticleImportOptions.h"
#include "Workflow/Particle/ParticleImportWorkflow.h"
#include "Core/Async/CancellationToken.h"
#include "Workflow/Common/ImportContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Domain/Tool/ToolErrors.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/LogManager.h"

using namespace Core::Error;
using namespace Core::Path;
using namespace Domain::Tool;
using namespace Workflow::Common;
using namespace Workflow::Particle;
using Core::Async::CancellationToken;

class TestParticleWorkflow : public QObject {
    Q_OBJECT

private slots:
    // Tool executable resolution & precondition validation
    void testPrecondition_MissingSource1ImportExe();
    void testPrecondition_MissingResourceCompilerExe();

    // Asset Preservation
    void testExecution_ExistingFilesPreserved();

    // Cooperative Cancellation
    void testCancellation_PreCancelledToken();
    void testCancellation_CancelledDuringExecution();

    // PCF Staging & Cleanup
    void testStaging_LoosePcfStagedAndCleanedUp();
    void testStaging_ExistingPcfPreserved();

    // End-to-End Execution Simulation & Tripartite Diagnostics
    void testEndToEnd_FullPipelineSuccess();
    void testEndToEnd_Source1ImportFails();
    void testEndToEnd_Source1ImportZeroFilesGenerated();
    void testEndToEnd_ResourceCompilerFails();

private:
    // Helpers
    static void createFile(const QString& filePath, const QByteArray& content = "dummy content") {
        QFileInfo fi(filePath);
        QDir().mkpath(fi.path());
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(content);
            file.close();
        }
    }

    static void setupDirs(const QString& s1Dir, const QString& cs2Dir) {
        QDir().mkpath(s1Dir);
        QDir().mkpath(cs2Dir);
        QDir().mkpath(QDir(cs2Dir).filePath("game/csgo"));
    }

    static QString createMockToolBat(const QString& batPath, const QStringList& lines) {
        QFileInfo fi(batPath);
        QDir().mkpath(fi.path());
        QFile file(batPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&file);
            ts << "@echo off\n";
            for (const QString& line : lines) {
                ts << line << "\n";
            }
            file.close();
        }
        return batPath;
    }
};

// ---------------------------------------------------------------------------
// Tool Executable Resolution & Precondition Validation Tests
// ---------------------------------------------------------------------------

void TestParticleWorkflow::testPrecondition_MissingSource1ImportExe()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString pcfPath = temp.filePath("test.pcf");
    createFile(pcfPath);

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(temp.path());
    options.cs2BaseDir = FilesystemPath(temp.path());
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    // Explicit non-existent tool path
    options.source1ImportExe = FilesystemPath(temp.filePath("missing_s1import.exe"));

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);
    QVERIFY(result.isFailure());
    QVERIFY(result.error().is(ToolErrorCode::ExecutableNotFound));
}

void TestParticleWorkflow::testPrecondition_MissingResourceCompilerExe()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString pcfPath = temp.filePath("test.pcf");
    createFile(pcfPath);

    const QString generatedVpcf = temp.filePath("test.vpcf");
    QString s1Tool = temp.filePath("s1import.bat");
    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(temp.path());
    options.cs2BaseDir = FilesystemPath(temp.path());
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(temp.filePath("missing_rc.exe"));

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);
    QVERIFY(result.isFailure());
    QVERIFY(result.error().is(ToolErrorCode::ExecutableNotFound));
}

// ---------------------------------------------------------------------------
// Asset Preservation Tests
// ---------------------------------------------------------------------------

void TestParticleWorkflow::testExecution_ExistingFilesPreserved()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("new_effect.pcf");
    createFile(pcfPath);

    // Existing files in target addon directory
    const QString existingVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/existing_effect.vpcf");
    const QString existingVpcfC = temp.filePath("cs2/game/csgo_addons/test/particles/existing_effect.vpcf_c");
    createFile(existingVpcf, "existing vpcf content");
    createFile(existingVpcfC, "existing compiled content");

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");
    const QString generatedVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/new_effect.vpcf");
    const QString compiledVpcfC = temp.filePath("cs2/game/csgo_addons/test/particles/new_effect.vpcf_c");

    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo  - Wrote to: %1").arg(compiledVpcfC),
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);
    QVERIFY2(result.isSuccess(), qPrintable(result.message()));

    // Existing files must be preserved - workflow never touches non-target files
    QVERIFY(QFile::exists(existingVpcf));
    QVERIFY(QFile::exists(existingVpcfC));
    QCOMPARE(result.value().generatedVpcfFiles.size(), 1);
    QCOMPARE(result.value().compiledVpcfCFiles.size(), 1);
}

// ---------------------------------------------------------------------------
// Cooperative Cancellation Tests
// ---------------------------------------------------------------------------

void TestParticleWorkflow::testCancellation_PreCancelledToken()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString pcfPath = temp.filePath("test.pcf");
    createFile(pcfPath);

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(temp.path());
    options.cs2BaseDir = FilesystemPath(temp.path());
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);

    Core::Async::CancellationToken token;
    token.cancel(); // Pre-cancel

    ParticleImportWorkflow workflow;
    ImportContext ctx(nullptr, token);
    auto result = workflow.execute(options, ctx);
    QVERIFY(result.isCancelled());
}

void TestParticleWorkflow::testCancellation_CancelledDuringExecution()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("test.pcf");
    createFile(pcfPath);

    Core::Async::CancellationToken token;

    // Tool that sleeps slightly so cancellation triggers while/after it runs
    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");

    createMockToolBat(s1Tool, {
        QStringLiteral("powershell -nop -c \"Start-Sleep -Milliseconds 150\""),
        QStringLiteral("echo Writing file \"C:/mock.vpcf\""),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    // Cancel asynchronously after 50ms (during s1import execution)
    std::thread cancelThread([&token]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        token.cancel();
    });

    ParticleImportWorkflow workflow;
    ImportContext ctx(nullptr, token);
    auto result = workflow.execute(options, ctx);
    cancelThread.join();

    QVERIFY(result.isCancelled());
}

// ---------------------------------------------------------------------------
// PCF Staging & Cleanup Tests
// ---------------------------------------------------------------------------

void TestParticleWorkflow::testStaging_LoosePcfStagedAndCleanedUp()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString loosePcf = temp.filePath("external/loose.pcf");
    createFile(loosePcf, "BINARY_DATA");

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");
    const QString generatedVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/loose.vpcf");
    const QString compiledVpcfC = temp.filePath("cs2/game/csgo_addons/test/particles/loose.vpcf_c");

    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo  - Wrote to: %1").arg(compiledVpcfC),
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(loosePcf);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY2(result.isSuccess(), qPrintable(result.message()));

    // Original loose PCF remains intact
    QVERIFY(QFile::exists(loosePcf));

    // Staged temporary copy under s1Dir/particles/loose.pcf has been cleaned up
    const QString stagedPcf = temp.filePath("s1/particles/loose.pcf");
    QVERIFY(!QFile::exists(stagedPcf));
}

void TestParticleWorkflow::testStaging_ExistingPcfPreserved()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    // Native PCF already in <s1Dir>/particles/native.pcf
    const QString nativePcf = temp.filePath("s1/particles/native.pcf");
    createFile(nativePcf, "NATIVE_DATA");

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");
    const QString generatedVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/native.vpcf");
    const QString compiledVpcfC = temp.filePath("cs2/game/csgo_addons/test/particles/native.vpcf_c");

    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo  - Wrote to: %1").arg(compiledVpcfC),
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(nativePcf);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY2(result.isSuccess(), qPrintable(result.message()));

    // Existing native PCF in s1/particles/ must NOT be deleted by cleanup!
    QVERIFY(QFile::exists(nativePcf));
}

// ---------------------------------------------------------------------------
// End-to-End Execution Simulation & Tripartite Diagnostics
// ---------------------------------------------------------------------------

void TestParticleWorkflow::testEndToEnd_FullPipelineSuccess()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("flame.pcf");
    createFile(pcfPath);

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");
    const QString generatedVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/flame.vpcf");
    const QString compiledVpcfC = temp.filePath("cs2/game/csgo_addons/test/particles/flame.vpcf_c");

    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo  - Wrote to: %1").arg(compiledVpcfC),
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.allowDepthBlend = true;
    options.disableDiffuse = true;
    options.isCsgo = false;
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY2(result.isSuccess(), qPrintable(result.message()));
    const auto& val = result.value();
    QCOMPARE(val.totalConverted, 1);
    QCOMPARE(val.totalCompiled, 1);
    QCOMPARE(val.generatedVpcfFiles.size(), 1);
    QCOMPARE(val.compiledVpcfCFiles.size(), 1);
    QCOMPARE(val.generatedVpcfFiles.first(), QDir::cleanPath(generatedVpcf));
    QCOMPARE(val.compiledVpcfCFiles.first(), QDir::cleanPath(compiledVpcfC));
}

void TestParticleWorkflow::testEndToEnd_Source1ImportFails()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("bad.pcf");
    createFile(pcfPath);

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");

    // s1import mock outputs error banner and exits with code 1
    createMockToolBat(s1Tool, {
        QStringLiteral("echo Corrupted particle dictionary in file >&2"),
        QStringLiteral("echo ERROR: 0 imported, 1 failed, 0 skipped, 0 unknown, 0m:00s"),
        QStringLiteral("exit /b 1")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY(result.isFailure());

    // Tripartite diagnostic contract verification:
    // 1. High-level operation summary in result.message()
    QVERIFY(result.message().contains(QStringLiteral("PCF conversion failed")) ||
            result.message().contains(QStringLiteral("PCF 粒子转换失败")));

    // 2. Domain / tool error code
    QVERIFY(result.error().is(ToolErrorCode::ImportFailed) ||
            result.error().code() == ErrorCode::OperationFailed);

    // 3. Technical details contain the tool output
    QVERIFY(result.details().contains(QStringLiteral("ERROR: 0 imported, 1 failed")) ||
            result.error().message().contains(QStringLiteral("failed")));
}

void TestParticleWorkflow::testEndToEnd_Source1ImportZeroFilesGenerated()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("empty.pcf");
    createFile(pcfPath);

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");

    // s1import succeeds with 0 files generated (e.g. empty PCF)
    createMockToolBat(s1Tool, {
        QStringLiteral("echo Found no files matching specifications"),
        QStringLiteral("echo OK: 0 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo OK: 1 compiled, 0 failed, 0 skipped")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY(result.isFailure());
    QVERIFY(result.message().contains(QStringLiteral("No .vpcf files generated")) ||
            result.message().contains(QStringLiteral("未找到与规格匹配的文件")) ||
            result.error().is(ToolErrorCode::NoMatchingFiles));
}

void TestParticleWorkflow::testEndToEnd_ResourceCompilerFails()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString s1Dir = temp.filePath("s1");
    const QString cs2Dir = temp.filePath("cs2");
    setupDirs(s1Dir, cs2Dir);

    const QString pcfPath = temp.filePath("flame.pcf");
    createFile(pcfPath);

    const QString s1Tool = temp.filePath("s1import.bat");
    const QString rcTool = temp.filePath("rc.bat");
    const QString generatedVpcf = temp.filePath("cs2/content/csgo_addons/test/particles/flame.vpcf");

    createMockToolBat(s1Tool, {
        QStringLiteral("echo Writing file \"%1\"").arg(generatedVpcf),
        QStringLiteral("echo OK: 1 imported, 0 failed, 0 skipped, 0 unknown, 0m:00s")
    });

    createMockToolBat(rcTool, {
        QStringLiteral("echo RESOURCE COMPILE ERROR: Syntax error in shader definition"),
        QStringLiteral("echo ERROR: 0 compiled, 1 failed, 0 skipped"),
        QStringLiteral("exit /b 1")
    });

    ParticleImportOptions options;
    options.source1GameDir = FilesystemPath(s1Dir);
    options.cs2BaseDir = FilesystemPath(cs2Dir);
    options.addonName = QStringLiteral("test");
    options.sourcePcfPath = FilesystemPath(pcfPath);
    options.source1ImportExe = FilesystemPath(s1Tool);
    options.resourceCompilerExe = FilesystemPath(rcTool);

    ParticleImportWorkflow workflow;
    auto result = workflow.execute(options);

    QVERIFY(result.isFailure());

    // Tripartite diagnostic contract verification:
    // 1. High-level operation summary mentions resource compilation failed
    QVERIFY(result.message().contains(QStringLiteral("Resource compilation failed")) ||
            result.message().contains(QStringLiteral("资源编译失败")));

    // 2. Specific failure reason contains compiler error
    QVERIFY(result.error().message().contains(QStringLiteral("Syntax error in shader definition")) ||
            result.details().contains(QStringLiteral("RESOURCE COMPILE ERROR")));
}

QTEST_MAIN(TestParticleWorkflow)
#include "TestParticleWorkflow.moc"

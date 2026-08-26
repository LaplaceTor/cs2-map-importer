#include <QTest>
#include <memory>
#include <exception>
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Core/Async/TaskResult.h"
#include "Core/Process/ProcessResult.h"
#include "Domain/Game/GameInfoParser.h"

using namespace Core::Error;
using namespace Core::Async;
using namespace Core::Process;
using namespace Domain::Game;

class TestError : public QObject {
    Q_OBJECT

private slots:
    void testErrorCodeBasics();
    void testErrorValueObject();
    void testExceptionLifecycleAndStdExceptionCompatibility();
    void testProcessResultMapping();
    void testTaskResultStructuredError();
    void testTaskResultValueOr();
    void testGameInfoParserStructuredError();
    void testBackwardCompatibilityAliases();
};

void TestError::testErrorCodeBasics()
{
    QCOMPARE(static_cast<int>(ErrorCode::Success), 0);
    QVERIFY(ErrorCode::Unknown != ErrorCode::Success);
    QVERIFY(ErrorCode::FileNotFound != ErrorCode::DirectoryNotFound);
    QVERIFY(ErrorCode::ProcessTimeout != ErrorCode::ProcessFailed);
}

void TestError::testErrorValueObject()
{
    Error defaultErr;
    QVERIFY(defaultErr.isSuccess());
    QCOMPARE(defaultErr.code(), ErrorCode::Success);

    Error customErr(ErrorCode::FileNotFound, QStringLiteral("map.vmf"), QStringLiteral("C:/maps/map.vmf"));
    QVERIFY(customErr.isFailure());
    QCOMPARE(customErr.code(), ErrorCode::FileNotFound);
    QCOMPARE(customErr.message(), QStringLiteral("map.vmf"));
    QCOMPARE(customErr.details(), QStringLiteral("C:/maps/map.vmf"));
    QCOMPARE(customErr.toString(), QStringLiteral("map.vmf (C:/maps/map.vmf)"));

    Error copyErr = customErr;
    QVERIFY(copyErr == customErr);

    Error staticErr = Error::invalidArgument(QStringLiteral("Invalid game type"));
    QCOMPARE(staticErr.code(), ErrorCode::InvalidArgument);
    QCOMPARE(staticErr.message(), QStringLiteral("Invalid game type"));
}

void TestError::testExceptionLifecycleAndStdExceptionCompatibility()
{
    // Test throwing and catching by std::exception
    bool caughtAsStdException = false;
    try {
        throw Exception(ErrorCode::PermissionDenied, QStringLiteral("Access denied to pak01_dir.vpk"), QStringLiteral("Read lock held"));
    } catch (const std::exception& ex) {
        caughtAsStdException = true;
        QVERIFY(QString::fromUtf8(ex.what()).contains("Access denied"));
    }
    QVERIFY(caughtAsStdException);

    // Test catching by Core::Error::Exception and inspecting structured fields
    try {
        throw Exception(ErrorCode::PermissionDenied, QStringLiteral("Access denied to pak01_dir.vpk"), QStringLiteral("Read lock held"));
    } catch (const Exception& ex) {
        QCOMPARE(ex.errorCode(), ErrorCode::PermissionDenied);
        QCOMPARE(ex.error().code(), ErrorCode::PermissionDenied);
        QCOMPARE(ex.message(), QStringLiteral("Access denied to pak01_dir.vpk"));
        QCOMPARE(ex.details(), QStringLiteral("Read lock held"));
        QVERIFY(QString::fromUtf8(ex.what()).contains("Access denied"));

        std::unique_ptr<Exception> cloned(ex.clone());
        QCOMPARE(cloned->errorCode(), ErrorCode::PermissionDenied);
    }
}

void TestError::testProcessResultMapping()
{
    ProcessResult rSuccess{ProcessStatus::Success, 0, QStringLiteral("ok"), QString(), QString()};
    QCOMPARE(rSuccess.toErrorCode(), ErrorCode::Success);
    QVERIFY(rSuccess.toError().isSuccess());

    ProcessResult rTimeout{ProcessStatus::TimedOut, -1, QString(), QString(), QStringLiteral("Timeout after 60s")};
    QCOMPARE(rTimeout.toErrorCode(), ErrorCode::ProcessTimeout);
    QCOMPARE(rTimeout.toError().code(), ErrorCode::ProcessTimeout);
    QCOMPARE(rTimeout.toError().message(), QStringLiteral("Timeout after 60s"));

    ProcessResult rCrashed{ProcessStatus::Crashed, 139, QString(), QStringLiteral("Segmentation fault"), QString()};
    QCOMPARE(rCrashed.toErrorCode(), ErrorCode::ProcessCrashed);
    QCOMPARE(rCrashed.toError().code(), ErrorCode::ProcessCrashed);
    QCOMPARE(rCrashed.toError().details(), QStringLiteral("Segmentation fault"));

    ProcessResult rFailedStart{ProcessStatus::FailedToStart, -1, QString(), QString(), QStringLiteral("Failed to execute")};
    QCOMPARE(rFailedStart.toErrorCode(), ErrorCode::ProcessFailed);
    QCOMPARE(rFailedStart.toError().code(), ErrorCode::ProcessFailed);

    ProcessResult rNonZero{ProcessStatus::NonZeroExit, 2, QString(), QStringLiteral("error"), QString()};
    QCOMPARE(rNonZero.toErrorCode(), ErrorCode::ProcessFailed);
}

void TestError::testTaskResultStructuredError()
{
    // TaskResult<int>
    auto failResult = TaskResult<int>::failure(ErrorCode::CorruptedData, QStringLiteral("Corrupted VPK header"), 10);
    QVERIFY(failResult.isFailure());
    QCOMPARE(failResult.errorCode(), ErrorCode::CorruptedData);
    QCOMPARE(failResult.message(), QStringLiteral("Corrupted VPK header"));
    QVERIFY(failResult.hasValue());
    QCOMPARE(failResult.value(), 10);

    // TaskResult<void>
    auto failVoid = TaskResult<void>::failure(ErrorCode::NetworkError, QStringLiteral("Steam network offline"));
    QVERIFY(failVoid.isFailure());
    QCOMPARE(failVoid.errorCode(), ErrorCode::NetworkError);
    QCOMPARE(failVoid.message(), QStringLiteral("Steam network offline"));
}

void TestError::testTaskResultValueOr()
{
    TaskResult<int> successRes = TaskResult<int>::success(42);
    int fallbackVal = 100;
    QCOMPARE(successRes.valueOr(fallbackVal), 42);

    TaskResult<int> failRes = TaskResult<int>::failure(ErrorCode::Unknown, QStringLiteral("error"));
    QCOMPARE(failRes.valueOr(fallbackVal), 100);
}

void TestError::testGameInfoParserStructuredError()
{
    // Non-existent file
    auto nonExistent = GameInfoParser::parse(Core::Path::FilesystemPath(QStringLiteral("C:/non_existent_folder/gameinfo.txt")));
    QVERIFY(nonExistent.isFailure());
    QCOMPARE(nonExistent.errorCode(), ErrorCode::FileNotFound);

    // Malformed string (unclosed brace)
    QString malformed = QStringLiteral("GameInfo { key value");
    auto badParse = GameInfoParser::parseFromString(malformed);
    QVERIFY(badParse.isFailure());
    QCOMPARE(badParse.errorCode(), ErrorCode::InvalidFile);

    // Valid string
    QString valid = QStringLiteral(
        "\"GameInfo\"\n"
        "{\n"
        "    \"game\" \"TestGame\"\n"
        "}\n"
    );
    auto goodParse = GameInfoParser::parseFromString(valid);
    QVERIFY(goodParse.isSuccess());
    QCOMPARE(goodParse->game(), QStringLiteral("TestGame"));
}

void TestError::testBackwardCompatibilityAliases()
{
    ImportErrorCode oldCode = ImportErrorCode::InvalidFile;
    QCOMPARE(oldCode, ErrorCode::InvalidFile);

    try {
        throw ImportException(ImportErrorCode::DirectoryNotFound, QStringLiteral("Missing dir"));
    } catch (const ImportException& ex) {
        QCOMPARE(ex.errorCode(), ErrorCode::DirectoryNotFound);
        QCOMPARE(ex.message(), QStringLiteral("Missing dir"));
    }
}

QTEST_MAIN(TestError)
#include "TestError.moc"

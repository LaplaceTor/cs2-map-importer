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
    void testTaskResultSeparationOfStatusAndMessage();
    void testTaskResultStatusErrorCodeInvariants();
    void testTaskResultValueOr();
    void testGameInfoParserStructuredError();
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

void TestError::testTaskResultSeparationOfStatusAndMessage()
{
    // Success with message
    auto okResult = TaskResult<int>::success(123, QStringLiteral("Loaded 123 items"));
    QVERIFY(okResult.isSuccess());
    QCOMPARE(okResult.value(), 123);
    QCOMPARE(okResult.message(), QStringLiteral("Loaded 123 items"));
    QVERIFY(okResult.error().isSuccess());

    // Skipped: not an error, distinct status, carries reason as message
    auto skipResult = TaskResult<int>::skipped(QStringLiteral("Already up to date"), 42);
    QVERIFY(skipResult.isSkipped());
    QVERIFY(!skipResult.isFailure());
    QCOMPARE(skipResult.message(), QStringLiteral("Already up to date"));
    QVERIFY(skipResult.hasValue());
    QCOMPARE(skipResult.value(), 42);

    // Failure with operation summary and underlying Error semantics
    Error domainErr(ErrorCode::FileNotFound, QStringLiteral("gameinfo.gi not found"), QStringLiteral("C:/games/csgo/gameinfo.gi"));
    auto failWithSummary = TaskResult<void>::failure(domainErr, QStringLiteral("Validation failed for Counter-Strike 2"));
    QVERIFY(failWithSummary.isFailure());
    QCOMPARE(failWithSummary.errorCode(), ErrorCode::FileNotFound);
    QCOMPARE(failWithSummary.message(), QStringLiteral("Validation failed for Counter-Strike 2"));
    QCOMPARE(failWithSummary.error().message(), QStringLiteral("gameinfo.gi not found"));
    QCOMPARE(failWithSummary.details(), QStringLiteral("C:/games/csgo/gameinfo.gi"));

    // Cancelled: distinct status, carries reason
    auto cancelResult = TaskResult<void>::cancelled(QStringLiteral("User aborted import"));
    QVERIFY(cancelResult.isCancelled());
    QCOMPARE(cancelResult.message(), QStringLiteral("User aborted import"));
    QCOMPARE(cancelResult.errorCode(), ErrorCode::Cancelled);
}

void TestError::testTaskResultStatusErrorCodeInvariants()
{
    // Invariant for TaskResult<int>
    {
        // 1. Success -> ErrorCode::Success
        auto s = TaskResult<int>::success(100, QStringLiteral("All good"));
        QVERIFY(s.isSuccess());
        QVERIFY(!s.isFailure());
        QVERIFY(!s.isCancelled());
        QVERIFY(!s.isSkipped());
        QCOMPARE(s.status(), TaskExecutionStatus::Success);
        QCOMPARE(s.errorCode(), ErrorCode::Success);
        QVERIFY(s.error().isSuccess());

        // 2. Skipped -> ErrorCode::Success (benign non-fault path)
        auto sk = TaskResult<int>::skipped(QStringLiteral("Cache hit"), 100);
        QVERIFY(sk.isSkipped());
        QVERIFY(!sk.isSuccess());
        QVERIFY(!sk.isFailure());
        QVERIFY(!sk.isCancelled());
        QCOMPARE(sk.status(), TaskExecutionStatus::Skipped);
        QCOMPARE(sk.errorCode(), ErrorCode::Success);
        QVERIFY(sk.error().isSuccess());
        QCOMPARE(sk.message(), QStringLiteral("Cache hit"));

        // 3. Cancelled -> ErrorCode::Cancelled
        auto c = TaskResult<int>::cancelled(QStringLiteral("Aborted by user"));
        QVERIFY(c.isCancelled());
        QVERIFY(!c.isSuccess());
        QVERIFY(!c.isFailure());
        QVERIFY(!c.isSkipped());
        QCOMPARE(c.status(), TaskExecutionStatus::Cancelled);
        QCOMPARE(c.errorCode(), ErrorCode::Cancelled);
        QVERIFY(c.error().isFailure());
        QCOMPARE(c.message(), QStringLiteral("Aborted by user"));

        // 4. Failure -> Non-Success ErrorCode
        auto f = TaskResult<int>::failure(ErrorCode::PermissionDenied, QStringLiteral("Access denied"));
        QVERIFY(f.isFailure());
        QVERIFY(!f.isSuccess());
        QVERIFY(!f.isCancelled());
        QVERIFY(!f.isSkipped());
        QCOMPARE(f.status(), TaskExecutionStatus::Failure);
        QVERIFY(f.errorCode() != ErrorCode::Success);
        QCOMPARE(f.errorCode(), ErrorCode::PermissionDenied);
        QVERIFY(f.error().isFailure());
    }

    // Invariant for TaskResult<void>
    {
        // 1. Success -> ErrorCode::Success
        auto s = TaskResult<void>::success(QStringLiteral("Completed"));
        QVERIFY(s.isSuccess());
        QVERIFY(!s.isFailure());
        QVERIFY(!s.isCancelled());
        QVERIFY(!s.isSkipped());
        QCOMPARE(s.status(), TaskExecutionStatus::Success);
        QCOMPARE(s.errorCode(), ErrorCode::Success);
        QVERIFY(s.error().isSuccess());

        // 2. Skipped -> ErrorCode::Success (benign non-fault path)
        auto sk = TaskResult<void>::skipped(QStringLiteral("Up to date"));
        QVERIFY(sk.isSkipped());
        QVERIFY(!sk.isSuccess());
        QVERIFY(!sk.isFailure());
        QVERIFY(!sk.isCancelled());
        QCOMPARE(sk.status(), TaskExecutionStatus::Skipped);
        QCOMPARE(sk.errorCode(), ErrorCode::Success);
        QVERIFY(sk.error().isSuccess());
        QCOMPARE(sk.message(), QStringLiteral("Up to date"));

        // 3. Cancelled -> ErrorCode::Cancelled
        auto c = TaskResult<void>::cancelled(QStringLiteral("User cancel"));
        QVERIFY(c.isCancelled());
        QVERIFY(!c.isSuccess());
        QVERIFY(!c.isFailure());
        QVERIFY(!c.isSkipped());
        QCOMPARE(c.status(), TaskExecutionStatus::Cancelled);
        QCOMPARE(c.errorCode(), ErrorCode::Cancelled);
        QVERIFY(c.error().isFailure());

        // 4. Failure -> Non-Success ErrorCode
        auto f = TaskResult<void>::failure(ErrorCode::CorruptedData, QStringLiteral("Corrupted VPK"));
        QVERIFY(f.isFailure());
        QVERIFY(!f.isSuccess());
        QVERIFY(!f.isCancelled());
        QVERIFY(!f.isSkipped());
        QCOMPARE(f.status(), TaskExecutionStatus::Failure);
        QVERIFY(f.errorCode() != ErrorCode::Success);
        QCOMPARE(f.errorCode(), ErrorCode::CorruptedData);
        QVERIFY(f.error().isFailure());

        // 5. Invariant enforcement against Error::success() passed into failure()
        auto defensiveErr = TaskResult<int>::failure(Error::success());
        QVERIFY(defensiveErr.isFailure());
        QVERIFY(defensiveErr.errorCode() != ErrorCode::Success);
        QCOMPARE(defensiveErr.errorCode(), ErrorCode::OperationFailed);

        auto defensiveVoid = TaskResult<void>::failure(ErrorCode::Success);
        QVERIFY(defensiveVoid.isFailure());
        QVERIFY(defensiveVoid.errorCode() != ErrorCode::Success);
        QCOMPARE(defensiveVoid.errorCode(), ErrorCode::OperationFailed);
    }
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

QTEST_MAIN(TestError)
#include "TestError.moc"

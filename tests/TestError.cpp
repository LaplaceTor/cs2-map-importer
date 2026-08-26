#include <QTest>
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"
#include "Core/Async/TaskResult.h"
#include "Core/Process/ProcessResult.h"

using namespace Core::Error;
using namespace Core::Async;
using namespace Core::Process;

class TestError : public QObject {
    Q_OBJECT

private slots:
    void testErrorCodeBasics();
    void testErrorValueObject();
    void testExceptionLifecycle();
    void testProcessResultMapping();
    void testTaskResultStructuredError();
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

void TestError::testExceptionLifecycle()
{
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


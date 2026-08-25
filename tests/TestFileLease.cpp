#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "Core/FileSystem/FileLease.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Path/FilesystemPath.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace Core::FileSystem;
using namespace Application::Environment;
using namespace Core::Path;

class TestFileLease : public QObject {
    Q_OBJECT

private slots:
    void testBasicAcquireAndRelease();
    void testNonexistentAndInvalidPaths();
    void testMoveSemantics();
    void testDestructorReleasesHandle();
    void testOsLevelExclusionOnWindows();
    void testScopeLifetime();
    void testVpkSignatureLeaseServiceIntegration();
    void testVpkSignatureConflictAndRetry();
};

void TestFileLease::testBasicAcquireAndRelease() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testFilePath = QDir(tempDir.path()).filePath(QStringLiteral("vpk.signatures"));
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("dummy signature content");
    file.close();

    FileLease lease;
    QVERIFY(!lease.isHeld());
    QVERIFY(lease.filePath().isEmpty());

    QString errorMsg;
    bool ok = lease.acquireExclusive(testFilePath, &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QVERIFY(lease.isHeld());
    QCOMPARE(lease.filePath(), QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath()));

    lease.release();
    QVERIFY(!lease.isHeld());
    QVERIFY(lease.filePath().isEmpty());
}

void TestFileLease::testNonexistentAndInvalidPaths() {
    FileLease lease;
    QString errorMsg;

    // Empty path
    QVERIFY(!lease.acquireExclusive(QString(), &errorMsg));
    QVERIFY(!lease.isHeld());
    QVERIFY(!errorMsg.isEmpty());

    // Nonexistent file
    errorMsg.clear();
    QVERIFY(!lease.acquireExclusive(QStringLiteral("C:/nonexistent_file_xyz_12345.bin"), &errorMsg));
    QVERIFY(!lease.isHeld());
    QVERIFY(!errorMsg.isEmpty());

    // Directory path
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    errorMsg.clear();
    QVERIFY(!lease.acquireExclusive(tempDir.path(), &errorMsg));
    QVERIFY(!lease.isHeld());
    QVERIFY(!errorMsg.isEmpty());
}

void TestFileLease::testMoveSemantics() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testFilePath = QDir(tempDir.path()).filePath(QStringLiteral("vpk.signatures"));
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("test data");
    file.close();

    FileLease lease1;
    QVERIFY(lease1.acquireExclusive(testFilePath));
    QVERIFY(lease1.isHeld());

    // Move constructor
    FileLease lease2(std::move(lease1));
    QVERIFY(!lease1.isHeld());
    QVERIFY(lease1.filePath().isEmpty());
    QVERIFY(lease2.isHeld());
    QCOMPARE(lease2.filePath(), QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath()));

    // Move assignment
    FileLease lease3;
    lease3 = std::move(lease2);
    QVERIFY(!lease2.isHeld());
    QVERIFY(lease2.filePath().isEmpty());
    QVERIFY(lease3.isHeld());
    QCOMPARE(lease3.filePath(), QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath()));
}

void TestFileLease::testDestructorReleasesHandle() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testFilePath = QDir(tempDir.path()).filePath(QStringLiteral("vpk.signatures"));
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("destruction test");
    file.close();

    {
        FileLease lease;
        QVERIFY(lease.acquireExclusive(testFilePath));
        QVERIFY(lease.isHeld());
    }

    // After destruction, another lease should be acquired immediately without sharing violation
    FileLease lease2;
    QVERIFY(lease2.acquireExclusive(testFilePath));
    QVERIFY(lease2.isHeld());
}

void TestFileLease::testOsLevelExclusionOnWindows() {
#ifdef Q_OS_WIN
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testFilePath = QDir(tempDir.path()).filePath(QStringLiteral("vpk.signatures"));
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("signature binary blob");
    file.close();

    FileLease lease;
    QVERIFY(lease.acquireExclusive(testFilePath));
    QVERIFY(lease.isHeld());

    const QString nativePath = QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath());

    // Attempt second open with CreateFileW -> MUST fail with sharing violation (ERROR_SHARING_VIOLATION = 32)
    HANDLE hSecond = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    QCOMPARE(hSecond, INVALID_HANDLE_VALUE);
    DWORD err = GetLastError();
    QCOMPARE(err, static_cast<DWORD>(ERROR_SHARING_VIOLATION));

    // Release lease
    lease.release();
    QVERIFY(!lease.isHeld());

    // Second open should now SUCCEED
    hSecond = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    QVERIFY(hSecond != INVALID_HANDLE_VALUE);
    CloseHandle(hSecond);
#endif
}

void TestFileLease::testScopeLifetime() {
#ifdef Q_OS_WIN
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testFilePath = QDir(tempDir.path()).filePath(QStringLiteral("vpk.signatures"));
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("scope lifetime test");
    file.close();

    const QString nativePath = QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath());

    {
        FileLease lease;
        QVERIFY(lease.acquireExclusive(testFilePath));

        // Conflicting open MUST fail here
        HANDLE hConflict = CreateFileW(
            reinterpret_cast<LPCWSTR>(nativePath.utf16()),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        QCOMPARE(hConflict, INVALID_HANDLE_VALUE);
        QCOMPARE(GetLastError(), static_cast<DWORD>(ERROR_SHARING_VIOLATION));
    }

    // Conflicting open MUST succeed here after scope exit
    HANDLE hAllowed = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    QVERIFY(hAllowed != INVALID_HANDLE_VALUE);
    CloseHandle(hAllowed);
#endif
}

void TestFileLease::testVpkSignatureLeaseServiceIntegration() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Create CS2 layout: <tempDir>/game/bin/win64/vpk.signatures
    const QString win64Dir = QDir(tempDir.path()).filePath(QStringLiteral("game/bin/win64"));
    QVERIFY(QDir().mkpath(win64Dir));

    const QString sigPath = QDir(win64Dir).filePath(QStringLiteral("vpk.signatures"));
    QFile file(sigPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("CS2 vpk signature mock data");
    file.close();

    VpkSignatureLeaseService service;
    QVERIFY(!service.isLeaseHeld());
    QVERIFY(service.leasedFilePath().isEmpty());

    // Acquire lease through service
    QString errorMsg;
    bool ok = service.acquireLease(FilesystemPath(tempDir.path()), &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QVERIFY(service.isLeaseHeld());
    QCOMPARE(service.leasedFilePath(), QDir::toNativeSeparators(QFileInfo(sigPath).absoluteFilePath()));

#ifdef Q_OS_WIN
    // Verify external tool opening fails
    const QString nativePath = service.leasedFilePath();
    HANDLE hTool = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    QCOMPARE(hTool, INVALID_HANDLE_VALUE);
    QCOMPARE(GetLastError(), static_cast<DWORD>(ERROR_SHARING_VIOLATION));
#endif

    // Release lease
    service.releaseLease();
    QVERIFY(!service.isLeaseHeld());

#ifdef Q_OS_WIN
    // External tool opening now succeeds
    HANDLE hToolAfter = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    QVERIFY(hToolAfter != INVALID_HANDLE_VALUE);
    CloseHandle(hToolAfter);
#endif
}

void TestFileLease::testVpkSignatureConflictAndRetry() {
#ifdef Q_OS_WIN
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString win64Dir = QDir(tempDir.path()).filePath(QStringLiteral("game/bin/win64"));
    QVERIFY(QDir().mkpath(win64Dir));

    const QString sigPath = QDir(win64Dir).filePath(QStringLiteral("vpk.signatures"));
    QFile file(sigPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("CS2 active process simulation");
    file.close();

    const QString nativePath = QDir::toNativeSeparators(QFileInfo(sigPath).absoluteFilePath());

    // 1. Simulate external CS2 process holding vpk.signatures
    HANDLE hExternalCs2 = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    QVERIFY(hExternalCs2 != INVALID_HANDLE_VALUE);

    // 2. Service tries to acquire exclusive lease while CS2 is holding it -> must FAIL
    VpkSignatureLeaseService service;
    QString errorMsg;
    bool ok = service.acquireLease(FilesystemPath(tempDir.path()), &errorMsg);
    QVERIFY(!ok);
    QVERIFY(!service.isLeaseHeld());
    QVERIFY(!errorMsg.isEmpty());

    // 3. User closes CS2 (simulate closing handle)
    CloseHandle(hExternalCs2);

    // 4. Retry acquiring lease -> must SUCCEED
    ok = service.acquireLease(FilesystemPath(tempDir.path()), &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QVERIFY(service.isLeaseHeld());

    service.releaseLease();
#endif
}

QTEST_MAIN(TestFileLease)
#include "TestFileLease.moc"


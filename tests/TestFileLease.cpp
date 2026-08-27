#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "Core/FileSystem/FileLease.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

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
using Core::Result;
using Core::ResultStatus;

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
    void testVpkSignatureServiceInstallationLifecycle();
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

    FileLeaseResult res = lease.acquireExclusive(testFilePath);
    QVERIFY2(res.isSuccess(), qPrintable(res.message));
    QCOMPARE(res.error, FileLeaseError::None);
    QVERIFY(lease.isHeld());
    QCOMPARE(lease.filePath(), QDir::toNativeSeparators(QFileInfo(testFilePath).absoluteFilePath()));

    lease.release();
    QVERIFY(!lease.isHeld());
    QVERIFY(lease.filePath().isEmpty());
}

void TestFileLease::testNonexistentAndInvalidPaths() {
    FileLease lease;

    // Empty path
    FileLeaseResult emptyRes = lease.acquireExclusive(QString());
    QVERIFY(!emptyRes.isSuccess());
    QCOMPARE(emptyRes.error, FileLeaseError::InvalidPath);
    QVERIFY(!lease.isHeld());

    // Nonexistent file
    FileLeaseResult missingRes = lease.acquireExclusive(QStringLiteral("C:/nonexistent_file_xyz_12345.bin"));
    QVERIFY(!missingRes.isSuccess());
    QCOMPARE(missingRes.error, FileLeaseError::NotFound);
    QVERIFY(!lease.isHeld());

    // Directory path
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    FileLeaseResult dirRes = lease.acquireExclusive(tempDir.path());
    QVERIFY(!dirRes.isSuccess());
    QCOMPARE(dirRes.error, FileLeaseError::InvalidPath);
    QVERIFY(!lease.isHeld());
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
    QVERIFY(lease1.acquireExclusive(testFilePath).isSuccess());
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
        QVERIFY(lease.acquireExclusive(testFilePath).isSuccess());
        QVERIFY(lease.isHeld());
    }

    // After destruction, another lease should be acquired immediately without sharing violation
    FileLease lease2;
    QVERIFY(lease2.acquireExclusive(testFilePath).isSuccess());
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
    QVERIFY(lease.acquireExclusive(testFilePath).isSuccess());
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
        QVERIFY(lease.acquireExclusive(testFilePath).isSuccess());

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
    auto res = service.acquireLease(FilesystemPath(tempDir.path()));
    QVERIFY(res.isSuccess());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::Acquired);
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

    // 2. Service tries to acquire exclusive lease while CS2 is holding it -> must FAIL with AlreadyInUse
    VpkSignatureLeaseService service;
    auto res = service.acquireLease(FilesystemPath(tempDir.path()));
    QVERIFY(res.isFailure());
    QVERIFY(res.hasValue());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::AlreadyInUse);
    QVERIFY(!service.isLeaseHeld());
    QVERIFY(!res.message().isEmpty());

    // 3. User closes CS2 (simulate closing handle)
    CloseHandle(hExternalCs2);

    // 4. Retry acquiring lease -> must SUCCEED
    res = service.acquireLease(FilesystemPath(tempDir.path()));
    QVERIFY(res.isSuccess());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::Acquired);
    QVERIFY(service.isLeaseHeld());

    service.releaseLease();
#endif
}

void TestFileLease::testVpkSignatureServiceInstallationLifecycle() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString win64Dir = QDir(tempDir.path()).filePath(QStringLiteral("game/bin/win64"));
    QVERIFY(QDir().mkpath(win64Dir));
    const QString sigPath = QDir(win64Dir).filePath(QStringLiteral("vpk.signatures"));
    QFile file(sigPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("CS2 signatures");
    file.close();

    VpkSignatureLeaseService service;

    // 1. Non-CS2 installation (e.g. CSS) -> should be Inactive and not hold lease
    GameInstallation cssInst;
    cssInst.setType(Domain::Game::GameType::CSS);
    cssInst.setBaseDirectory(FilesystemPath(tempDir.path()));
    cssInst.setValid(true);

    auto res = service.updateInstallation(cssInst);
    QVERIFY(res.isSuccess());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::Inactive);
    QVERIFY(!service.isLeaseHeld());

    // 2. Valid CS2 installation -> should automatically acquire lease
    GameInstallation cs2Inst;
    cs2Inst.setType(Domain::Game::GameType::CS2);
    cs2Inst.setBaseDirectory(FilesystemPath(tempDir.path()));
    cs2Inst.setValid(true);

    res = service.updateInstallation(cs2Inst);
    QVERIFY(res.isSuccess());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::Acquired);
    QVERIFY(service.isLeaseHeld());

    // 3. Reset / invalid installation -> should automatically release lease
    GameInstallation invalidInst;
    res = service.updateInstallation(invalidInst);
    QVERIFY(res.isSuccess());
    QCOMPARE(res.value().status, VpkSignatureLeaseStatus::Inactive);
    QVERIFY(!service.isLeaseHeld());
}

QTEST_MAIN(TestFileLease)
#include "TestFileLease.moc"

#include <future>
#include <vector>

#include <QTest>
#include <QTemporaryDir>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"

#include "TestPackFixtures.h"

using namespace TestPackFixtures;
using Core::Path::FilesystemPath;
using Domain::Package::PackArchive;
using Domain::Package::PackArchivePool;

class TestPackArchivePool : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void missingFileFails();
    void invalidPathFails();
    void getOrOpenCachesInstance();
    void pathIsCaseInsensitive();
    void concurrentGetOrOpenIsThreadSafe();
    void clearEmptiesPool();

private:
    QTemporaryDir m_dir;
    QString m_vpkPath1;
    QString m_vpkPath2;
};

void TestPackArchivePool::initTestCase() {
    QVERIFY(m_dir.isValid());

    m_vpkPath1 = m_dir.filePath(QStringLiteral("test1_dir.vpk"));
    QVERIFY(createTestVpk(m_vpkPath1, {
        {QStringLiteral("materials/test1.vmt"), QByteArrayLiteral("test 1 vmt")},
    }));

    m_vpkPath2 = m_dir.filePath(QStringLiteral("test2_dir.vpk"));
    QVERIFY(createTestVpk(m_vpkPath2, {
        {QStringLiteral("materials/test2.vmt"), QByteArrayLiteral("test 2 vmt")},
    }));
}

void TestPackArchivePool::missingFileFails() {
    PackArchivePool pool;
    auto res = pool.getOrOpen(FilesystemPath(m_dir.filePath(QStringLiteral("nonexistent.vpk"))));
    QVERIFY(res.isFailure());
    QCOMPARE(res.errorCode(), Core::Error::ErrorCode::FileNotFound);
    QCOMPARE(pool.size(), static_cast<std::size_t>(0));
}

void TestPackArchivePool::invalidPathFails() {
    PackArchivePool pool;
    auto res = pool.getOrOpen(FilesystemPath(QString()));
    QVERIFY(res.isFailure());
    QCOMPARE(res.errorCode(), Core::Error::ErrorCode::InvalidPath);
}

void TestPackArchivePool::getOrOpenCachesInstance() {
    PackArchivePool pool;
    const FilesystemPath path(m_vpkPath1);

    QVERIFY(!pool.contains(path));
    QCOMPARE(pool.size(), static_cast<std::size_t>(0));

    auto first = pool.getOrOpen(path);
    QVERIFY(first.isSuccess());
    QVERIFY(first.value() != nullptr);
    QVERIFY(pool.contains(path));
    QCOMPARE(pool.size(), static_cast<std::size_t>(1));

    auto second = pool.getOrOpen(path);
    QVERIFY(second.isSuccess());
    QCOMPARE(first.value().get(), second.value().get());
    QCOMPARE(pool.size(), static_cast<std::size_t>(1));
}

void TestPackArchivePool::pathIsCaseInsensitive() {
    PackArchivePool pool;
    const FilesystemPath pathLower(m_vpkPath1.toLower());
    const FilesystemPath pathUpper(m_vpkPath1.toUpper());

    auto first = pool.getOrOpen(pathLower);
    QVERIFY(first.isSuccess());

    auto second = pool.getOrOpen(pathUpper);
    QVERIFY(second.isSuccess());
    QCOMPARE(first.value().get(), second.value().get());
    QCOMPARE(pool.size(), static_cast<std::size_t>(1));
}

void TestPackArchivePool::concurrentGetOrOpenIsThreadSafe() {
    PackArchivePool pool;
    const FilesystemPath path1(m_vpkPath1);
    const FilesystemPath path2(m_vpkPath2);

    constexpr int kThreadCount = 8;
    constexpr int kIterations = 25;

    std::vector<std::future<bool>> futures;
    futures.reserve(kThreadCount);

    for (int t = 0; t < kThreadCount; ++t) {
        futures.push_back(std::async(std::launch::async, [&pool, &path1, &path2, t]() {
            for (int i = 0; i < kIterations; ++i) {
                const auto& targetPath = (t % 2 == 0) ? path1 : path2;
                auto res = pool.getOrOpen(targetPath);
                if (res.isFailure() || !res.value() || !res.value()->isOpen()) {
                    return false;
                }
            }
            return true;
        }));
    }

    for (auto& f : futures) {
        QVERIFY(f.get());
    }

    QCOMPARE(pool.size(), static_cast<std::size_t>(2));
}

void TestPackArchivePool::clearEmptiesPool() {
    PackArchivePool pool;
    const FilesystemPath path(m_vpkPath1);

    auto res = pool.getOrOpen(path);
    QVERIFY(res.isSuccess());
    QCOMPARE(pool.size(), static_cast<std::size_t>(1));

    pool.clear();
    QCOMPARE(pool.size(), static_cast<std::size_t>(0));
    QVERIFY(!pool.contains(path));

    auto reopen = pool.getOrOpen(path);
    QVERIFY(reopen.isSuccess());
    QCOMPARE(pool.size(), static_cast<std::size_t>(1));
}

QTEST_MAIN(TestPackArchivePool)
#include "TestPackArchivePool.moc"


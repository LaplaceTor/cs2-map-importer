#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include "Application/Soundscape/SoundscapeConvertService.h"
#include "Core/Path/FilesystemPath.h"

using namespace Application::Soundscape;

class TestSoundscapeApplication : public QObject {
    Q_OBJECT

private slots:
    void testConvertContent();
    void testConvertFile();
    void testConvertMapSoundscapes();
    void testConvertMapSoundscapesAsync();
};

void TestSoundscapeApplication::testConvertContent() {
    const QString script = QStringLiteral(
        "\"office.indoor\"\n"
        "{\n"
        "    \"dsp\" \"5\"\n"
        "    \"playlooping\"\n"
        "    {\n"
        "        \"volume\" \"0.3\"\n"
        "        \"pitch\" \"100\"\n"
        "        \"wave\" \"ambient/office/office_ambient1.wav\"\n"
        "    }\n"
        "    \"playrandom\"\n"
        "    {\n"
        "        \"time\" \"10, 30\"\n"
        "        \"volume\" \"0.1, 0.4\"\n"
        "        \"pitch\" \"90, 110\"\n"
        "        \"wave\" \"ambient/office/phone.wav\"\n"
        "    }\n"
        "}\n"
    );

    SoundscapeConvertService service;
    auto res = service.convertContent(script, QStringLiteral("office_test"));
    QVERIFY(res.isSuccess());

    const auto& val = res.value();
    QVERIFY(val.succeeded);
    QCOMPARE(val.totalSoundscapesConverted, 1);
    QCOMPARE(val.totalSoundEventsGenerated, 3); // 1 master + 2 children
    QCOMPARE(val.uniqueRawSoundAssets.size(), 2);
    QVERIFY(val.uniqueRawSoundAssets.contains(QStringLiteral("sound/ambient/office/office_ambient1.wav")));
    QVERIFY(val.uniqueRawSoundAssets.contains(QStringLiteral("sound/ambient/office/phone.wav")));
}

void TestSoundscapeApplication::testConvertFile() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString sourceFilePath = tempDir.filePath(QStringLiteral("soundscapes_testmap.vsc"));
    const QString targetFilePath = tempDir.filePath(QStringLiteral("soundevents_testmap.vsndevts"));

    QFile srcFile(sourceFilePath);
    QVERIFY(srcFile.open(QIODevice::WriteOnly | QIODevice::Text));
    srcFile.write(
        "\"testmap.room\"\n"
        "{\n"
        "    \"dsp\" \"6\"\n"
        "    \"playlooping\"\n"
        "    {\n"
        "        \"volume\" \"0.4\"\n"
        "        \"wave\" \"ambient/test/room.wav\"\n"
        "    }\n"
        "}\n"
    );
    srcFile.close();

    SoundscapeConvertService service;
    auto res = service.convertFile(
        Core::Path::FilesystemPath(sourceFilePath),
        Core::Path::FilesystemPath(targetFilePath));

    QVERIFY(res.isSuccess());
    QVERIFY(QFile::exists(targetFilePath));

    QFile outFile(targetFilePath);
    QVERIFY(outFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString outputContent = QString::fromUtf8(outFile.readAll());
    outFile.close();

    QVERIFY(outputContent.contains(QStringLiteral("testmap.room =")));
    QVERIFY(outputContent.contains(QStringLiteral("reverb_6_largeRoom")));
    QVERIFY(outputContent.contains(QStringLiteral("sounds/ambient/test/room.vsnd")));
}

void TestSoundscapeApplication::testConvertMapSoundscapes() {
    QTemporaryDir s1Dir;
    QTemporaryDir s2Dir;
    QVERIFY(s1Dir.isValid() && s2Dir.isValid());

    QDir(s1Dir.path()).mkpath(QStringLiteral("scripts"));
    const QString script1 = s1Dir.filePath(QStringLiteral("scripts/soundscapes_de_dust2.vsc"));
    const QString script2 = s1Dir.filePath(QStringLiteral("scripts/soundscapes_de_inferno.vsc"));

    QFile f1(script1);
    QVERIFY(f1.open(QIODevice::WriteOnly | QIODevice::Text));
    f1.write("\"dust2.base\" { \"dsp\" \"21\" \"playlooping\" { \"wave\" \"ambient/wind.wav\" } }\n");
    f1.close();

    QFile f2(script2);
    QVERIFY(f2.open(QIODevice::WriteOnly | QIODevice::Text));
    f2.write("\"inferno.base\" { \"dsp\" \"20\" \"playlooping\" { \"wave\" \"ambient/city.wav\" } }\n");
    f2.close();

    ConvertSoundscapeRequest req;
    req.s1ScriptsDir = Core::Path::FilesystemPath(s1Dir.filePath(QStringLiteral("scripts")));
    req.s2ContentDir = Core::Path::FilesystemPath(s2Dir.path());
    req.mapName = QStringLiteral("de_dust2");

    SoundscapeConvertService service;
    auto res = service.convertMapSoundscapes(req);
    QVERIFY(res.isSuccess());

    const auto& val = res.value();
    QCOMPARE(val.totalSoundscapesConverted, 2);
    QCOMPARE(val.generatedFiles.size(), 2);
    QVERIFY(QFile::exists(s2Dir.filePath(QStringLiteral("soundevents/soundevents_de_dust2.vsndevts"))));
    QVERIFY(QFile::exists(s2Dir.filePath(QStringLiteral("soundevents/soundevents_de_inferno.vsndevts"))));
}

void TestSoundscapeApplication::testConvertMapSoundscapesAsync() {
    QTemporaryDir s1Dir;
    QTemporaryDir s2Dir;
    QVERIFY(s1Dir.isValid() && s2Dir.isValid());

    QDir(s1Dir.path()).mkpath(QStringLiteral("scripts"));
    const QString script = s1Dir.filePath(QStringLiteral("scripts/soundscapes_nuke.vsc"));
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("\"nuke.yard\" { \"dsp\" \"21\" \"playlooping\" { \"wave\" \"ambient/nuke/wind.wav\" } }\n");
    f.close();

    ConvertSoundscapeRequest req;
    req.s1ScriptsDir = Core::Path::FilesystemPath(s1Dir.filePath(QStringLiteral("scripts")));
    req.s2ContentDir = Core::Path::FilesystemPath(s2Dir.path());
    req.mapName = QStringLiteral("de_nuke");

    SoundscapeConvertService service;
    QEventLoop loop;
    Core::Result<ConvertSoundscapeResult> asyncResult;
    bool callbackReceived = false;

    service.convertMapSoundscapesAsync(req, nullptr, [&](const Core::Result<ConvertSoundscapeResult>& res) {
        asyncResult = res;
        callbackReceived = true;
        loop.quit();
    });

    // Timeout safety
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(callbackReceived);
    QVERIFY(asyncResult.isSuccess());
    QCOMPARE(asyncResult.value().totalSoundscapesConverted, 1);
}

QTEST_MAIN(TestSoundscapeApplication)
#include "TestSoundscapeApplication.moc"


#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "Domain/Audio/Range.h"
#include "Domain/Audio/DspPresetRegistry.h"
#include "Domain/Audio/SoundLevelMapper.h"
#include "Domain/Audio/SoundscapeDefinition.h"
#include "Domain/Audio/SoundEvent.h"
#include "Domain/Audio/SoundscapeParser.h"
#include "Domain/Audio/SoundscapeToSoundEventConverter.h"
#include "Domain/Audio/SoundEventKv3Writer.h"
#include "Core/Path/FilesystemPath.h"

using namespace Domain::Audio;

class TestSoundscapeDomain : public QObject {
    Q_OBJECT

private slots:
    void testRangeParsing();
    void testVector3Parsing();
    void testDspPresetRegistry();
    void testSoundLevelMapper();
    void testSoundscapeParserDirect();
    void testSoundscapeConverterPitchMath();
    void testSoundscapeConverterProperties();
    void testSoundEventKv3Writer();
    void testParseOfficialSoundscapeFiles();
};

void TestSoundscapeDomain::testRangeParsing() {
    // Single values
    auto r1 = DoubleRange::fromString(QStringLiteral("0.4"));
    QVERIFY(r1.has_value());
    QVERIFY(r1->isFixed());
    QCOMPARE(r1->min(), 0.4);
    QCOMPARE(r1->max(), 0.4);

    // Leading dot
    auto r2 = DoubleRange::fromString(QStringLiteral(".2"));
    QVERIFY(r2.has_value());
    QCOMPARE(r2->min(), 0.2);

    // Comma separated range
    auto r3 = DoubleRange::fromString(QStringLiteral("13, 35"));
    QVERIFY(r3.has_value());
    QVERIFY(!r3->isFixed());
    QCOMPARE(r3->min(), 13.0);
    QCOMPARE(r3->max(), 35.0);
    QCOMPARE(r3->delta(), 22.0);

    // Space separated range
    auto r4 = DoubleRange::fromString(QStringLiteral("0.15 0.3"));
    QVERIFY(r4.has_value());
    QCOMPARE(r4->min(), 0.15);
    QCOMPARE(r4->max(), 0.3);

    // Integers
    auto r5 = IntRange::fromString(QStringLiteral("85, 105"));
    QVERIFY(r5.has_value());
    QCOMPARE(r5->min(), 85);
    QCOMPARE(r5->max(), 105);
}

void TestSoundscapeDomain::testVector3Parsing() {
    auto v1 = Vector3::fromString(QStringLiteral("100, 200, 300"));
    QVERIFY(v1.has_value());
    QCOMPARE(v1->x, 100.0);
    QCOMPARE(v1->y, 200.0);
    QCOMPARE(v1->z, 300.0);

    auto v2 = Vector3::fromString(QStringLiteral("-1005.25, -509.18, 163.83;"));
    QVERIFY(v2.has_value());
    QCOMPARE(v2->x, -1005.25);
    QCOMPARE(v2->y, -509.18);
    QCOMPARE(v2->z, 163.83);

    auto v3 = Vector3::fromString(QStringLiteral("1437 2714 339"));
    QVERIFY(v3.has_value());
    QCOMPARE(v3->x, 1437.0);
    QCOMPARE(v3->y, 2714.0);
    QCOMPARE(v3->z, 339.0);
}

void TestSoundscapeDomain::testDspPresetRegistry() {
    // 0 is off
    auto dsp0 = DspPresetRegistry::lookupByIndex(0);
    QVERIFY(dsp0.has_value());
    QVERIFY(!dsp0->overrideDsp);

    // 6 is Large Room
    auto dsp6 = DspPresetRegistry::lookupByIndex(6);
    QVERIFY(dsp6.has_value());
    QVERIFY(dsp6->overrideDsp);
    QCOMPARE(dsp6->s2PresetName, QStringLiteral("reverb_6_largeRoom"));

    // 21 is Outside Street
    auto dsp21 = DspPresetRegistry::lookupByIndex(21);
    QVERIFY(dsp21.has_value());
    QVERIFY(dsp21->overrideDsp);
    QCOMPARE(dsp21->s2PresetName, QStringLiteral("reverb_21_outsideStreet"));
    QCOMPARE(dsp21->defaultReverbWet, 1.0);

    // String lookup
    auto dspStr = DspPresetRegistry::lookupByString(QStringLiteral("20"));
    QVERIFY(dspStr.has_value());
    QCOMPARE(dspStr->s2PresetName, QStringLiteral("reverb_20_outsideAlley"));
}

void TestSoundscapeDomain::testSoundLevelMapper() {
    auto db1 = SoundLevelMapper::parseSoundLevelToDecibels(QStringLiteral("SNDLVL_75dB"));
    QVERIFY(db1.has_value());
    QCOMPARE(*db1, 75);

    auto db2 = SoundLevelMapper::parseSoundLevelToDecibels(QStringLiteral("SNDLVL_NORM"));
    QVERIFY(db2.has_value());
    QCOMPARE(*db2, 75);

    auto db3 = SoundLevelMapper::parseSoundLevelToDecibels(QStringLiteral("80"));
    QVERIFY(db3.has_value());
    QCOMPARE(*db3, 80);

    auto curve = SoundLevelMapper::createDistanceVolumeCurve(QStringLiteral("SNDLVL_75dB"));
    QCOMPARE(static_cast<int>(curve.size()), 2);
    QCOMPARE(curve[0].x, 0.0);
    QCOMPARE(curve[0].y, 1.0);
    QCOMPARE(curve[1].x, 800.0);
    QCOMPARE(curve[1].y, 0.0);
}

void TestSoundscapeDomain::testSoundscapeParserDirect() {
    const QString script = QStringLiteral(
        "\"dust2.indoors\"\n"
        "{\n"
        "    \"dsp\" \"6\"\n"
        "    \"fadetime\" \"0.2\"\n"
        "    \"playlooping\"\n"
        "    {\n"
        "        \"volume\" \".4\"\n"
        "        \"pitch\" \"100\"\n"
        "        \"wave\" \"ambient/wind/wind_bass.wav\"\n"
        "    }\n"
        "    \"playrandom\"\n"
        "    {\n"
        "        \"time\" \"13,20\"\n"
        "        \"volume\" \".15,.4\"\n"
        "        \"pitch\" \"85,105\"\n"
        "        \"soundlevel\" \"SNDLVL_75dB\"\n"
        "        \"position\" \"random\"\n"
        "        \"rndwave\"\n"
        "        {\n"
        "            \"wave\" \"ambient/misc/rock1.wav\"\n"
        "            \"wave\" \"ambient/misc/rock2.wav\"\n"
        "        }\n"
        "    }\n"
        "    \"playsoundscape\"\n"
        "    {\n"
        "        \"name\" \"dust2.birds\"\n"
        "    }\n"
        "}\n"
    );

    auto parseRes = SoundscapeParser::parseString(script);
    QVERIFY(parseRes.isSuccess());
    const auto& defs = parseRes.value();
    QCOMPARE(static_cast<int>(defs.size()), 1);

    const auto& def = defs.front();
    QCOMPARE(def.name, QStringLiteral("dust2.indoors"));
    QCOMPARE(def.dspIndex.value_or(-1), 6);
    QCOMPARE(def.fadeTime.value_or(0.0), 0.2);
    QCOMPARE(static_cast<int>(def.elements.size()), 3);

    const auto* loop = std::get_if<PlayLoopingElement>(&def.elements[0]);
    QVERIFY(loop != nullptr);
    QCOMPARE(loop->wave, QStringLiteral("ambient/wind/wind_bass.wav"));
    QCOMPARE(loop->volume.min(), 0.4);

    const auto* randElem = std::get_if<PlayRandomElement>(&def.elements[1]);
    QVERIFY(randElem != nullptr);
    QCOMPARE(randElem->timeInterval.min(), 13.0);
    QCOMPARE(randElem->timeInterval.max(), 20.0);
    QCOMPARE(randElem->pitch.min(), 85.0);
    QCOMPARE(randElem->pitch.max(), 105.0);
    QCOMPARE(randElem->waves.size(), 2);

    const auto* scapeElem = std::get_if<PlaySoundscapeElement>(&def.elements[2]);
    QVERIFY(scapeElem != nullptr);
    QCOMPARE(scapeElem->targetSoundscape, QStringLiteral("dust2.birds"));
}

void TestSoundscapeDomain::testSoundscapeConverterPitchMath() {
    SoundscapeDefinition def;
    def.name = QStringLiteral("test.pitch");

    PlayRandomElement randElem;
    randElem.pitch = DoubleRange(85.0, 105.0);
    randElem.volume = DoubleRange(0.2, 0.5);
    randElem.timeInterval = DoubleRange(5.0, 10.0);
    randElem.waves.append(QStringLiteral("ambient/test/sound.wav"));
    def.elements.push_back(randElem);

    auto result = SoundscapeToSoundEventConverter::convert(def);
    QCOMPARE(static_cast<int>(result.soundEvents.size()), 2); // 1 master + 1 child

    const auto& child = result.soundEvents[1];
    QCOMPARE(child.name, QStringLiteral("test.pitch.part1"));
    QCOMPARE(child.pitch, 1.0);
    QVERIFY(child.pitchRandomMin.has_value());
    QVERIFY(child.pitchRandomMax.has_value());

    // CRITICAL: Must be -0.15 and +0.05, NOT 85 and 105!
    QCOMPARE(child.pitchRandomMin.value(), -0.15);
    QCOMPARE(child.pitchRandomMax.value(), 0.05);

    // Volume min and random delta
    QCOMPARE(child.volume, 0.2);
    QVERIFY(child.volumeRandomMax.has_value());
    QCOMPARE(child.volumeRandomMax.value(), 0.3);
}

void TestSoundscapeDomain::testSoundscapeConverterProperties() {
    SoundscapeDefinition def;
    def.name = QStringLiteral("inferno.outside");
    def.dspIndex = 21;
    def.dspVolume = 0.8;
    def.fadeTime = 1.5;

    PlayLoopingElement loop;
    loop.wave = QStringLiteral("ambient/inferno/exterior_01.wav");
    loop.volume = DoubleRange(0.5);
    loop.pitch = DoubleRange(100.0);
    loop.origin = Vector3(1437.0, 2714.0, 339.0);
    loop.soundLevel = QStringLiteral("SNDLVL_80dB");
    def.elements.push_back(loop);

    PlaySoundscapeElement scape;
    scape.targetSoundscape = QStringLiteral("inferno.birds");
    def.elements.push_back(scape);

    ConversionOptions options;
    options.mixgroup = QStringLiteral("Inferno");

    auto result = SoundscapeToSoundEventConverter::convert(def, options);
    QCOMPARE(static_cast<int>(result.soundEvents.size()), 2); // 1 master + 1 child loop

    const auto& master = result.soundEvents[0];
    QCOMPARE(master.name, QStringLiteral("inferno.outside"));
    QCOMPARE(master.mixgroup, QStringLiteral("Inferno"));
    QCOMPARE(master.dspPreset, QStringLiteral("reverb_21_outsideStreet"));
    QVERIFY(master.overrideDspPreset);
    QCOMPARE(master.reverbWet.value_or(0.0), 0.8);
    QVERIFY(master.useTimeVolumeMappingCurve);
    QCOMPARE(master.childEvents.size(), 2);
    QCOMPARE(master.childEvents[0], QStringLiteral("inferno.outside.part1"));
    QCOMPARE(master.childEvents[1], QStringLiteral("inferno.birds"));

    const auto& child = result.soundEvents[1];
    QCOMPARE(child.name, QStringLiteral("inferno.outside.part1"));
    QVERIFY(child.useWorldPosition);
    QVERIFY(child.position.has_value());
    QCOMPARE(child.position->x, 1437.0);
    QVERIFY(child.useDistanceVolumeMappingCurve);
    QCOMPARE(child.vsndFiles.size(), 1);
    QCOMPARE(child.vsndFiles.first(), QStringLiteral("sounds/ambient/inferno/exterior_01.vsnd"));

    QVERIFY(result.uniqueRawSoundAssets.contains(QStringLiteral("sound/ambient/inferno/exterior_01.wav")));
}

void TestSoundscapeDomain::testSoundEventKv3Writer() {
    SoundEvent ev;
    ev.name = QStringLiteral("test.event");
    ev.volume = 0.5;
    ev.pitch = 1.0;
    ev.vsndFiles.append(QStringLiteral("sounds/ambient/test.vsnd"));

    const QString kv3 = SoundEventKv3Writer::writeToString({ev});
    QVERIFY(kv3.startsWith(QStringLiteral("<!-- kv3")));
    QVERIFY(kv3.contains(QStringLiteral("test.event =")));
    QVERIFY(kv3.contains(QStringLiteral("volume = 0.5")));
    QVERIFY(kv3.contains(QStringLiteral("vsnd_files_track_01 = \"sounds/ambient/test.vsnd\"")));
}

void TestSoundscapeDomain::testParseOfficialSoundscapeFiles() {
    const QString officialDir = QStringLiteral(SOUNDSCRIPTS_TEST_DIR);
    QDir dir(officialDir);
    if (!dir.exists()) {
        QSKIP("source1soundscripts directory not available in test environment");
    }

    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.vsc"), QDir::Files);
    QVERIFY(!files.isEmpty());

    int totalParsed = 0;
    for (const auto& fileName : files) {
        if (fileName.toLower() == QStringLiteral("soundscapes_manifest.txt")) {
            continue;
        }

        const QString filePath = dir.absoluteFilePath(fileName);
        auto parseRes = SoundscapeParser::parseFile(Core::Path::FilesystemPath(filePath));
        QVERIFY2(parseRes.isSuccess(), qPrintable(QStringLiteral("Failed to parse %1: %2").arg(fileName, parseRes.message())));

        const auto& defs = parseRes.value();
        QVERIFY2(!defs.empty(), qPrintable(QStringLiteral("No soundscapes parsed from %1").arg(fileName)));

        auto convRes = SoundscapeToSoundEventConverter::convertBatch(defs);
        QVERIFY(!convRes.soundEvents.empty());

        totalParsed += static_cast<int>(defs.size());
    }

    QVERIFY(totalParsed > 100);
}

QTEST_MAIN(TestSoundscapeDomain)
#include "TestSoundscapeDomain.moc"


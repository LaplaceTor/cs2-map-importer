#include <QTest>
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesWriter.h"

using namespace Core::KeyValues;

class TestCoreKeyValues : public QObject {
    Q_OBJECT

private slots:
    void testBasicParseAndQuery() {
        const QString kvText = QStringLiteral(
            "\"Root\"\n"
            "{\n"
            "\t\"Key1\"\t\"Value1\"\n"
            "\t\"Key2\"\t\"Value2\"\n"
            "}\n");

        auto doc = KeyValuesDocument::fromString(kvText);
        auto* root = doc.root().findChild(QStringLiteral("Root"));
        QVERIFY(root != nullptr);
        QCOMPARE(root->property(QStringLiteral("Key1")), QStringLiteral("Value1"));
        QCOMPARE(root->property(QStringLiteral("Key2")), QStringLiteral("Value2"));
        QCOMPARE(root->indexOfProperty(QStringLiteral("Key1")), 0);
        QCOMPARE(root->indexOfProperty(QStringLiteral("Key2")), 1);
        QCOMPARE(root->indexOfProperty(QStringLiteral("NonExistent")), -1);
    }

    void testInsertPropertyPositional() {
        KeyValuesNode node = KeyValuesNode::makeSection(QStringLiteral("TestSection"));
        node.addProperty(QStringLiteral("PropA"), QStringLiteral("1"));
        node.addProperty(QStringLiteral("PropC"), QStringLiteral("3"));

        // Insert PropB at index 1
        node.insertProperty(1, QStringLiteral("PropB"), QStringLiteral("2"));

        QCOMPARE(node.childCount(), 3);
        QCOMPARE(node.children()[0].name(), QStringLiteral("PropA"));
        QCOMPARE(node.children()[1].name(), QStringLiteral("PropB"));
        QCOMPARE(node.children()[2].name(), QStringLiteral("PropC"));

        // Insert at beginning (index 0)
        node.insertProperty(0, QStringLiteral("PropStart"), QStringLiteral("0"));
        QCOMPARE(node.childCount(), 4);
        QCOMPARE(node.children()[0].name(), QStringLiteral("PropStart"));

        // Insert at end (index >= count)
        node.insertProperty(100, QStringLiteral("PropEnd"), QStringLiteral("4"));
        QCOMPARE(node.childCount(), 5);
        QCOMPARE(node.children()[4].name(), QStringLiteral("PropEnd"));
    }

    void testInsertPropertyAfterAndBefore() {
        KeyValuesNode node = KeyValuesNode::makeSection(QStringLiteral("Layer0"));
        node.addProperty(QStringLiteral("Shader"), QStringLiteral("csgo_complex.vfx"));
        node.addProperty(QStringLiteral("TextureColor"), QStringLiteral("materials/brick.tga"));

        // Insert F_FORCE_UV2 after Shader
        bool okAfter = node.insertPropertyAfter(QStringLiteral("Shader"), QStringLiteral("F_FORCE_UV2"), QStringLiteral("1"));
        QVERIFY(okAfter);
        QCOMPARE(node.childCount(), 3);
        QCOMPARE(node.children()[0].name(), QStringLiteral("Shader"));
        QCOMPARE(node.children()[1].name(), QStringLiteral("F_FORCE_UV2"));
        QCOMPARE(node.children()[1].value(), QStringLiteral("1"));
        QCOMPARE(node.children()[2].name(), QStringLiteral("TextureColor"));

        // Insert F_TEST before TextureColor
        bool okBefore = node.insertPropertyBefore(QStringLiteral("TextureColor"), QStringLiteral("F_TEST"), QStringLiteral("99"));
        QVERIFY(okBefore);
        QCOMPARE(node.childCount(), 4);
        QCOMPARE(node.children()[2].name(), QStringLiteral("F_TEST"));
        QCOMPARE(node.children()[3].name(), QStringLiteral("TextureColor"));

        // Failure case: target key does not exist
        bool okFail = node.insertPropertyAfter(QStringLiteral("MissingKey"), QStringLiteral("Foo"), QStringLiteral("Bar"));
        QVERIFY(!okFail);
    }

    void testInsertChildPositional() {
        KeyValuesNode node = KeyValuesNode::makeSection(QStringLiteral("Root"));
        node.addSection(QStringLiteral("Section1"));
        node.addSection(QStringLiteral("Section3"));

        QCOMPARE(node.indexOfChild(QStringLiteral("Section1")), 0);
        QCOMPARE(node.indexOfChild(QStringLiteral("Section3")), 1);
        QCOMPARE(node.indexOfChild(QStringLiteral("Missing")), -1);

        node.insertSection(1, QStringLiteral("Section2"));
        QCOMPARE(node.childCount(), 3);
        QCOMPARE(node.children()[1].name(), QStringLiteral("Section2"));

        bool okAfter = node.insertChildAfter(QStringLiteral("Section1"), KeyValuesNode::makeSection(QStringLiteral("Section1_5")));
        QVERIFY(okAfter);
        QCOMPARE(node.children()[1].name(), QStringLiteral("Section1_5"));

        bool okBefore = node.insertChildBefore(QStringLiteral("Section3"), KeyValuesNode::makeSection(QStringLiteral("Section2_5")));
        QVERIFY(okBefore);
        int idx = node.indexOfChild(QStringLiteral("Section2_5"));
        QCOMPARE(idx, 3);
    }

    void testSetPropertyAndChildAt() {
        KeyValuesNode node = KeyValuesNode::makeSection(QStringLiteral("Root"));
        node.addProperty(QStringLiteral("KeyA"), QStringLiteral("ValA"));
        node.addProperty(QStringLiteral("KeyB"), QStringLiteral("ValB"));

        // Replace index 0
        bool okSetProp = node.setPropertyAt(0, QStringLiteral("KeyA_New"), QStringLiteral("ValA_New"));
        QVERIFY(okSetProp);
        QCOMPARE(node.children()[0].name(), QStringLiteral("KeyA_New"));
        QCOMPARE(node.children()[0].value(), QStringLiteral("ValA_New"));

        // Replace with section at index 1
        bool okSetChild = node.setChildAt(1, KeyValuesNode::makeSection(QStringLiteral("SubSection")));
        QVERIFY(okSetChild);
        QVERIFY(node.children()[1].isSection());
        QCOMPARE(node.children()[1].name(), QStringLiteral("SubSection"));

        // Out of bounds check
        QVERIFY(!node.setPropertyAt(-1, QStringLiteral("X"), QStringLiteral("Y")));
        QVERIFY(!node.setPropertyAt(5, QStringLiteral("X"), QStringLiteral("Y")));
        QVERIFY(!node.setChildAt(99, KeyValuesNode::makeSection(QStringLiteral("Z"))));
    }

    void testVmatInjectionSimulation() {
        const QString vmatText = QStringLiteral(
            "\"Layer0\"\n"
            "{\n"
            "\t\"Shader\"\t\"csgo_complex.vfx\"\n"
            "\t\"TextureColor\"\t\"materials/de_jnt/flats/int_flats_steel_worn_01_color.tga\"\n"
            "\t\"TextureNormal\"\t\"materials/de_jnt/world/int_detail_n_02_normal.tga\"\n"
            "\t\"SystemAttributes\"\n"
            "\t{\n"
            "\t\t\"PhysicsSurfaceProperties\"\t\"paper\"\n"
            "\t}\n"
            "}\n");

        auto doc = KeyValuesDocument::fromString(vmatText);
        auto* layer0 = doc.root().findChild(QStringLiteral("Layer0"));
        QVERIFY(layer0 != nullptr);

        // Inject F_FORCE_UV2 = 1 directly after Shader
        if (layer0->hasProperty(QStringLiteral("F_FORCE_UV2"))) {
            layer0->setProperty(QStringLiteral("F_FORCE_UV2"), QStringLiteral("1"));
        } else {
            bool inserted = layer0->insertPropertyAfter(
                QStringLiteral("Shader"),
                QStringLiteral("F_FORCE_UV2"),
                QStringLiteral("1"));
            QVERIFY(inserted);
        }

        // Verify ordering in Layer0
        QCOMPARE(layer0->children()[0].name(), QStringLiteral("Shader"));
        QCOMPARE(layer0->children()[1].name(), QStringLiteral("F_FORCE_UV2"));
        QCOMPARE(layer0->children()[1].value(), QStringLiteral("1"));
        QCOMPARE(layer0->children()[2].name(), QStringLiteral("TextureColor"));

        // Verify serialized output contains F_FORCE_UV2 between Shader and TextureColor
        QString serialized = doc.saveToString();
        int shaderIdx = serialized.indexOf(QStringLiteral("\"Shader\""));
        int uv2Idx = serialized.indexOf(QStringLiteral("\"F_FORCE_UV2\""));
        int textureIdx = serialized.indexOf(QStringLiteral("\"TextureColor\""));

        QVERIFY(shaderIdx != -1);
        QVERIFY(uv2Idx != -1);
        QVERIFY(textureIdx != -1);
        QVERIFY(shaderIdx < uv2Idx);
        QVERIFY(uv2Idx < textureIdx);
    }

    void testRemoveAndClear() {
        KeyValuesNode node = KeyValuesNode::makeSection(QStringLiteral("Root"));
        node.addProperty(QStringLiteral("P1"), QStringLiteral("V1"));
        node.addProperty(QStringLiteral("P2"), QStringLiteral("V2"));
        node.addProperty(QStringLiteral("P1"), QStringLiteral("V3"));

        QCOMPARE(node.childCount(), 3);
        int removedProps = node.removeProperties(QStringLiteral("P1"));
        QCOMPARE(removedProps, 2);
        QCOMPARE(node.childCount(), 1);
        QCOMPARE(node.children()[0].name(), QStringLiteral("P2"));

        node.clear();
        QCOMPARE(node.childCount(), 0);
        QVERIFY(node.isEmpty());
    }
};

QTEST_MAIN(TestCoreKeyValues)
#include "TestCoreKeyValues.moc"

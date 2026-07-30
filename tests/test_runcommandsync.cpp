#include "Miscellaneous.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <iostream>
#include <cassert>

void runTest() {
    QString currentDir = QDir::currentPath();
    QString mockEnv = currentDir + "/mock_env";
    QDir().mkdir("mock_env");
    QDir().mkpath("mock_env/game/bin/win64");

    // Let's configure Miscellaneous options
    Miscellaneous::Options opts;
    opts.cs2Basefolder = mockEnv;
    Miscellaneous::SetOptions(opts);

    // Helper to write a mock file
    auto writeMockFile = [](const QString& path, const QString& content) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << content;
            file.close();
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                        QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser |
                                        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                        QFileDevice::ReadOther | QFileDevice::ExeOther);
        }
    };

    std::cout << "--- Test 1: isMap = true, isSource1Import = true, finding ParseEpar ---" << std::endl;
    // We want the mock source1import to output ParseEpar: token too long
    writeMockFile(mockEnv + "/game/bin/win64/source1import.exe",
                  "#!/bin/bash\n"
                  "echo 'Some random log'\n"
                  "echo 'ParseEpar: token too long'\n"
                  "echo 'More log'\n");

    bool threw = false;
    try {
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, QStringList(), false, nullptr, true, false);
    } catch (const AppException& e) {
        threw = true;
        std::cout << "Caught expected AppException: " << e.message().toStdString() << std::endl;
        assert(e.message() == "This map geometry is too bad to run the clean up faces process!");
    }
    assert(threw);
    std::cout << "Test 1 Passed!" << std::endl;

    // Reset CancelImport if it was set
    Miscellaneous::CanceLImport = 0;

    std::cout << "--- Test 2: isMap = false, isSource1Import = true, ParseEpar present ---" << std::endl;
    threw = false;
    try {
        int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, QStringList(), false, nullptr, false, false);
        std::cout << "Returned code: " << ret << std::endl;
    } catch (const AppException& e) {
        threw = true;
    }
    assert(!threw);
    std::cout << "Test 2 Passed!" << std::endl;

    std::cout << "--- Test 3: isCSGO = false, checks writing 'y/n' ---" << std::endl;
    // Set up mock source1import.exe to read from stdin with a timeout
    writeMockFile(mockEnv + "/game/bin/win64/source1import.exe",
                  "#!/bin/bash\n"
                  "read -t 12 line\n"
                  "if [ \"$line\" = \"y\" ]; then\n"
                  "    echo 'RECV_Y'\n"
                  "else\n"
                  "    echo 'NO_Y'\n"
                  "fi\n");

    QStringList logOut3;
    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, QStringList(), true, &logOut3, false, false);
    bool foundY = false;
    for (const QString& line : logOut3) {
        std::cout << "Test 3 output: " << line.toStdString() << std::endl;
        if (line.contains("RECV_Y")) foundY = true;
    }
    assert(foundY);
    std::cout << "Test 3 Passed!" << std::endl;

    std::cout << "--- Test 4: isCSGO = true, should NOT write 'y/n' ---" << std::endl;
    QStringList logOut4;
    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, QStringList(), true, &logOut4, false, true);
    bool foundY4 = false;
    for (const QString& line : logOut4) {
        std::cout << "Test 4 output: " << line.toStdString() << std::endl;
        if (line.contains("RECV_Y")) foundY4 = true;
    }
    assert(!foundY4);
    std::cout << "Test 4 Passed!" << std::endl;

    std::cout << "--- Test 5: Java version mismatch on PROGRAM_BSPSRC ---" << std::endl;
    // Create a mock java in mockEnv
    writeMockFile(mockEnv + "/java",
                  "#!/bin/bash\n"
                  "echo 'Error: this version of Java Runtime only recognizes class file versions up to 61.0'\n");

    // Modify PATH so that our mock java is found
    QString oldPath = qgetenv("PATH");
    qputenv("PATH", (mockEnv + ":" + oldPath).toLocal8Bit());

    threw = false;
    try {
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_BSPSRC, QStringList());
    } catch (const AppException& e) {
        threw = true;
        std::cout << "Caught expected AppException: " << e.message().toStdString() << std::endl;
        assert(e.message() == "Your Java version is too old. Please upgrade your Java version to a newer one.");
    }
    assert(threw);

    // Restore PATH
    qputenv("PATH", oldPath.toLocal8Bit());
    std::cout << "Test 5 Passed!" << std::endl;

    // Cleanup mock_env
    QDir(mockEnv).removeRecursively();
    std::cout << "All tests passed successfully!" << std::endl;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    runTest();
    return 0;
}

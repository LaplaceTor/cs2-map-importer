cmake_minimum_required(VERSION 3.10)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
    src/Main.cpp
    src/Ui.cpp
    src/Ui.h
    src/Miscellaneous.cpp
    src/Miscellaneous.h
    src/VmfBspProcess.cpp
    src/VmfBspProcess.h
    src/MapImporter.cpp
    src/MapImporter.h
    src/SoundscapeImport.cpp
    src/SoundscapeImport.h
    src/FileExtractFromVPK.cpp
    src/FileExtractFromVPK.h
    src/MaterialFix.cpp
    src/MaterialFix.h
    src/MaterialImporter.cpp
    src/MaterialImporter.h
)

qt_add_executable(cs2importer_app MANUAL_FINALIZATION ${SOURCES})

set_property(TARGET cs2importer_app PROPERTY WIN32_EXECUTABLE TRUE)

qt_add_qml_module(cs2importer_app
    URI cs2importer
    VERSION 1.0
    QML_FILES
        src/qml/main.qml
)

target_link_libraries(cs2importer_app PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network)

target_include_directories(cs2importer_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(cs2importer_app PRIVATE "APP_VERSION=\"${PROJECT_VERSION}\"")

qt_import_qml_plugins(cs2importer_app)
qt_finalize_executable(cs2importer_app)

cmake_minimum_required(VERSION 3.10)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
    src/main.cpp
    src/ui.cpp
    src/ui.h
    src/appcore.cpp
    src/appcore.h
    src/vmf_bsp_process.cpp
    src/vmf_bsp_process.h
    src/mapimporter.cpp
    src/mapimporter.h
)

qt_add_executable(cs2importer MANUAL_FINALIZATION ${SOURCES})

if(WIN32)
    set_property(TARGET cs2importer PROPERTY WIN32_EXECUTABLE TRUE)
endif()

qt_add_qml_module(cs2importer
    URI cs2importer
    VERSION 1.0
    QML_FILES
        src/qml/main.qml
)

target_link_libraries(cs2importer PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_import_qml_plugins(cs2importer)
qt_finalize_executable(cs2importer)

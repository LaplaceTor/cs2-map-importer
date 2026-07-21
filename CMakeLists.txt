cmake_minimum_required(VERSION 3.10)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add MSVC-specific compiler options
if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
endif()

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
    src/ModelImporter.cpp
    src/ModelImporter.h
    src/SoundscapeImport.cpp
    src/SoundscapeImport.h
    src/FileExtractFromVPK.cpp
    src/FileExtractFromVPK.h
    src/MaterialFix.cpp
    src/MaterialFix.h
)

qt_add_executable(cs2importer MANUAL_FINALIZATION ${SOURCES})

set_property(TARGET cs2importer PROPERTY WIN32_EXECUTABLE TRUE)

# Use for agent
if(NOT WIN32)
    set_target_properties(cs2importer PROPERTIES OUTPUT_NAME "cs2importer_app")
endif()

qt_add_qml_module(cs2importer
    URI cs2importer
    VERSION 1.0
    QML_FILES
        src/qml/main.qml
)

target_link_libraries(cs2importer PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network)

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")
target_compile_definitions(cs2importer PRIVATE "APP_VERSION=\"${APP_VERSION}\"")

qt_import_qml_plugins(cs2importer)
qt_finalize_executable(cs2importer)

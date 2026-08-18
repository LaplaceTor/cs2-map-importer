add_subdirectory(Core)

set(APP_SOURCES
    Main.cpp
    Ui.cpp
    Ui.h
    Miscellaneous.cpp
    Miscellaneous.h
    VmfBspProcess.cpp
    VmfBspProcess.h
    MapImporter.cpp
    MapImporter.h
    ModelImporter.cpp
    ModelImporter.h
    ParticleImporter.cpp
    ParticleImporter.h
    SoundscapeImport.cpp
    SoundscapeImport.h
    FileExtractFromVPK.cpp
    FileExtractFromVPK.h
    MaterialFix.cpp
    MaterialFix.h
)

if(WIN32)
    list(APPEND APP_SOURCES ${CMAKE_SOURCE_DIR}/icons/cs2importer.rc)
endif()

qt_add_executable(cs2importer MANUAL_FINALIZATION ${APP_SOURCES})

set_property(TARGET cs2importer PROPERTY WIN32_EXECUTABLE TRUE)

if(NOT WIN32)
    set_target_properties(cs2importer PROPERTIES OUTPUT_NAME "cs2importer_app")
endif()

set_source_files_properties(qml/main.qml PROPERTIES QT_RESOURCE_ALIAS "src/qml/main.qml")

qt_add_qml_module(cs2importer
    URI cs2importer
    RESOURCE_PREFIX "/"
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

qt_add_resources(cs2importer "cs2importer_icons"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        ${CMAKE_SOURCE_DIR}/icons/icon.png
)

target_link_libraries(cs2importer PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

target_include_directories(cs2importer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")
target_compile_definitions(cs2importer PRIVATE "APP_VERSION=\"${APP_VERSION}\"")

qt_import_qml_plugins(cs2importer)
qt_finalize_executable(cs2importer)

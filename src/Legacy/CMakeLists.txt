set(LEGACY_SOURCES
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
    ${CMAKE_SOURCE_DIR}/icons/cs2importer.rc
)

qt_add_executable(cs2importerlegacy MANUAL_FINALIZATION ${LEGACY_SOURCES})

set_property(TARGET cs2importerlegacy PROPERTY WIN32_EXECUTABLE TRUE)

qt_add_resources(cs2importerlegacy "cs2importer_icons"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        ${CMAKE_SOURCE_DIR}/icons/icon.png
)

target_link_libraries(cs2importerlegacy PRIVATE
    cs2importer_core
    cs2importer_qml
    cs2importer_qmlplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

target_include_directories(cs2importerlegacy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")
target_compile_definitions(cs2importerlegacy PRIVATE "APP_VERSION=\"${APP_VERSION}\"")

qt_import_qml_plugins(cs2importerlegacy)
qt_finalize_executable(cs2importerlegacy)

install(TARGETS cs2importerlegacy
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(CS2IMPORTER_DEPLOY_QT_RUNTIME)
    qt_generate_deploy_qml_app_script(
        TARGET cs2importerlegacy
        OUTPUT_SCRIPT deploy_script_legacy
        NO_TRANSLATIONS
    )

    install(SCRIPT ${deploy_script_legacy})
endif()

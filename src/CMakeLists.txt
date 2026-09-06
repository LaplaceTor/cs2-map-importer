add_subdirectory(Core)
add_subdirectory(Domain)
add_subdirectory(Workflow)
add_subdirectory(Application)
add_subdirectory(UI)
add_subdirectory(qml/cs2importer)

set(APP_SOURCES
    Main.cpp
    ${CMAKE_SOURCE_DIR}/icons/cs2importer.rc
)

qt_add_executable(cs2importer MANUAL_FINALIZATION ${APP_SOURCES})

set_property(TARGET cs2importer PROPERTY WIN32_EXECUTABLE TRUE)

qt_add_resources(cs2importer "cs2importer_icons"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        ${CMAKE_SOURCE_DIR}/icons/icon.png
)

target_link_libraries(cs2importer PRIVATE
    cs2importer_ui
    cs2importer_qml
    cs2importer_qmlplugin
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

install(TARGETS cs2importer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(CS2IMPORTER_DEPLOY_QT_RUNTIME)
    qt_generate_deploy_qml_app_script(
        TARGET cs2importer
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
    )

    install(SCRIPT ${deploy_script})
endif()

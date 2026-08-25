qt_add_library(cs2importer_ui STATIC
    ViewModels/GameViewModel.h
    ViewModels/GameViewModel.cpp
    ViewModels/LogViewModel.h
    ViewModels/LogViewModel.cpp
    Controllers/MainController.h
    Controllers/MainController.cpp
)

target_link_libraries(cs2importer_ui PUBLIC
    cs2importer_application
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(cs2importer_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")
target_compile_definitions(cs2importer_ui PRIVATE "APP_VERSION=\"${APP_VERSION}\"")

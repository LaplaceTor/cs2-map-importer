qt_add_library(cs2importer_application STATIC
    Environment/GameInstallation.h
    Environment/SteamService.h
    Environment/SteamService.cpp
    Environment/GameDetectService.h
    Environment/GameDetectService.cpp
)

target_link_libraries(cs2importer_application PUBLIC
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Gui
)

target_include_directories(cs2importer_application PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


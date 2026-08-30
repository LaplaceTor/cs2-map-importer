qt_add_library(cs2importer_application STATIC
    Async/AsyncTaskRunner.h
    Execution/ExecutionGuard.h
    Environment/GameInstallationInfo.h
    Environment/GameInstallation.h
    Environment/GameInstallationValidator.h
    Environment/GameInstallationValidator.cpp
    Environment/Internal/SteamLibraryDetector.h
    Environment/Internal/SteamLibraryDetector.cpp
    Environment/SteamService.h
    Environment/SteamService.cpp
    Environment/GameDetectService.h
    Environment/GameDetectService.cpp
    Environment/VpkSignatureLeaseService.h
    Environment/VpkSignatureLeaseService.cpp
    Environment/GameEnvironmentService.h
    Environment/GameEnvironmentService.cpp
    Soundscape/SoundscapeConvertDTOs.h
    Soundscape/SoundscapeConvertService.h
    Soundscape/SoundscapeConvertService.cpp
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

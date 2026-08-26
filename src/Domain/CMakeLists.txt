qt_add_library(cs2importer_domain STATIC
    Asset/AssetType.h
    Asset/AssetPath.h
    Asset/AssetPath.cpp
    Asset/AssetTypeDetector.h
    Asset/AssetTypeDetector.cpp
    Game/GameType.h
    Game/GameError.h
    Game/GameDefinition.h
    Game/GameRegistry.h
    Game/GameRegistry.cpp
    Game/GameValidator.h
    Game/GameValidator.cpp
    Game/SearchTarget.h
    Game/GameInfo.h
    Game/SearchPathResolver.h
    Game/SearchPathResolver.cpp
    Game/GameInfoParser.h
    Game/GameInfoParser.cpp
    Game/GameInstallationResolver.h
    Game/GameInstallationResolver.cpp
    Game/SteamGameLocator.h
    Game/SteamGameLocator.cpp
)

target_link_libraries(cs2importer_domain PUBLIC
    cs2importer_core
    Qt6::Core
)

target_include_directories(cs2importer_domain PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


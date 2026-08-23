qt_add_library(cs2importer_domain STATIC
    Game/SearchTarget.h
    Game/GameInfo.h
    Game/SearchPathResolver.h
    Game/SearchPathResolver.cpp
    Game/GameInfoParser.h
    Game/GameInfoParser.cpp
)

target_link_libraries(cs2importer_domain PUBLIC
    cs2importer_core
    Qt6::Core
)

target_include_directories(cs2importer_domain PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


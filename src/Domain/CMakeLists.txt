# Third-party: sourcepp (vpkpp / bsppp / vtfpp) via git submodule under third_party/.
# Note: sourcepp pulls pinned dependencies (bufferstream, libtomcrypt, libtommath,
# hat-trie, miniz, minizip-ng) at configure time via FetchContent; first configure needs network.
if(NOT TARGET sourcepp::vpkpp)
    set(sourcepp_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sourcepp")
    if(NOT EXISTS "${sourcepp_dir}/CMakeLists.txt")
        find_package(Git QUIET)
        if(Git_FOUND)
            message(STATUS "third_party/sourcepp submodule is missing, initializing...")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive third_party/sourcepp
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
                RESULT_VARIABLE sourcepp_submodule_result
            )
        endif()
        if(NOT EXISTS "${sourcepp_dir}/CMakeLists.txt")
            message(FATAL_ERROR
                "third_party/sourcepp does not contain CMakeLists.txt. "
                "Please run 'git submodule update --init --recursive' in the repository root."
            )
        endif()
    endif()

    set(SOURCEPP_LIBS_START_ENABLED OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_USE_VPKPP ON CACHE BOOL "" FORCE)
    set(SOURCEPP_USE_BSPPP ON CACHE BOOL "" FORCE)
    set(SOURCEPP_USE_VTFPP ON CACHE BOOL "" FORCE)
    set(SOURCEPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_VTFPP_SUPPORT_EXR OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_VTFPP_SUPPORT_QOI OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_VTFPP_SUPPORT_WEBP OFF CACHE BOOL "" FORCE)
    set(SOURCEPP_VTFPP_BUILD_WITH_COMPRESSONATOR OFF CACHE BOOL "" FORCE)
    add_subdirectory(${sourcepp_dir} sourcepp-build)
endif()


qt_add_library(cs2importer_domain STATIC
    Asset/AssetType.h
    Asset/AssetPath.h
    Asset/AssetPath.cpp
    Asset/AssetTypeDetector.h
    Asset/AssetTypeDetector.cpp
    Game/GameType.h
    Game/GameErrors.h
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
    Audio/Range.h
    Audio/DspPresetRegistry.h
    Audio/DspPresetRegistry.cpp
    Audio/SoundLevelMapper.h
    Audio/SoundLevelMapper.cpp
    Audio/SoundscapeDefinition.h
    Audio/SoundEvent.h
    Audio/SoundscapeParser.h
    Audio/SoundscapeParser.cpp
    Audio/SoundscapeToSoundEventConverter.h
    Audio/SoundscapeToSoundEventConverter.cpp
    Audio/SoundEventKv3Writer.h
    Audio/SoundEventKv3Writer.cpp
    Material/VtfConverter.h
    Material/VtfConverter.cpp
    Package/BspPackExtractor.h
    Package/BspPackExtractor.cpp
    Package/PackArchive.h
    Package/PackArchive.cpp
    Package/PackArchivePool.h
    Package/PackArchivePool.cpp
    Tool/Cs2PathLayout.h
    Tool/ToolErrors.h
    Tool/Source1ImportLogParser.h
    Tool/Source1ImportLogParser.cpp
    Tool/ResourceCompilerLogParser.h
    Tool/ResourceCompilerLogParser.cpp
    Tool/Source1ImportTool.h
    Tool/Source1ImportTool.cpp
    Tool/ResourceCompilerTool.h
    Tool/ResourceCompilerTool.cpp
)

target_link_libraries(cs2importer_domain PUBLIC
    cs2importer_core
    Qt6::Core
)

# sourcepp stays an implementation detail: no sourcepp types in Domain public headers.
target_link_libraries(cs2importer_domain PRIVATE
    sourcepp::vpkpp
    sourcepp::bsppp
    sourcepp::vtfpp
    MINIZIP::minizip
)

target_include_directories(cs2importer_domain PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


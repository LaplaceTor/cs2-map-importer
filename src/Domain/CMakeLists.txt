# Third-party: sourcepp (vpkpp / bsppp / vtfpp) via git submodule under third_party/.
# Note: sourcepp pulls a few pinned-commit dependencies (bufferstream, cryptopp, hat-trie,
# miniz, minizip-ng) at configure time via FetchContent; the first configure needs network.
if(NOT TARGET sourcepp::vpkpp)
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
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sourcepp sourcepp-build)
endif()

# C++20 and modern MSVC (>= 19.38 / VS 2022 17.8+ / VS 2026) removed non-standard
# stdext::make_checked_array_iterator / make_unchecked_array_iterator.
# Refactor the fetched cryptopp sources in place if pinned legacy sources are present,
# adapting them directly to C++20 and modern MSVC standards without /FI shims.
function(cs2importer_modernize_cryptopp_sources)
    set(candidate_dirs
        "${CMAKE_BINARY_DIR}/_deps/cryptopp-cmake-build/cryptopp"
        "${CMAKE_BINARY_DIR}/sourcepp-build/ext/cryptopp"
        "${cryptopp_SOURCE_DIR}"
    )
    foreach(candidate ${candidate_dirs})
        if(EXISTS "${candidate}/integer.cpp")
            file(READ "${candidate}/integer.cpp" integer_content)
            if(integer_content MATCHES "CRYPTOPP_MSC_VERSION >= 1500" AND NOT integer_content MATCHES "CRYPTOPP_MSC_VERSION < 1938")
                string(REPLACE
                    "CRYPTOPP_MSC_VERSION >= 1500)"
                    "(CRYPTOPP_MSC_VERSION >= 1500) && (CRYPTOPP_MSC_VERSION < 1938)"
                    integer_content "${integer_content}"
                )
                file(WRITE "${candidate}/integer.cpp" "${integer_content}")
            endif()
        endif()

        if(EXISTS "${candidate}/zdeflate.cpp")
            file(READ "${candidate}/zdeflate.cpp" zdeflate_content)
            if(zdeflate_content MATCHES "CRYPTOPP_MSC_VERSION >= 1600" AND NOT zdeflate_content MATCHES "CRYPTOPP_MSC_VERSION < 1938")
                string(REPLACE
                    "CRYPTOPP_MSC_VERSION >= 1600"
                    "(CRYPTOPP_MSC_VERSION >= 1600) && (CRYPTOPP_MSC_VERSION < 1938)"
                    zdeflate_content "${zdeflate_content}"
                )
                file(WRITE "${candidate}/zdeflate.cpp" "${zdeflate_content}")
            endif()
        endif()
    endforeach()
endfunction()
cs2importer_modernize_cryptopp_sources()

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
    Package/PackArchive.h
    Package/PackArchive.cpp
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
)

target_include_directories(cs2importer_domain PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


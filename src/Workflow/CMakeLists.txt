qt_add_library(cs2importer_workflow STATIC
    Common/CancellationToken.h
    Common/AssetExtractor.h
    Common/AssetExtractor.cpp
    Common/BspEmbeddedExtractor.h
    Common/BspEmbeddedExtractor.cpp
    Common/VtfExtractor.h
    Common/VtfExtractor.cpp
)

target_link_libraries(cs2importer_workflow PUBLIC
    cs2importer_domain
    cs2importer_core
    Qt6::Core
)

target_include_directories(cs2importer_workflow PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

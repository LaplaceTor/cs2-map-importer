qt_add_library(cs2importer_core STATIC)

target_sources(cs2importer_core
PRIVATE
Asset/AssetType.h
Asset/AssetTypeDetector.h
Asset/AssetTypeDetector.cpp

```
    Error/ImportError.h
    Error/ImportException.h

    FileSystem/FileSystem.h
    FileSystem/FileSystem.cpp
    FileSystem/AtomicFile.h
    FileSystem/AtomicFile.cpp
    FileSystem/DirectorySnapshot.h
    FileSystem/DirectorySnapshot.cpp

    Logging/Logger.h
    Logging/Logger.cpp

    Path/AssetPath.h
    Path/AssetPath.cpp
    Path/FilesystemPath.h
    Path/FilesystemPath.cpp
    Path/PathUtils.h

    Process/ProcessOptions.h
    Process/ProcessResult.h
    Process/ProcessRunner.h
    Process/ProcessRunner.cpp

    Temp/TempDirectory.h
    Temp/TempFile.h
```

)

target_include_directories(cs2importer_core
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(cs2importer_core
PUBLIC
Qt6::Core
)

target_compile_features(cs2importer_core
PUBLIC
cxx_std_17
)

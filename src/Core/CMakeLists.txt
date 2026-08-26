qt_add_library(cs2importer_core STATIC
    Async/TaskResult.h
    Error/ImportError.h
    Error/ImportException.h
    FileSystem/AtomicFile.h
    FileSystem/AtomicFile.cpp
    FileSystem/DirectorySnapshot.h
    FileSystem/DirectorySnapshot.cpp
    FileSystem/FileLease.h
    FileSystem/FileLease.cpp
    FileSystem/FileSystem.h
    FileSystem/FileSystem.cpp
    KeyValues/KeyValuesNode.h
    KeyValues/KeyValuesNode.cpp
    KeyValues/KeyValuesLexer.h
    KeyValues/KeyValuesLexer.cpp
    KeyValues/KeyValuesParser.h
    KeyValues/KeyValuesParser.cpp
    KeyValues/KeyValuesWriter.h
    KeyValues/KeyValuesWriter.cpp
    KeyValues/KeyValuesDocument.h
    KeyValues/KeyValuesDocument.cpp
    Logging/LogLevel.h
    Logging/LogEntry.h
    Logging/LogBlock.h
    Logging/LogBlock.cpp
    Logging/Logger.h
    Logging/Logger.cpp
    Logging/TaskState.h
    Logging/TaskSnapshot.h
    Logging/FaultBarrier.h
    Logging/FaultBarrier.cpp
    Logging/ILogSink.h
    Logging/FileSink.h
    Logging/FileSink.cpp
    Logging/TaskLoggingContext.h
    Logging/TaskLoggingContext.cpp
    Logging/LogManager.h
    Logging/LogManager.cpp
    Path/FilesystemPath.h
    Path/FilesystemPath.cpp
    Path/PathUtils.h
    Process/ProcessOptions.h
    Process/ProcessResult.h
    Process/ProcessRunner.h
    Process/ProcessRunner.cpp
    Temp/TempDirectory.h
    Temp/TempFile.h
)

target_link_libraries(cs2importer_core PRIVATE Qt6::Core)

target_include_directories(cs2importer_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)


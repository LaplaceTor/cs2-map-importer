# CS2 Map Importer — Agent Instructions

## Project Overview

A Windows desktop GUI application that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.

* **Language:** C++17
* **Framework:** Qt 6.8+
* **Build system:** Modern CMake
* **Platform:** Windows (the project also has non-Windows build branches)
* **UI:** QML / Qt Quick Controls 2
* **QML style:** Fusion

The project is undergoing a staged architecture refactor. The reusable Core layer has been extracted, but application/importer migration is still staged and must not be inferred from the existence of Core.

## Current Architecture

```text
src/
├── Core/                         # Reusable infrastructure, independent of application workflows
│   ├── Asset/                    # AssetType and AssetTypeDetector
│   ├── Error/                    # ImportErrorCode and ImportException
│   ├── FileSystem/               # FileSystem, AtomicFile, DirectorySnapshot
│   ├── Logging/                  # Task-oriented logging and sinks
│   ├── Path/                     # AssetPath, FilesystemPath, PathUtils
│   ├── Process/                  # ProcessOptions, ProcessResult, ProcessRunner
│   ├── Temp/                     # TempFile and TempDirectory
│   └── CMakeLists.txt
│
├── Main.cpp                      # Existing application entry point
├── Ui.h / Ui.cpp                 # Existing QML-facing controller
├── ModelImporter.h/.cpp          # Existing importer; not yet migrated to Core
├── ParticleImporter.h/.cpp       # Existing importer; not yet migrated to Core
├── MapImporter.h/.cpp            # Existing legacy implementation
├── Miscellaneous.h/.cpp          # Existing legacy utilities
├── VmfBspProcess.h/.cpp          # Existing legacy map workflow
├── FileExtractFromVPK.h/.cpp     # Existing legacy map workflow
├── MaterialFix.h/.cpp            # Existing legacy map workflow
├── SoundscapeImport.h/.cpp       # Existing legacy map workflow
└── qml/cs2importer/Main.qml
```

`src/Core/CMakeLists.txt` currently builds `cs2importer_core` as a static library. Its public include root is `src/`, so Core headers use project-root includes such as:

```cpp
#include "Core/Path/AssetPath.h"
#include "Core/FileSystem/FileSystem.h"
```

Do not add Core source files directly to the application target. The application and tests link against `cs2importer_core`.

## Refactor Status and Scope

### Completed

* Core infrastructure is extracted under `src/Core`.
* Core is an independent `cs2importer_core` target linked by the application and tests.
* `AssetPath` and `FilesystemPath` are separate value types.
* Core currently contains the APIs documented in the **Core API Reference** section below.

### Not yet completed

* **ModelImporter migration:** not started; it still uses application/legacy infrastructure.
* **ParticleImporter migration:** not started; it still uses application/legacy infrastructure.
* **MapImporter migration:** intentionally deferred; preserve its current implementation and behavior.
* **UI/QML and remaining application infrastructure:** still current-state/legacy code.

At the current stage, do not migrate application code merely because an equivalent Core API exists. Only use Core APIs in application code when the task explicitly migrates that component. Do not combine refactor stages unless explicitly requested.

The Core extraction has not intentionally rewritten import workflows or runtime behavior. Preserve existing behavior during structural work and do not claim that an importer has been migrated unless the task actually performs that migration.

## Core Dependency Rules

Dependency direction must remain:

```text
Application / Importers
        ↓
      Core
```

Core must not depend on `MapImporter`, `ModelImporter`, `ParticleImporter`, `Ui`, QML, or legacy application utilities. Do not place workflow policy, UI behavior, or importer-specific logic in Core.

When migrating a component to Core, use an existing equivalent Core facility rather than duplicating it:

* filesystem operations → `Core::FileSystem`
* asset classification → `Core::Asset`
* asset-relative paths → `Core::Path::AssetPath`
* filesystem paths → `Core::Path::FilesystemPath`
* external processes → `Core::Process`
* temporary resources → `Core::Temp`
* task logging → `Core::Logging`
* import failures → `Core::Error`

## Core API Reference

This section reflects the current public declarations and should be updated if the API changes.

### Paths: `Core::Path`

`AssetPath` represents a validated asset-relative path. `FilesystemPath` represents a path in the host filesystem. They are intentionally not interchangeable.

```cpp
#include "Core/Path/AssetPath.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"

Core::Path::AssetPath assetPath(QStringLiteral("models/props/example.mdl"));
Core::Path::FilesystemPath filePath(QStringLiteral("C:/game/assets/models/props/example.mdl"));

if (assetPath.isValid()) {
    const QString extension = assetPath.extension();
}
if (filePath.exists() && filePath.isFile()) {
    const auto parent = filePath.parentPath();
}
```

`AssetPath` normalizes backslashes to `/`, rejects absolute paths, drive letters, schemes, empty components, `.` and `..`, and stores only a valid relative path. It provides `isEmpty`, `isValid`, `fileName`, `extension`, `directory`, `toString`, and equality operators. It does **not** provide the old `type()` API.

`FilesystemPath` normalizes with `QDir::cleanPath` and provides `isEmpty`, `isValid`, `exists`, `isFile`, `isDirectory`, `fileName`, `extension`, `parentPath`, `absolutePath`, `canonicalPath`, `toString`, and equality operators. `canonicalPath()` returns an invalid empty path when canonicalization fails.

`PathUtils` provides static helpers for raw `QString` paths: `normalize`, `filename`, `extension`, `directory`, and `relativePath`. It also provides:

* `resolveAssetPath(FilesystemPath baseDir, AssetPath assetPath)` → `FilesystemPath`; returns an invalid path if either input is invalid.
* `makeAssetPath(FilesystemPath baseDir, FilesystemPath filePath)` → `std::optional<AssetPath>`; returns no value when the file is outside the base directory or either path is invalid.

Do not merge the two path types, reintroduce `AssetPath::type()`, or add compatibility methods solely for legacy code.

### Asset detection: `Core::Asset`

```cpp
#include "Core/Asset/AssetTypeDetector.h"

const Core::Asset::AssetType type =
    Core::Asset::AssetTypeDetector::detect(assetPath);
```

`AssetType` values are `Unknown`, `Model`, `Particle`, `Material`, and `Map`. `AssetTypeDetector::detect` accepts `AssetPath`, `FilesystemPath`, or `QString`; `detectFromExtension` accepts an extension. Detection is case-insensitive and currently recognizes:

* Model: `mdl`, `vmdl`, `smd`, `fbx`
* Particle: `pcf`, `vpcf`
* Material: `vmt`, `vmat`, `vtf`
* Map: `vmf`, `bsp`, `vmap`

Unknown extensions return `AssetType::Unknown`.

### Errors: `Core::Error`

`ImportErrorCode` values are `Unknown`, `FileNotFound`, `InvalidPath`, `PermissionDenied`, `InvalidFile`, `DirectoryNotFound`, `ProcessFailed`, `ProcessTimeout`, and `OperationFailed`.

`Core::Error::ImportException` derives from `QException` and stores an error code and message:

```cpp
try {
    Core::FileSystem::FileSystem::readAll(path);
} catch (const Core::Error::ImportException& exception) {
    const auto code = exception.errorCode();
    const QString message = exception.message();
}
```

Use `ImportException` and `ImportErrorCode` in migrated Core-based code. Do not introduce new uses of the legacy `AppException` in migrated code. Preserve existing exception behavior in code that has not been migrated.

### Filesystem: `Core::FileSystem`

`FileSystem` provides static `exists`, `isFile`, `isDirectory`, `createDirectory`, `remove`, `copy`, `move`, `readAll`, and `writeAll` helpers. `copy` and `move` default to overwrite and throw `ImportException` on failure. `copy` supports files and recursive directory merge-copy.

`AtomicFile` is a move-only RAII wrapper around `QSaveFile`. Construct it with a target path, then call `open`, `write`, and `commit`; call `rollback` when abandoning the operation. `writeAtomic(target, data)` is the one-shot helper. It throws `ImportException` for failures.

`DirectorySnapshot` captures a directory into relative `FileEntry` records (`path`, `exists`, `size`, `lastModified`). Use `capture`, `diff`, `added`, `removed`, `modified`, `contains`, and `fileEntry`. Snapshot comparisons require the same root and may throw on an invalid comparison.

### Processes: `Core::Process`

`ProcessOptions` contains `timeout` in milliseconds (default `30000`, `-1` means no timeout), `workingDirectory`, `environment`, and `arguments`. `ProcessRunner::run` and `execute` return a `ProcessResult` rather than throwing for normal process outcomes.

`ProcessResult` contains `status`, `exitCode`, `stdOut`, `stdErr`, and `errorMessage`. `ProcessStatus` is `Success`, `FailedToStart`, `Crashed`, `TimedOut`, or `NonZeroExit`; use `isSuccess()` instead of checking only the exit code. Prefer the overload that passes arguments explicitly:

```cpp
Core::Process::ProcessOptions options;
options.timeout = 60000;
options.workingDirectory = workingDirectory;
const auto result = Core::Process::ProcessRunner::run(executable, arguments, options);
if (!result.isSuccess()) {
    // inspect result.status, result.errorMessage, and result.stdErr
}
```

### Temporary resources: `Core::Temp`

`TempFile` and `TempDirectory` are move-only RAII wrappers over Qt temporary resources. They create the resource during construction, expose `path()`, `exists()`, and `isValid()`, and clean up when destroyed. Construction throws `Core::Error::ImportException` with `OperationFailed` if creation fails. Do not manually delete resources owned by these wrappers.

### Logging: `Core::Logging`

`Logger::debug/info/warning/error` are simple static logging helpers. For importer/task workflows use `LogManager::instance().createTask(...)`, which returns `std::shared_ptr<TaskLoggingContext>`:

```cpp
auto task = Core::Logging::LogManager::instance().createTask(QStringLiteral("Import model"));
task->start();
task->info(QStringLiteral("Started"));
task->updateProgress(0.5, QStringLiteral("Converting"));
task->complete(QStringLiteral("Finished"));
```

`TaskLoggingContext` supports `debug`, `info`, `warning`, `error`, `log`, `reportFault`, progress/current-message updates, lifecycle transitions (`start`, `complete`, `fail`, `cancel`), snapshots, and block flushing. Valid lifecycle transitions are `Pending -> Running -> Completed|Failed|Cancelled`; terminal states cannot transition again.

`LogManager` also supports task lookup/finalization, sinks (`addSink`, `removeSink`, `clearSinks`), `flushTask`, `flushAll`, task snapshots, and sealed/all-block inspection. Reader callbacks such as `readSealedBlocks`, `readAllBlocks`, and `readLogBlock` execute while locks are held: keep them short, in-memory, and free of I/O or calls that may re-enter logging.

Use `FileSink` for file output or implement `ILogSink::writeBlock` and `flush` for another destination. Logs are block-based; only sealed blocks are delivered to sinks. `FaultBarrier` coordinates accepted submissions and fault/draining/termination states. Do not assume ordinary logging remains accepted after a fault or session termination; inspect `LogSubmissionResult` when ordering matters.

### Current Core build boundary

The current Core target includes the headers and implementations listed in `src/Core/CMakeLists.txt`. In particular, `TempFile` and `TempDirectory` are header-only. Core links privately to `Qt6::Core`; consumers should link the Core target rather than manually adding Core sources.

## C++ and Existing-Code Conventions

* Use C++17, Qt types, RAII, deterministic ownership, `const` correctness, and lightweight headers.
* Use `PascalCase` for classes/enums and `camelCase` for functions, methods, locals, and members.
* Do not perform unrelated style cleanup in legacy code.
* Preserve legacy behavior unless a task explicitly requests a behavior change.
* Do not duplicate Core facilities in newly migrated code.

## CMake Rules

* Require CMake 3.28+ and Qt 6.8+.
* Use `qt_standard_project_setup()` where appropriate.
* Use `qt_add_executable()` for the application, `qt_add_library()` for Core, and `qt_add_qml_module()` for QML.
* Use `qt_add_resources()` only for non-QML resources.
* Prefer target-based configuration with explicit `PRIVATE`, `PUBLIC`, or `INTERFACE` visibility.
* Do not use global include paths when target-specific configuration is sufficient.
* Do not manually list generated MOC/RCC/QML compiler outputs.
* Do not use Qt 5 CMake APIs, `Qt5::` targets, qmake syntax, or `add_executable()` for the application.
* Do not put QML files in `qt_add_resources()`.

When modifying CMake, consult `skills/qt-cmake-project/SKILL.md` and its references, especially `simple-project.md`, `modular-architecture.md`, `qml-integration.md`, `resources.md`, and `common-mistakes.md`.

## Build and Tests

Standard local build:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The repository currently defines `test_logmanager` and `logging_test` under `tests/`. If `CMakePresets.json` is introduced, prefer presets for normal development and CI.

## Skills — Auto-Load Rules

Before making changes, read the relevant skill:

| Task | Skill |
|---|---|
| C++ implementation | `skills/qt-cmake-project/SKILL.md` |
| CMake/build changes | `skills/qt-cmake-project/SKILL.md` |
| QML implementation | `skills/qt-qml/SKILL.md` |
| C++ review | `skills/qt-cpp-review/SKILL.md` |
| QML review | `skills/qt-qml-review/SKILL.md` |
| UI/UX decisions | `skills/qt-ui-design/SKILL.md` |

## Do NOT

* Do not make Core depend on application code, UI, or QML.
* Do not migrate ModelImporter or ParticleImporter unless explicitly requested.
* Do not migrate MapImporter as part of another importer's migration.
* Do not perform unrelated refactoring during a focused migration.
* Do not assume current runtime/import behavior has already been rewritten around Core.
* Do not add compatibility APIs solely for legacy code.

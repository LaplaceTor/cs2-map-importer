# CS2 Map Importer — Agent Instructions

## Project Overview

A Windows desktop GUI application that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.

* **Language:** C++17
* **Framework:** Qt 6.8+
* **Build system:** Modern CMake
* **Platform:** Windows (the project also has non-Windows build branches)
* **UI:** QML / Qt Quick Controls 2
* **QML style:** Fusion

The project is undergoing a staged architecture refactor. The reusable Core layer has been extracted, and unmigrated components are being transitioned into a clean 4-tier layered architecture.

---

## Target Layered Architecture

```text
┌───────────────────────────────────────────────────────────┐
│                    Presentation Layer (UI)                │
│   QML Views ◄──(Q_PROPERTY / Signals)──► ViewModels/Ui    │
└─────────────────────────────┬─────────────────────────────┘
                              │ calls / connects
┌─────────────────────────────▼─────────────────────────────┐
│                 Application Service Layer                 │
│  • WorkflowRunner / Task Management • ConfigService       │
│  • GameDetectService (Steam/Paths)  • UpdateService       │
│  • IUserPromptHandler / Modal Confirmation Bridge         │
└─────────────────────────────┬─────────────────────────────┘
                              │ executes pipelines / invokes
┌─────────────────────────────▼─────────────────────────────┐
│                  Domain / Workflow Layer                  │
│  • Importers: MapImporter, ModelImporter, ParticleImporter│
│  • Domain Tools: BspsrcTool, Source1ImportTool, RC...     │
│  • Asset Processors: VmfProcessor, MaterialFixer, Audio   │
│  • Package Extractor (VPK) & GameInfo/SearchPaths         │
└─────────────────────────────┬─────────────────────────────┘
                              │ uses base utilities
┌─────────────────────────────▼─────────────────────────────┐
│                    Core Infrastructure                    │
│  Core::Path, Core::FileSystem, Core::KeyValues,           │
│  Core::Process, Core::Logging, Core::Temp,                │
│  Core::Error                                              │
└───────────────────────────────────────────────────────────┘
```

### Dependency Rules

1. **Strict Downward Dependency**:
   $$\text{Presentation} \longrightarrow \text{Application} \longrightarrow \text{Domain/Workflow} \longrightarrow \text{Core}$$
2. **Zero Upward Perception**: Lower layers (`Core`, `Domain`) must **never** include, link to, or be aware of upper layers (`UI`, `Presentation`, `Application`).
3. **Core Independence**: `Core` contains 0 business logic, 0 Valve-specific domain logic, and 0 UI/QML dependencies.

---

## Target Directory Structure & Module Placement

```text
src/
├── Core/                         # Reusable infrastructure (cs2importer_core)
│   ├── Error/                    # ImportErrorCode and ImportException
│   ├── FileSystem/               # FileSystem, AtomicFile, DirectorySnapshot
│   ├── KeyValues/                # KeyValuesNode, Document, Lexer, Parser, Writer (Single I/O AST)
│   ├── Logging/                  # Task-oriented logging, Sinks, FaultBarrier
│   ├── Path/                     # FilesystemPath, PathUtils (Sanitization)
│   ├── Process/                  # ProcessOptions, ProcessResult, ProcessRunner
│   ├── Temp/                     # TempFile and TempDirectory
│   └── CMakeLists.txt
│
├── Legacy/                       # Transitional unmigrated code (cs2importerlegacy executable)
│   ├── Main.cpp, Ui.h/.cpp, Miscellaneous.h/.cpp
│   ├── MapImporter, ModelImporter, ParticleImporter
│   ├── VmfBspProcess, MaterialFix, SoundscapeImport, FileExtractFromVPK
│   └── CMakeLists.txt
│
├── Domain/                       # Valve & Source 1/2 domain models, parsers, and tool adapters (cs2importer_domain)
│   ├── Asset/                    # AssetType, AssetTypeDetector, AssetPath
│   ├── Audio/                    # Soundscape extraction and VMF audio linking
│   ├── Bsp/                      # BSP unpacking, embedded file extraction
│   ├── Game/                     # GameInfo parser, SearchPath resolution (GameInfo, SearchTarget, SearchPathResolver, GameInfoParser)
│   ├── Material/                 # VMT / VMAT conversion, skybox, UV & shader fixing
│   ├── Package/                  # VPK asset extraction & indexing
│   ├── Tool/                     # Tool wrappers (bspsrc, source1import, vpkeditcli, etc.)
│   ├── Vmf/                      # VMF AST parsing, entity/brush manipulation, serialization
│   └── CMakeLists.txt
│
├── Workflow/                     # Concrete import pipeline use-cases
│   ├── Common/                   # ImportContext, CancellationToken, IImporter
│   ├── Map/                      # MapImporter workflow
│   ├── Model/                    # ModelImporter workflow
│   └── Particle/                 # ParticleImporter workflow
│
├── Application/                  # Application services & execution orchestration
│   ├── Config/                   # Configuration persistence (AppConfig / settings)
│   ├── Environment/              # Game / Steam library path detection
│   ├── Task/                     # WorkflowRunner, background worker thread management
│   └── Update/                   # UpdateService (version checking)
│
├── UI/                           # QML presentation controllers and ViewModels
│   ├── Controllers/              # Main UI controller, Tab controller
│   └── ViewModels/               # Property-binding models (MapViewModel, ModelViewModel, etc.)
│
└── qml/                          # QML interface declarations (cs2importer_qml)
    └── cs2importer/
        ├── CMakeLists.txt
        └── Main.qml
```

### Component Placement Mapping

| Existing / Legacy Component | Target Layer & Namespace | Target Path | Key Responsibilities |
| :--- | :--- | :--- | :--- |
| `Miscellaneous::RunCommandSync`, `PROGRAM_*` | `Domain::Tool` | `src/Domain/Tool/` | Structured invocation of external binaries via `Core::Process::ProcessRunner`. |
| `VmfBspProcess.h/.cpp` | `Domain::Vmf`<br>`Domain::Bsp` | `src/Domain/Vmf/`<br>`src/Domain/Bsp/` | VMF AST parsing/manipulation/serialization; BSP decompile orchestration. |
| `MaterialFix.h/.cpp` | `Domain::Material` | `src/Domain/Material/` | Material (VMT/VMAT) corrections, shader properties, UV fixes. |
| `SoundscapeImport.h/.cpp` | `Domain::Audio` | `src/Domain/Audio/` | Soundscape extraction and VMF entity sound connections. |
| `FileExtractFromVPK.h/.cpp` | `Domain::Package` | `src/Domain/Package/` | Typed asset extraction from VPK archives. |
| `Miscellaneous::ParseGameInfo`, `SearchTarget` | `Domain::Game` | `src/Domain/Game/` | `gameinfo.txt` parsing and game search path resolution. |
| `ModelImporter.h/.cpp` | `Workflow::Model` | `src/Workflow/Model/` | Model import pipeline (.mdl $\rightarrow$ .vmdl). |
| `ParticleImporter.h/.cpp` | `Workflow::Particle` | `src/Workflow/Particle/` | Particle import pipeline (.pcf $\rightarrow$ .vpcf). |
| `MapImporter.h/.cpp` | `Workflow::Map` | `src/Workflow/Map/` | Map import pipeline (BSP $\rightarrow$ VMF $\rightarrow$ compile/assets). |
| `Ui::AutoDetectPaths`, `IsValid*` | `Application::Environment` | `src/Application/Environment/` | Registry & Steam library scanning for game paths. |
| `Ui::CheckForUpdate` | `Application::Update` | `src/Application/Update/` | Update checking via network API. |
| `Ui::LoadFromCfg`, `SaveToCfg` | `Application::Config` | `src/Application/Config/` | Application configuration load/save. |
| `Ui::Start`, `m_workerThread`, `CancelAll` | `Application::Task` | `src/Application/Task/` | `WorkflowRunner`: Worker thread management, cancellation, exception handling. |
| `Ui.h/.cpp` (Q_PROPERTY, Slots) | `UI` | `src/UI/` | Thin presentation adapters and view models for QML. |

---

## Inter-Layer Communication & Routing Design

To eliminate legacy anti-patterns (such as global static options, global logger pointers, and cross-thread UI coupling), all inter-layer communication must follow these four routing patterns:

### 1. Context Pipeline Routing (Task Trigger & Execution)
* **Presentation $\rightarrow$ Application**: `UI` validates user input, constructs an immutable value object `ImportOptions` (using `Core::Path::FilesystemPath` and `Domain::Asset::AssetPath`), and passes it to `WorkflowRunner`.
* **Application $\rightarrow$ Workflow**: `WorkflowRunner` creates a `TaskLoggingContext`, an execution token / cancellation flag, and dispatches the task to a worker thread running the target `IImporter`.
* **Completion / Signals**: Upon completion or error, `WorkflowRunner` emits Qt signals using `Qt::QueuedConnection` back to the UI controller to update `isGoing`, `canGo`, or display result alerts.

### 2. Task-Oriented Logging & Progress Routing
* All importers and domain processors accept `std::shared_ptr<Core::Logging::TaskLoggingContext>`.
* **No global static loggers**: Components call `context->info(...)`, `context->updateProgress(...)`, or `context->reportFault(...)`.
* **Sinks**: File output is handled via `Core::Logging::FileSink`. UI console output is routed by implementing `Core::Logging::ILogSink` or subscribing to `LogManager` block commits, delivering log entries to the UI thread via queued signals.

### 3. Decoupled User Prompt & Confirmation Routing
When an importer requires interactive confirmation (e.g. overwriting files or unresolved dependencies), domain code must **not** directly call UI dialogs.
* **Interface**: Define `IUserPromptHandler` (pure virtual):
  ```cpp
  class IUserPromptHandler {
  public:
      virtual ~IUserPromptHandler() = default;
      virtual bool promptConfirmation(const QString& title, const QString& message) = 0;
  };
  ```
* **Implementation**: `Application` / `UI` provides an `AsyncPromptHandler` that uses `QMetaObject::invokeMethod` to trigger QML modals and blocks the worker thread safely via `QWaitCondition` until the user responds.

### 4. External Tool Execution Routing
Direct creation of `QProcess` or shell execution across business files is forbidden. All external CLI tools (`bspsrc`, `source1import`, `resourcecompiler`, `vpkeditcli`, `vtfcmd`) must be wrapped under `Domain::Tool`, returning structured `Core::Process::ProcessResult` using `Core::Process::ProcessRunner`.

---

## Refactor Status and Staged Roadmap

### Refactor Stages

1. **Stage 1 (Completed)**: Core infrastructure extraction (`src/Core`).
2. **Stage 2 (In Progress / Next)**: Domain foundations and tool wrappers:
   * [x] Implement `Domain::Game` (`GameInfo`, `SearchTarget`, `SearchPathResolver`, `GameInfoParser` under `src/Domain/Game/`).
   * [ ] Implement `Domain::Tool` (wrap tools with `Core::Process`).
   * [ ] Implement `Domain::Package` (VPK extraction with `Core::FileSystem`).
3. **Stage 3**: Standalone Importers & Domain Processors:
   * Migrate `ModelImporter` $\rightarrow$ `src/Workflow/Model/`.
   * Migrate `ParticleImporter` $\rightarrow$ `src/Workflow/Particle/`.
   * Refactor `MaterialFix` $\rightarrow$ `Domain::Material`.
   * Refactor `VmfBspProcess` $\rightarrow$ `Domain::Vmf` & `Domain::Bsp`.
4. **Stage 4**: Application Services & WorkflowRunner:
   * Implement `WorkflowRunner`, `ConfigService`, `GameDetectService`.
   * Connect `Core::Logging` to UI sink.
5. **Stage 5**: MapImporter Migration & UI Slimming:
   * Migrate `MapImporter` $\rightarrow$ `src/Workflow/Map/`.
   * Refactor `Ui.cpp` into thin ViewModels / Controllers under `src/UI/`.

> [!IMPORTANT]
> **Preserve Existing Behavior**: Do not combine refactor stages. When migrating a specific component, keep its functional behavior intact while replacing legacy utilities with the target architectural abstractions.

---

## Core API Reference

`src/Core/CMakeLists.txt` builds `cs2importer_core` as a static library. Its public include root is `src/`, so Core headers use project-root includes:

```cpp
#include "Core/Path/FilesystemPath.h"
#include "Core/FileSystem/FileSystem.h"
```

### Paths: `Core::Path`

`FilesystemPath` represents a path in the host filesystem.

```cpp
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"

Core::Path::FilesystemPath filePath(QStringLiteral("C:/game/assets/models/props/example.mdl"));

if (filePath.exists() && filePath.isFile()) {
    const auto parent = filePath.parentPath();
}
```

* `FilesystemPath` normalizes with `QDir::cleanPath` and provides `isEmpty`, `isValid`, `exists`, `isFile`, `isDirectory`, `fileName`, `extension`, `parentPath`, `absolutePath`, `canonicalPath`, `toString`.
* `PathUtils`:
  * `normalize(QString path)` $\rightarrow$ `QString`
  * `filename(QString path)` $\rightarrow$ `QString`
  * `extension(QString path)` $\rightarrow$ `QString`
  * `directory(QString path)` $\rightarrow$ `QString`
  * `relativePath(QString path, QString baseDir)` $\rightarrow$ `QString`
  * `sanitizeFilename(QString filename, QString replacement = "_")` $\rightarrow$ `QString` (strips host OS illegal chars `< > : " / \ | ? *`)

### KeyValues & Single I/O AST: `Core::KeyValues`

Generic Valve KeyValues (KV/VDF) single-pass parser, serializer, and in-memory AST.

```cpp
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesNode.h"

// 1. Single-read into in-memory AST
auto doc = Core::KeyValues::KeyValuesDocument::fromFile(vmfPath);

// 2. Perform multi-pass mutations entirely in memory
auto entities = doc.findChildren(QStringLiteral("entity"));
for (auto* ent : entities) {
    if (ent->property(QStringLiteral("classname")) == QStringLiteral("light")) {
        ent->setProperty(QStringLiteral("_light"), QStringLiteral("255 255 255 200"));
    }
}

// 3. Single atomic write back to disk
doc.saveToFile(vmfPath);
```

* **Single I/O Lifecycle**: Replaces legacy anti-patterns (e.g. 11 sequential disk reads and regex passes over the same VMF). Loads once into memory, transforms in-place, and saves atomically once via `Core::FileSystem::AtomicFile`.
* **Unquoted Token Support**: Supports unquoted keys and values (`skin 0`, `$basetexture custom/wall`, `( -64 -64 0 )`, `[PR#]{gate_trigger}`).
* **Special Character Resiliency**: Correctly distinguishes standalone syntax braces (`{` / `}`) from tokens containing braces (e.g. `{fence`, `{ladder.vmt`), and guarantees proper double-quoting upon serialization.
* **Multi-Key AST**: Preserves key ordering and duplicate siblings (`entity`, `solid`, `side`, `connections`), with rich querying (`findChild`, `findChildren`, `propertyInt`, `setProperty`).

### Errors: `Core::Error`

`ImportErrorCode` values: `Unknown`, `FileNotFound`, `InvalidPath`, `PermissionDenied`, `InvalidFile`, `DirectoryNotFound`, `ProcessFailed`, `ProcessTimeout`, `OperationFailed`.

`Core::Error::ImportException` derives from `QException` and stores an error code and message. Use `ImportException` in migrated code instead of legacy `AppException`.

### Filesystem: `Core::FileSystem`

* `FileSystem`: Static helpers for `exists`, `isFile`, `isDirectory`, `createDirectory`, `remove`, `copy`, `move`, `readAll`, `writeAll`.
* `AtomicFile`: Move-only RAII wrapper around `QSaveFile`. `writeAtomic(target, data)` is the one-shot helper.
* `DirectorySnapshot`: Captures relative directory snapshots (`FileEntry`) and computes diffs (`added`, `removed`, `modified`).

### Processes: `Core::Process`

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

`TempFile` and `TempDirectory` are move-only RAII wrappers. They clean up automatically upon destruction.

### Logging: `Core::Logging`

```cpp
auto task = Core::Logging::LogManager::instance().createTask(QStringLiteral("Import model"));
task->start();
task->info(QStringLiteral("Started"));
task->updateProgress(0.5, QStringLiteral("Converting"));
task->complete(QStringLiteral("Finished"));
```

---

## Domain API Reference

`src/Domain/CMakeLists.txt` builds `cs2importer_domain` as a static library linking `cs2importer_core` and `Qt6::Core`. Its public include root is `src/`:

```cpp
#include "Domain/Asset/AssetPath.h"
#include "Domain/Asset/AssetType.h"
#include "Domain/Asset/AssetTypeDetector.h"
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/SearchTarget.h"
```

### Assets & Asset Paths: `Domain::Asset`

`AssetPath` represents a validated game-asset-relative path (e.g. `materials/models/props/box.vmat`). `FilesystemPath` represents a host filesystem path.

```cpp
#include "Domain/Asset/AssetPath.h"
#include "Domain/Asset/AssetType.h"
#include "Domain/Asset/AssetTypeDetector.h"

Domain::Asset::AssetPath assetPath(QStringLiteral("models/props/example.mdl"));
Core::Path::FilesystemPath baseDir(QStringLiteral("C:/game/csgo"));

if (assetPath.isValid()) {
    const QString extension = assetPath.extension();
    const auto resolvedFile = assetPath.resolve(baseDir);
    const auto type = Domain::Asset::AssetTypeDetector::detect(assetPath);
}
```

* `AssetPath` normalizes backslashes to `/`, rejects absolute paths, drive letters, schemes, empty components, `.` and `..`, and stores only a valid relative path.
* Methods:
  * `resolve(FilesystemPath baseDir)` $\rightarrow$ `FilesystemPath`
  * `fromFilesystemPath(FilesystemPath baseDir, FilesystemPath filePath)` $\rightarrow$ `std::optional<AssetPath>`
  * `sanitizeAssetName(QString assetName, QString replacement = "_")` $\rightarrow$ `QString` (strips Source 2 / CS2 resource compiler illegal chars `{ } ^ # ~ + !`)
* `AssetTypeDetector::detect`:
  * Model: `mdl`, `vmdl`, `smd`, `fbx`
  * Particle: `pcf`, `vpcf`
  * Material: `vmt`, `vmat`, `vtf`
  * Map: `vmf`, `bsp`, `vmap`

### Game & Search Paths: `Domain::Game`

```cpp
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/SearchTarget.h"

// Parse gameinfo.txt into structured Domain model
Core::Path::FilesystemPath gameinfoPath(QStringLiteral("C:/game/cstrike/gameinfo.txt"));
auto gameInfo = Domain::Game::GameInfoParser::parse(gameinfoPath);

if (gameInfo) {
    const QString& title = gameInfo->game();
    int appId = gameInfo->steamAppId();
    const auto& baseDir = gameInfo->baseDirectory();
    const auto& searchTargets = gameInfo->searchTargets();

    for (const auto& target : searchTargets) {
        if (target.isVpk()) {
            // target.path() is a Core::Path::FilesystemPath to _dir.vpk
        } else {
            // target.path() is a directory path
        }
    }
}
```

* `SearchTarget`: Value object encapsulating `SearchTargetType` (`Directory` or `Vpk`) and `Core::Path::FilesystemPath`.
* `GameInfo`: Encapsulates game name, title, SteamAppId, ToolsAppId, modDirectory, baseDirectory, searchTargets, and `Core::KeyValues::KeyValuesDocument`.
* `SearchPathResolver`: Resolves Source 1 `SearchPaths` KV nodes (handles `|gameinfo_path|`, wildcard skipping, `.vpk` to `_dir.vpk` normalization, and deduplication).
* `GameInfoParser`: Parses `gameinfo.txt` via `Core::KeyValues`, detects game base directory from `game+game_write` or parent path fallback, and resolves all search paths.

---

## C++ and Coding Conventions

* Use C++17, Qt types, RAII, deterministic ownership, `const` correctness, and lightweight headers.
* Use `PascalCase` for classes/enums and `camelCase` for functions, methods, locals, and members.
* Preserve legacy behavior unless a task explicitly requests a behavior change.
* Do not duplicate Core facilities in newly migrated code.

## CMake Rules

* Require CMake 3.28+ and Qt 6.8+.
* Use `qt_standard_project_setup()` where appropriate.
* Use `qt_add_executable()` for the application, `qt_add_library()` for Core/Domain libraries, and `qt_add_qml_module()` for QML.
* Use `qt_add_resources()` only for non-QML resources.
* Prefer target-based configuration with explicit `PRIVATE`, `PUBLIC`, or `INTERFACE` visibility.
* Do not use global include paths when target-specific configuration is sufficient.
* Do not manually list generated MOC/RCC/QML compiler outputs.
* Do not use Qt 5 CMake APIs, `Qt5::` targets, qmake syntax, or `add_executable()` for the application.
* Do not put QML files in `qt_add_resources()`.

## Build and Tests

Standard local build using CMake Presets:

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --test-dir build/local-debug --output-on-failure
```

Tests reside under `tests/` (`test_logmanager`, `logging_test`, `test_keyvalues`, `test_gameinfo`).

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

* Do not make Core depend on application code, UI, QML, or Domain logic.
* Do not bypass layer boundaries (e.g. Domain directly triggering QML or UI widgets).
* Do not introduce new global static variables for options or loggers.
* Do not migrate MapImporter as part of another importer's migration.
* Do not perform unrelated refactoring during a focused migration.
* Do not add compatibility APIs solely for legacy code.

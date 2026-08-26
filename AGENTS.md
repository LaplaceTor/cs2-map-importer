# CS2 Map Importer — Agent Instructions

## 0. Purpose and Priority

This file is the **architecture contract for AI agents and human contributors** working in this repository.

The project is undergoing a staged refactor from a legacy monolithic Qt application into a strict layered architecture. The most important rule is:

> **Do not merely place files in the correct directory. The dependency graph and responsibilities must also obey the layer rules below.**

When these instructions conflict with a tempting local implementation shortcut, prefer the architecture contract and explicitly preserve behavior through adapters/facades rather than bypassing a layer.

### Non-negotiable invariants

1. Presentation/UI may call **Application only**.
2. Application may orchestrate **Workflow/Domain and Core**.
3. Workflow may call **Domain and Core**, but never UI/Application.
4. Domain may call **Core only**, never UI/Application/Workflow.
5. Core must be reusable infrastructure and know nothing about Valve business rules, workflows, Application services, or QML.
6. No layer may bypass the layer immediately below it to reach an upper/lower implementation for convenience.
7. **A directory name is not an architectural boundary by itself. The real boundary is the include/link/dependency graph.**

---

## 1. Project Overview

A Windows desktop GUI application that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.

* **Language:** C++17
* **Framework:** Qt 6.8+
* **Build system:** Modern CMake
* **Platform:** Windows (the project also has non-Windows build branches)
* **UI:** QML / Qt Quick Controls 2
* **QML style:** Fusion

The reusable Core layer has been extracted. Domain foundations and Application environment services are being migrated, while Workflow and remaining legacy components are still transitional.

---

## 2. Target Layered Architecture

```text
┌──────────────────────────────────────────────────────────────┐
│ Presentation / UI                                           │
│ QML Views <-> ViewModels / Controllers                      │
│                                                              │
│ Allowed: Qt/QML + Application contracts                     │
│ Forbidden: direct Domain/Core orchestration                  │
└─────────────────────────────┬────────────────────────────────┘
                              │ calls / connects
┌─────────────────────────────▼────────────────────────────────┐
│ Application                                                   │
│ Services / Task orchestration / Config / Update / Environment│
│                                                              │
│ Converts UI contracts into Domain/Workflow inputs            │
│ Owns async dispatch, lifecycle and UI-facing results         │
└─────────────────────────────┬────────────────────────────────┘
                              │ invokes / composes
┌─────────────────────────────▼────────────────────────────────┐
│ Workflow                                                     │
│ Concrete import use-cases / pipelines / cancellation         │
│                                                              │
│ May use Domain + Core                                        │
│ Never depends on UI or Application                           │
└─────────────────────────────┬────────────────────────────────┘
                              │ uses
┌─────────────────────────────▼────────────────────────────────┐
│ Domain                                                       │
│ Valve / Source 1/2 rules, models, parsers, processors, tools │
│                                                              │
│ May use Core only                                            │
│ No UI, Application, Workflow dependencies                    │
└─────────────────────────────┬────────────────────────────────┘
                              │ uses base infrastructure
┌─────────────────────────────▼────────────────────────────────┐
│ Core                                                         │
│ Path / FileSystem / KeyValues / Process / Logging / Temp     │
│ Error / generic infrastructure                              │
│                                                              │
│ No business logic / no Valve-specific policy / no UI         │
└──────────────────────────────────────────────────────────────┘
```

### Canonical dependency direction

```text
Presentation → Application → Workflow → Domain → Core
```

Application may also use Domain/Core directly for services that are not workflow execution, but **UI must not do so**.

There must be **no upward dependencies**:

```text
Core        ✗→ Domain / Workflow / Application / UI
Domain      ✗→ Workflow / Application / UI
Workflow    ✗→ Application / UI
Application ✗→ UI
```

### Why Workflow is separate from Application

* **Application** answers: *When, under which app-level policy, and on which execution context should something run?*
* **Workflow** answers: *How does the import use-case proceed from input to output?*
* **Domain** answers: *What do Valve/Source-specific rules and transformations mean?*
* **Core** answers: *How do we safely perform generic infrastructure operations?*

Do not put the concrete import pipeline into `Application` merely because the old code used one `Ui::Start()` function.

---

## 3. Hard Architectural Rules for Agents

### 3.1 Presentation/UI rules

`src/UI/` and `src/qml/` are presentation only.

UI may:

* expose `Q_PROPERTY`, Qt signals/slots, commands and presentation state;
* validate trivial presentation concerns (empty field, tab selection, required UI state);
* call Application services/facades;
* transform UI-native values into **Application-defined contracts**;
* display errors, progress, prompts and results returned by Application.

UI must **not**:

* include `Domain/*` directly;
* include `Core/*` directly for business execution or filesystem/process operations;
* call `Domain::*` validators, registries, parsers or processors;
* call `Core::Process::*`, `Core::FileSystem::*`, `Core::KeyValues::*`, etc. to perform application work;
* scan game directories, parse `gameinfo`, inspect Steam libraries, extract packages, run external tools, or mutate import assets;
* decide which Domain validator/processor/tool should execute;
* own workflow threads, worker pools, cancellation tokens or import lifecycle;
* create `QProcess` or invoke shell commands;
* use global/static business state for current imports.

### 3.2 Application rules

`src/Application/` contains application services and orchestration.

Application may:

* expose UI-facing service/facade APIs;
* translate Application contracts into Domain/Workflow inputs;
* coordinate `WorkflowRunner` and task execution;
* own async dispatch and worker-thread policy;
* coordinate Steam/game detection services;
* coordinate configuration, update checking and application lifecycle policies;
* connect logging/progress/result channels to UI adapters;
* implement interfaces required by lower layers when those interfaces belong to Application policy (for example prompt mediation).

Application must not:

* contain QML or direct widget manipulation;
* implement Valve-specific parsing/transformation that belongs in Domain;
* contain concrete map/model/particle import pipelines that belong in Workflow;
* bypass Workflow and call legacy import entry points merely because they are convenient;
* expose raw implementation details to QML when a stable Application contract can be returned.

### 3.3 Workflow rules

`src/Workflow/` contains concrete import use-cases.

Workflow may:

* define `IImporter`, `ImportContext`, cancellation, task-level progress and pipeline result types;
* sequence Domain processors/tools;
* own use-case-specific step ordering and error handling;
* receive `TaskLoggingContext` and cancellation primitives from Application;
* use Core infrastructure when required;
* return `Core::Async::TaskResult<T>` or `TaskResult<void>` to explicitly communicate `Success`, `Failure`, `Cancelled`, and `Skipped` execution outcomes.

Workflow must not:

* include or call UI classes/QML;
* depend on Application policy or configuration classes;
* show dialogs or directly request user interaction through QML;
* own global application configuration;
* discover Steam installations or perform application-wide environment selection unless the behavior is explicitly part of an importer use-case and passed in as data/services;
* use `std::optional<T>` or `bool` to implicitly encode business outcomes. All new Workflow APIs must strictly use `TaskResult<T>`.

### 3.4 Domain rules

`src/Domain/` contains Valve/Source-specific knowledge and deterministic domain operations.

Domain may:

* parse and validate Valve formats;
* model Source 1/2 concepts;
* resolve game search paths;
* process VMF/VMT/VMAT/BSP/VPK/domain assets;
* wrap external tools behind typed interfaces under `Domain::Tool`.

Domain must not:

* include `Application/*`, `Workflow/*`, `UI/*`, or QML;
* emit UI notifications or open dialogs;
* access `QQmlApplicationEngine`, `QGuiApplication`, widgets, or presentation classes;
* decide application-level lifecycle, retries, Steam discovery policy, configuration persistence, or update policy;
* call a global application logger.

If a Domain operation needs user confirmation, define a narrow interface at the lower-layer boundary (for example in `Workflow/Common` or another dependency-neutral interface location) and have Application provide the implementation. The Domain operation must remain unaware of QML.

### 3.5 Core rules

`src/Core/` is reusable infrastructure.

Core must not contain:

* game definitions such as CS2/CSGO/HL2-specific rules;
* import workflow decisions;
* Steam detection;
* VPK policy;
* material conversion logic;
* UI/QML code;
* Application service classes.

Core APIs should be generic enough to be reused outside CS2 Map Importer.

---

## 4. UI/Application Boundary Contract

This is a critical rule because the old architecture frequently placed business logic inside `Ui` classes.

### Preferred pattern

```text
QML
  ↓
ViewModel / Controller
  ↓
Application service / facade
  ↓
Workflow / Domain
  ↓
Core
```

### Application contract rule

UI-facing Application APIs should prefer **Application-owned DTOs/value objects** or plain Qt value types rather than requiring UI code to construct Domain/Core implementation types.

Preferred:

```cpp
Application::Environment::ValidateGameRequest request;
request.path = selectedPath;
request.gameId = selectedGameId;

auto result = gameEnvironmentService->validate(request);
```

Avoid:

```cpp
// BAD: UI chooses and constructs Domain implementation details.
auto type = Domain::Game::GameRegistry::stringToGameType(selectedType);
auto path = Core::Path::FilesystemPath(selectedPath);
auto result = Domain::Game::GameValidator::validateDirectory(path, type);
```

The Application service should perform the translation:

```text
UI string/path
   ↓
Application request DTO
   ↓
Application resolves GameType / FilesystemPath
   ↓
Domain validator
   ↓
Application result DTO
   ↓
UI properties/signals
```

A small exception is allowed for harmless Qt presentation utilities, but it must not create a route around Application to execute business operations.

---

## 5. Async / Threading Rules

Any potentially blocking operation must be treated as non-UI work.

Examples:

* filesystem scans;
* parsing large files;
* Steam library discovery;
* game validation involving disk I/O;
* VPK/package extraction;
* external processes;
* map/model/particle import pipelines.

Rules:

1. UI thread must remain responsive.
2. Application owns worker dispatch policy.
3. Workflow execution occurs off the UI thread.
4. UI updates must cross back through queued Qt delivery or an equivalent safe mechanism.
5. Do not capture raw UI-owned objects into detached worker tasks without a lifetime guard.
6. Do not make Domain code directly manage UI event-loop synchronization.
7. Cancellation must be explicit and cooperative; do not add ad-hoc shared global cancellation flags.

For services with both sync and async APIs, the sync primitive belongs below Application; the Application service may provide the async wrapper.

### Dual-Plane Architecture: Task Execution Lifecycle vs. Business Outcome

To maintain strict conceptual clarity across async tasks and workflow operations, the architecture defines two orthogonal planes:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. Task Execution Lifecycle Plane (TaskState)                               │
│    Managed by LogManager / TaskLoggingContext                               │
│    States: Pending → Running → Completed | Failed | Cancelled | Skipped    │
│    Tracked and displayed in UI log models (LogViewModel / LogTaskModel)     │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. Business Outcome Plane (TaskResult<T>)                                   │
│    Standard single-layer return contract for Workflow and Application APIs  │
│    Statuses: Success | Failure | Cancelled | Skipped + payload T & message  │
└─────────────────────────────────────────────────────────────────────────────┘
```

`AsyncTaskRunner` acts as the bridge connecting both planes:
* **Primary API**: `AsyncTaskRunner::runTask<T>(taskName, context, worker, callback)`
  * `T` is the **Business Payload Type** (e.g. `GameInstallationInfo`, `DetectionResult`, `void`), never `TaskResult<TaskResult<T>>`.
  * The worker returns a single-layer `TaskResult<T>`.
  * `AsyncTaskRunner` inspects the business outcome (and logged errors / exceptions) to transition the underlying `TaskState` in `LogManager`.
  * The callback receives `const TaskResult<T>&` delivered thread-safely to the caller's context thread.

---

## 6. Logging Rules

The project uses task-oriented logging.

### Forbidden

```cpp
Core::Logging::Logger::info(...);
Core::Logging::Logger::warning(...);
Core::Logging::Logger::error(...);
```

Do not add new global/static logger APIs, global logger pointers, or module-global logging state.

### Required direction

Application/Workflow creates or receives a task logging context and passes it downward:

```text
Application
  ↓
TaskLoggingContext
  ↓
Workflow / Domain
```

Code should use the context for task messages, progress and faults.

### Log Level & Task Lifecycle Contract

The logging system is semantically coupled with task lifecycle arbitration:

* **`error()` / `reportFault()`**: **Unrecoverable business failure for the current task**.
  * Any task that emits an `error()` is automatically transitioned to `TaskState::Failed` by `AsyncTaskRunner`, even if the worker returns normally or yields `TaskResult::success`.
  * **Rule:** Only call `error()` if the current task has actually failed. If a step failed but was recovered, handled, or retried, do not emit `error()`.
* **`warning()`**: **Recoverable issue, fallback, degradation, or skip**.
  * Emitting a `warning()` does **not** fail the task. Use `warning()` when an unexpected condition occurred but execution continued safely or fell back.
* **`info()`**: **High-level user-facing milestones only** (e.g. task starting, task completed, major asset/game discovered). Do not spam technical details at `info` level.
* **`debug()`**: **Technical diagnostics and internal step tracing** (e.g. filesystem search paths, parser details, tool CLI arguments).

### UI logging

A ViewModel may implement `ILogSink` or subscribe to logging events, but that adapter must marshal updates onto the UI thread.

Core logging sinks may route committed logs to files or UI adapters, but Core must not know that a UI exists.

---

## 7. User Prompts / Confirmation Rules

Lower layers must never call a modal dialog directly.

Forbidden examples:

```cpp
QMessageBox::question(...);
QQmlApplicationEngine ...;
qml dialog invocation from Domain/Workflow;
```

Required pattern:

```text
Workflow/Domain needs confirmation
        ↓
abstract prompt interface
        ↓
Application implements policy/bridge
        ↓
UI/QML performs the actual prompt
        ↓
result returns to Application/worker context
```

The prompt abstraction must not expose UI widgets to Domain/Workflow.

---

## 8. External Tool Execution Rules

Direct creation of external processes from business files is forbidden.

Forbidden:

```cpp
QProcess process;
process.start(...);

std::system(...);
popen(...);
WinExec(...);
```

All external CLI tools (`bspsrc`, `source1import`, `resourcecompiler`, `vpkeditcli`, `vtfcmd`, etc.) must be wrapped under:

```text
Domain::Tool
    ↓
Core::Process::ProcessRunner
```

Tool wrappers return structured `Core::Process::ProcessResult` or a domain-specific result that contains it. They must not print directly to a UI console or display dialogs.

---

## 9. Filesystem / I/O Rules

* Prefer `Core::Path::FilesystemPath` and `Core::FileSystem` in migrated layers.
* Do not reimplement path normalization, sanitization, atomic writes, leases or generic filesystem helpers in Domain/Application/UI.
* Use `Core::FileSystem::AtomicFile` for atomic replacement when the operation requires it.
* Avoid repeatedly reading/parsing the same file when a single in-memory AST can support the transformation.
* Use `Core::KeyValues` for Valve KeyValues/VDF/VMF-like documents instead of ad-hoc regex parsing when the file belongs to the supported KV grammar.
* Domain decides Valve-specific relative paths/search rules; Core only manipulates generic host filesystem paths.

---

## 10. Target Directory Structure

```text
src/
├── Core/
│   ├── Error/
│   ├── FileSystem/
│   ├── KeyValues/
│   ├── Logging/
│   ├── Path/
│   ├── Process/
│   ├── Temp/
│   └── CMakeLists.txt
│
├── Domain/
│   ├── Asset/
│   ├── Audio/
│   ├── Bsp/
│   ├── Game/
│   ├── Material/
│   ├── Package/
│   ├── Tool/
│   ├── Vmf/
│   └── CMakeLists.txt
│
├── Workflow/
│   ├── Common/
│   ├── Map/
│   ├── Model/
│   ├── Particle/
│   └── CMakeLists.txt
│
├── Application/
│   ├── Config/
│   ├── Environment/
│   ├── Task/
│   ├── Update/
│   └── CMakeLists.txt
│
├── UI/
│   ├── Controllers/
│   ├── ViewModels/
│   └── CMakeLists.txt
│
└── qml/
    └── cs2importer/
        ├── CMakeLists.txt
        └── Main.qml
```

`src/Legacy/` is transitional only. New code must not depend on Legacy unless the migration task explicitly requires a temporary bridge. Do not expand Legacy dependencies as a permanent architecture strategy.

---

## 11. Component Placement Mapping

| Existing / Legacy Component                    | Target Layer                                    | Target Path                          | Key Responsibility                                        |
| :--------------------------------------------- | :---------------------------------------------- | :----------------------------------- | :-------------------------------------------------------- |
| `Miscellaneous::RunCommandSync`, `PROGRAM_*`   | `Domain::Tool`                                  | `src/Domain/Tool/`                   | Typed wrappers over external tools using `Core::Process`. |
| `VmfBspProcess`                                | `Domain::Vmf` / `Domain::Bsp`                   | `src/Domain/Vmf/`, `src/Domain/Bsp/` | VMF processing and BSP/decompile behavior.                |
| `MaterialFix`                                  | `Domain::Material`                              | `src/Domain/Material/`               | VMT/VMAT/material conversion and fixes.                   |
| `SoundscapeImport`                             | `Domain::Audio`                                 | `src/Domain/Audio/`                  | Soundscape extraction and VMF audio linking.              |
| `FileExtractFromVPK`                           | `Domain::Package`                               | `src/Domain/Package/`                | Typed VPK/package extraction.                             |
| `Miscellaneous::ParseGameInfo`, `SearchTarget` | `Domain::Game`                                  | `src/Domain/Game/`                   | GameInfo parsing, validation and search-path resolution.  |
| `ModelImporter`                                | `Workflow::Model`                               | `src/Workflow/Model/`                | `.mdl → .vmdl` import pipeline.                           |
| `ParticleImporter`                             | `Workflow::Particle`                            | `src/Workflow/Particle/`             | `.pcf → .vpcf` import pipeline.                           |
| `MapImporter`                                  | `Workflow::Map`                                 | `src/Workflow/Map/`                  | BSP → VMF → compile/assets pipeline.                      |
| `Ui::AutoDetectPaths`, `IsValid*`              | `Application::Environment` + `Domain::Game`     | corresponding directories            | Application orchestration + Domain validation.            |
| `vpk.signatures` locking                       | `Application::Environment` + `Core::FileSystem` | corresponding directories            | Application policy + generic file lease.                  |
| `Ui::CheckForUpdate`                           | `Application::Update`                           | `src/Application/Update/`            | Update checking.                                          |
| `Ui::LoadFromCfg`, `SaveToCfg`                 | `Application::Config`                           | `src/Application/Config/`            | Configuration persistence.                                |
| `Ui::Start`, worker thread, `CancelAll`        | `Application::Task`                             | `src/Application/Task/`              | WorkflowRunner/task lifecycle.                            |
| `Ui.h/.cpp` Q_PROPERTY/slots                   | `UI`                                            | `src/UI/`                            | Thin presentation adapter only.                           |

---

## 12. CMake Dependency Enforcement

CMake must reflect the architectural dependency graph.

### Required module dependency graph

```text
cs2importer_core
    ↑
cs2importer_domain
    ↑
cs2importer_workflow
    ↑
cs2importer_application
    ↑
cs2importer_ui
    ↑
cs2importer executable / QML integration
```

Where a module does not yet exist during a staged migration, do not invent cross-layer shortcuts. Add the smallest temporary adapter and remove it when the target layer lands.

### Rules

* `cs2importer_core` must not link Domain/Application/UI.
* `cs2importer_domain` links Core only.
* `cs2importer_workflow` links Domain + Core.
* `cs2importer_application` links Workflow + Domain + Core as required by its services.
* `cs2importer_ui` should link Application and Qt UI modules. **Do not add Domain/Core merely to make UI implementation easier.**
* The final executable may link the top-level application/UI/QML targets as needed, but application logic must still respect the runtime dependency rules.
* Prefer `PRIVATE` linkage where downstream targets do not need the dependency transitively. Use `PUBLIC` only when the dependency is part of the library's public API.
* Avoid broad `${CMAKE_SOURCE_DIR}` include exposure when a target-local include root is sufficient.

### CMake architecture red flag

If a patch changes `src/UI/CMakeLists.txt` to add `cs2importer_domain` or `cs2importer_core` only because a ViewModel wants to call a Domain/Core function directly, **stop and redesign the boundary**.

---

## 13. Application Service Design

Application services should be **instance-based** by default, not giant collections of `static` convenience functions.

Prefer:

```cpp
class GameEnvironmentService {
public:
    ValidationResult validate(const ValidateGameRequest& request);

    void detectAsync(
        const DetectGamesRequest& request,
        std::function<void(DetectionResult)> callback);
};
```

Avoid growing APIs like:

```cpp
GameDetectService::validateSource1(...);
GameDetectService::validateSource2(...);
GameDetectService::inspectGameInfo(...);
GameDetectService::detectAllGames(...);
...
```

A static helper is acceptable for a genuinely stateless pure function, but services that own policies, dependencies, lifecycle, logging, caching, or asynchronous work should be injected instances.

When a service becomes responsible for multiple unrelated concerns, split it instead of turning it into a universal environment utility.

---

## 14. Domain API Design

Domain APIs should be deterministic and testable.

Prefer:

```text
input value(s)
    → domain operation
    → value/result/error
```

Avoid Domain APIs that:

* reach into UI state;
* consult application-global state;
* show dialogs;
* spawn worker threads solely to hide a design problem;
* silently mutate unrelated global configuration;
* return presentation-specific structures.

Use strong domain types (`GameType`, `AssetPath`, `SearchTarget`, etc.) once the call has crossed from Application into Domain.

---

## 15. Error Handling Rules

* Use structured `Core::Error` / `ImportException` or typed result objects instead of stringly-typed control flow where practical.
* Preserve enough context to explain the failure to Application and UI.
* Domain errors should describe domain/infrastructure failure, not UI wording.
* Application may translate lower-layer errors into user-facing messages.
* Do not bury an error behind `catch (...) {}` unless the exception is intentionally treated as non-fatal and the failure is documented.
* Do not return `true` merely because an operation was skipped unless the public contract defines that state as success/no-op.

---

## 16. Logging API Reference

Task-oriented logging is the default architecture:

```cpp
auto task =
    Core::Logging::LogManager::instance()
        .createTask(QStringLiteral("Import model"));

task->start();
task->info(QStringLiteral("Started"));
task->updateProgress(0.5, QStringLiteral("Converting"));
task->complete(QStringLiteral("Finished"));
```

Migrated Workflow/Domain code should receive the appropriate task context from its caller rather than obtaining an unrelated global logger.

UI log sinks must be thread-safe and must use queued delivery for QObject state changes when messages originate off the UI thread.

---

## 17. Core API Reference

`src/Core/CMakeLists.txt` builds `cs2importer_core` as a static library. Its include root is `src/`.

### `Core::Path`

```cpp
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"

Core::Path::FilesystemPath filePath(
    QStringLiteral("C:/game/assets/models/props/example.mdl"));

if (filePath.exists() && filePath.isFile()) {
    const auto parent = filePath.parentPath();
}
```

`FilesystemPath` provides normalized host filesystem path operations including existence checks, path decomposition and canonical/absolute forms.

`PathUtils` provides generic normalization, filename/extension/directory extraction, relative paths and host-filename sanitization.

### `Core::KeyValues`

Generic Valve KeyValues/VDF AST parser and writer. The intended lifecycle is:

```text
load once → mutate/query in memory → save once
```

It supports unquoted tokens, nested sections, duplicate siblings, ordering preservation and atomic writes through Core filesystem facilities.

### `Core::FileSystem`

Includes generic filesystem helpers, `AtomicFile`, `DirectorySnapshot` and the move-only `FileLease` RAII wrapper.

`FileLease` is generic infrastructure. Policies such as “lease CS2 `vpk.signatures` while the app is active” belong in Application.

### `Core::Process`

```cpp
Core::Process::ProcessOptions options;
options.timeout = 60000;
options.workingDirectory = workingDirectory;

const auto result =
    Core::Process::ProcessRunner::run(executable, arguments, options);
```

### `Core::Temp`

`TempFile` and `TempDirectory` are move-only RAII wrappers for temporary resources.

### `Core::Error`

`Core::Error::ErrorCode`, `Core::Error::Error`, and `Core::Error::Exception` are generic infrastructure and application error primitives. `ProcessResult` converts cleanly to `Core::Error::Error` via `.toError()`. `TaskResult<T>` natively carries `Core::Error::Error`. `ImportErrorCode` and `ImportException` are retained as backward compatibility aliases. Prefer structured error codes and `TaskResult<T>` over throwing exceptions or dropping error reasons via `std::optional` in migrated code.

---

## 18. Domain API Reference

`src/Domain/CMakeLists.txt` builds `cs2importer_domain` and depends on Core.

### `Domain::Asset`

`AssetPath` represents a validated asset-relative path. `AssetTypeDetector` classifies model/particle/material/map assets. Domain code may resolve an `AssetPath` against a host filesystem base path using Core path infrastructure.

### `Domain::Game`

Important domain types include:

* `GameType`, `EngineType`;
* `GameDefinition`, `GameRegistry`;
* `GameInfo`, `GameInfoParser`;
* `SearchTarget`, `SearchPathResolver`;
* `GameValidator`.

These types own Source/Valve semantics. They must remain independent from Application and UI.

Example:

```cpp
Core::Path::FilesystemPath path(
    QStringLiteral("C:/game/cstrike/gameinfo.txt"));

auto info = Domain::Game::GameInfoParser::parse(
    path,
    Domain::Game::EngineType::Source1);
```

Adding a new supported game should normally be declaration/metadata driven through `GameDefinition` / `GameRegistry`, not by duplicating detection branches throughout Application/UI.

---

## 19. Application API Reference

Current Application environment responsibilities include:

* `SteamService`: Steam install/library discovery and app manifest reading;
* `GameInstallation`: application-level representation of a detected game installation;
* `GameDetectService`: detection/validation orchestration;
* `VpkSignatureLeaseService`: application policy for exclusive leasing of CS2 `vpk.signatures`.

These services should evolve toward cohesive, instance-based Application services as the migration proceeds.

### Application-facing result design

Application should return result objects suitable for UI consumption, for example:

```cpp
struct ValidationResult {
    bool valid = false;
    QString gameId;
    QString displayName;
    QString path;
    QString userMessage;
};
```

The exact type can vary, but do not expose Domain ASTs or Core implementation objects to QML as the normal UI API.

---

## 20. Refactor Roadmap

The roadmap is staged. **Do not merge stages merely to make a shortcut compile.**

1. **Stage 1 — Core extraction**

   * Completed.

2. **Stage 2 — Domain foundations**

   * Game model/parser/registry/validator;
   * Tool wrappers under `Domain::Tool`;
   * Package/VPK extraction under `Domain::Package`.

3. **Stage 3 — Importer/domain migrations**

   * ModelImporter → `Workflow::Model`;
   * ParticleImporter → `Workflow::Particle`;
   * MaterialFix → `Domain::Material`;
   * VmfBspProcess → `Domain::Vmf` + `Domain::Bsp`.

4. **Stage 4 — Application orchestration**

   * WorkflowRunner;
   * ConfigService;
   * UpdateService;
   * complete task/cancellation/logging routing.

5. **Stage 5 — MapImporter and UI slimming**

   * MapImporter → `Workflow::Map`;
   * reduce UI to presentation + Application calls only.

### Migration rule

When migrating one component:

* preserve existing behavior unless the task explicitly changes it;
* replace legacy utilities with the target layer's abstraction;
* do not introduce a temporary upward dependency “just for this migration”;
* if a temporary bridge is unavoidable, isolate it, document it, and create a clear removal path;
* do not combine unrelated refactor stages.

---

## 21. Architecture Change Procedure — MUST FOLLOW

Before changing code, an agent must answer these questions:

1. **What layer owns this behavior?**
2. **What is the lowest layer that can implement it without violating the dependency graph?**
3. **Who should orchestrate it?**
4. **What is the public contract crossing the layer boundary?**
5. **Does the proposed header include anything above the current layer?**
6. **Will the CMake target dependency graph remain one-way?**
7. **Does the operation block the UI thread?**
8. **Does the change introduce global state, global logging, direct QProcess usage, or UI coupling?**

If any answer reveals a boundary violation, redesign before implementation.

### Required implementation order

Prefer this sequence:

```text
1. Define/adjust lower-layer contract
2. Implement Domain/Core behavior
3. Add Application orchestration/facade
4. Connect UI to Application contract
5. Add/adjust tests
6. Verify include + CMake dependency direction
```

Do not start by putting business logic into a ViewModel and promise to “move it later”.

---

## 22. Mandatory Architecture Review Checklist

After every non-trivial refactor, review the diff against this checklist.

### Layer ownership

* [ ] Every changed function belongs to the correct layer.
* [ ] No UI class contains Domain orchestration.
* [ ] No Application class contains concrete Domain transformation logic that belongs below it.
* [ ] No Domain/Core class knows about Application/UI.

### Dependency graph

* [ ] No new upward include.
* [ ] No new CMake dependency that points upward.
* [ ] UI does not add direct Domain/Core linkage to access implementation details.
* [ ] Workflow does not depend on Application/UI.

### Runtime behavior

* [ ] Blocking I/O does not execute on the UI thread.
* [ ] Worker callbacks are lifetime-safe and return to UI with queued delivery.
* [ ] Cancellation is explicit and deterministic.

### Integration boundaries

* [ ] No direct `QProcess`/shell execution outside `Domain::Tool` + `Core::Process`.
* [ ] No direct modal dialogs outside the Application/UI prompt bridge.
* [ ] No global/static logger introduced.
* [ ] No new global mutable application state.

### API quality

* [ ] UI receives Application contracts, not Domain ASTs/implementation objects.
* [ ] Domain APIs use strong Domain types.
* [ ] Errors are structured and preserve useful diagnostic context.
* [ ] New helpers do not duplicate existing Core facilities.

### Tests

* [ ] New Domain/Core logic has isolated unit coverage where practical.
* [ ] Application routing is tested for the affected service.
* [ ] UI tests cover state/signals rather than re-testing Domain internals.

---

## 23. Architecture Smells That Require a Redesign

Treat these as review blockers unless the task explicitly targets legacy migration code:

```text
UI/ViewModel → Domain::GameValidator
UI/ViewModel → Core::FileSystem
UI/ViewModel → Core::KeyValues
UI/ViewModel → Core::Process
UI/ViewModel → Steam registry/library scanning

Application → QML object manipulation
Domain → Application
Domain → UI
Domain → QMessageBox / QWidget / QQml...
Workflow → UI
Workflow → Application

Any business file → QProcess / system() / shell
Any business file → global Logger::info/error/warning

Massive static Application service with unrelated responsibilities
Temporary cross-layer include with no removal plan
```

If an existing legacy path already contains one of these smells, do not copy the pattern into new code. Migrate it toward the target boundary.

---

## 24. C++ and Coding Conventions

* Use C++17, Qt types where appropriate, RAII, deterministic ownership and `const` correctness.
* Use `PascalCase` for classes/enums and `camelCase` for functions, methods, locals and members.
* Keep headers lightweight and self-contained.
* Include every standard library header directly required by a translation unit; do not rely on transitive includes.
* Preserve behavior unless the task explicitly requests a behavior change.
* Do not duplicate Core facilities in newly migrated code.
* Prefer `std::optional`, typed result objects and explicit ownership over sentinel values when appropriate.
* Prefer dependency injection for services with policies, state, logging or asynchronous work.

---

## 25. CMake Rules

* Require CMake 3.28+ and Qt 6.8+.
* Use `qt_standard_project_setup()` where appropriate.
* Use `qt_add_executable()` for the application.
* Use `qt_add_library()` for Core/Domain/Workflow/Application/UI libraries as appropriate.
* Use `qt_add_qml_module()` for QML modules.
* Use `qt_add_resources()` only for non-QML resources.
* Prefer target-based configuration with explicit `PRIVATE`, `PUBLIC`, or `INTERFACE` visibility.
* Do not use global include paths when target-specific configuration is sufficient.
* Do not manually list generated MOC/RCC/QML compiler outputs.
* Do not use Qt 5 CMake APIs, `Qt5::` targets, qmake syntax, or plain `add_executable()` for the main Qt application.
* Do not put QML files in `qt_add_resources()`.

---

## 26. Build and Tests

### Main application build

```bash
cmake -B build -S .
cmake --build build
```

Or with presets when configured:

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Tests

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --test-dir build/local-debug --output-on-failure
```

Or:

```bash
cmake -B build-tests -S tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

When changing a layer, prefer tests appropriate to that layer rather than exercising the entire GUI for every low-level change.

---

## 27. Test Structure & Test Dependency Rules

Typical test suites include:

* `TestLogManager.cpp`: logging, `FaultBarrier`, `LogManager`;
* `LoggingStressTest.cpp`: concurrent logging;
* `TestKeyValues.cpp`: KeyValues parsing/token/serialization;
* `TestGameInfo.cpp`: GameInfo, SearchPathResolver, GameValidator;
* `TestAsset.cpp`: AssetPath and AssetTypeDetector;
* `TestEnvironment.cpp`: SteamService/GameDetectService;
* `TestUiViewModels.cpp`: GameViewModel/LogViewModel/MainController;
* `TestFileLease.cpp`: FileLease and VpkSignatureLeaseService.

Test dependency rules mirror production:

* Domain tests may depend on Domain + Core.
* Workflow tests may depend on Workflow + Domain + Core.
* Application tests may depend on Application + its lower layers.
* UI tests may depend on UI + Application contracts/services as needed.
* Tests must not justify a production-layer violation merely to reach an implementation detail.

---

## 28. Skills — Auto-Load Rules

Before making changes, read the relevant skill:

| Task                | Skill                              |
| ------------------- | ---------------------------------- |
| C++ implementation  | `skills/qt-cmake-project/SKILL.md` |
| CMake/build changes | `skills/qt-cmake-project/SKILL.md` |
| QML implementation  | `skills/qt-qml/SKILL.md`           |
| C++ review          | `skills/qt-cpp-review/SKILL.md`    |
| QML review          | `skills/qt-qml-review/SKILL.md`    |
| UI/UX decisions     | `skills/qt-ui-design/SKILL.md`     |

For architecture refactors, read the relevant C++ review/project skill **before editing code**, then run the architecture review checklist above after the change.

---

## 29. Do NOT

* Do not make Core depend on Domain/Application/Workflow/UI/QML.
* Do not make Domain depend on Application/Workflow/UI/QML.
* Do not make Workflow depend on Application/UI.
* Do not make UI call Domain/Core directly for application behavior.
* Do not add direct Domain/Core linkage to `cs2importer_ui` as a shortcut.
* Do not put game detection, Steam scanning, file validation, import logic or external tool execution into ViewModels/Controllers.
* Do not add new global static variables for options, services, cancellation or loggers.
* Do not call `QProcess`, `system()`, shell commands or Windows process APIs directly from UI/Application business code when a `Domain::Tool`/`Core::Process` abstraction is required.
* Do not show dialogs from Domain/Workflow.
* Do not migrate MapImporter as part of another importer's migration.
* Do not perform unrelated refactoring during a focused migration.
* Do not add compatibility APIs solely for legacy code.
* Do not hide a layer violation behind `static` functions, convenience helpers, friend declarations, broad include paths, or transitive CMake linkage.
* Do not treat passing compilation as proof that the architecture is correct.

---

## 30. Final Rule

When in doubt, choose the design that makes the dependency graph **more obvious, more one-directional, more testable, and harder to violate accidentally**.

The correct question is not:

> “Where can I put this code so the current build works?”

It is:

> “Which layer owns this responsibility, what contract crosses the boundary, and how do I implement it without making any layer aware of layers above it?”

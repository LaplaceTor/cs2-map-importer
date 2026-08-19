# CS2 Map Importer — Agent Instructions

## Project Overview

A Windows desktop GUI application that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.

* **Language:** C++17
* **Framework:** Qt 6.8+
* **Build system:** Modern CMake
* **Platform:** Windows
* **UI:** QML / Qt Quick Controls 2
* **QML style:** Fusion

The project is undergoing a staged architecture refactor.

**Current status: only the Core layer and its CMake target have been refactored. The application/importer layer has not yet been migrated to the new Core APIs, and no intentional runtime behavior changes have been made as part of this refactor.**

## Current Architecture

```text
src/
├── Core/                         # Refactored reusable infrastructure
│   ├── Asset/                    # Asset types and detection
│   ├── Error/                    # Import errors and exceptions
│   ├── FileSystem/               # Filesystem operations
│   ├── Logging/                  # Logging infrastructure
│   ├── Path/                     # AssetPath / FilesystemPath
│   ├── Process/                  # External process execution
│   ├── Temp/                     # Temporary files/directories
│   └── CMakeLists.txt
│
├── Main.cpp                      # Existing application entry point
├── Ui.h / Ui.cpp                 # Existing QML-facing controller
│
├── ModelImporter.h/.cpp          # Existing importer; not yet migrated to Core
├── ParticleImporter.h/.cpp       # Existing importer; not yet migrated to Core
├── MapImporter.h/.cpp            # Existing legacy implementation
│
├── Miscellaneous.h/.cpp          # Existing legacy utilities
├── VmfBspProcess.h/.cpp          # Existing legacy map workflow
├── FileExtractFromVPK.h/.cpp     # Existing legacy map workflow
├── MaterialFix.h/.cpp            # Existing legacy map workflow
├── SoundscapeImport.h/.cpp       # Existing legacy map workflow
│
├── qml/
│   └── main.qml
└── CMakeLists.txt
```

## Refactor Status

The refactor is intentionally staged. The stages below describe the intended direction, not work that has already been completed.

### Completed

1. **Core layer refactor**
   - The reusable Core infrastructure has been extracted under `src/Core`.
   - Core is built as the independent `cs2importer_core` CMake target.
   - `AssetPath` and `FilesystemPath` are separate value types.

### Not yet completed

2. **ModelImporter migration**
   - Planned next stage.
   - ModelImporter still uses the existing application/legacy infrastructure.
   - Do not assume it already uses Core.

3. **ParticleImporter migration**
   - Planned next stage.
   - ParticleImporter still uses the existing application/legacy infrastructure.
   - Do not assume it already uses Core.

4. **MapImporter migration**
   - Intentionally deferred until later.
   - Keep the existing implementation and behavior unless explicitly asked to change it.

5. **UI/QML and remaining application infrastructure**
   - Not yet refactored.
   - Treat the current UI and QML architecture as legacy/current-state code, not as the final architecture.

### Important scope rule

At the current stage, **do not perform application-layer migration merely because Core exists**.

When a task explicitly asks to migrate an importer or another application component, use the existing Core APIs where appropriate. Otherwise, preserve the current application behavior and architecture.

Do not combine refactor stages unless explicitly requested.

## Runtime Behavior

The Core refactor is an architectural/code-organization change. **It has not yet been followed by an intentional rewrite of the application's actual import workflows or runtime behavior.**

Therefore:

* Do not assume that the application has been converted to the new Core architecture.
* Do not claim that ModelImporter, ParticleImporter, or MapImporter have been migrated unless the task explicitly performs that migration.
* Do not introduce behavioral changes while making unrelated structural changes.
* When implementing a future migration, preserve existing importer behavior unless the migration explicitly specifies a behavioral change.

## Core Architecture Rules

`src/Core` is reusable infrastructure. It must remain independent from application workflows and UI.

Dependency direction:

```text
Application / Importers
        ↓
      Core
```

Never:

```text
Core → Importer
Core → UI
Core → QML
Core → application workflow
```

Core must not depend on:

* `MapImporter`
* `ModelImporter`
* `ParticleImporter`
* `Ui`
* QML
* legacy application utilities

### Core APIs

Use the Core API whenever an equivalent facility exists **when working on code that is being migrated to Core**.

Examples:

* Filesystem operations → `Core::FileSystem`
* Asset classification → `Core::Asset`
* Asset-relative paths → `Core::Path::AssetPath`
* Filesystem paths → `Core::Path::FilesystemPath`
* Process execution → `Core::Process`
* Temporary resources → `Core::Temp`
* Logging → `Core::Logging`
* Import errors → `Core::Error`

Do not duplicate these facilities in newly migrated code.

### Path Types

`AssetPath` and `FilesystemPath` are intentionally separate value types.

Do not merge them.

Do not reintroduce the old `AssetPath::type()` design.

Do not add compatibility methods that existed only in the old path implementation.

## C++ Conventions

### New and migrated code

* Use C++17.
* Prefer Qt types where they form the established application API:
  `QString`, `QByteArray`, `QFile`, `QDir`, `QProcess`, etc.
* Use `PascalCase` for classes and enum types.
* Use `camelCase` for functions, methods, local variables, and members.
* Prefer RAII and deterministic ownership.
* Prefer small value types with explicit responsibilities.
* Use `const` and references appropriately.
* Avoid unnecessary copies.
* Keep headers lightweight where practical.
* Use the Core error model instead of introducing new uses of the legacy `AppException` in migrated code.

### Existing legacy code

Do not perform unrelated style cleanup while migrating functionality.

Preserve legacy behavior unless the migration explicitly requires behavioral changes.

## CMake Rules

The project uses modern Qt 6 CMake APIs.

* Require **CMake 3.28+**.
* Require **Qt 6.8+**.
* Use `qt_standard_project_setup()` where the project structure requires it.
* Use `qt_add_executable()` for the application.
* Use `qt_add_library()` for Core.
* Use `qt_add_qml_module()` for QML.
* Use `qt_add_resources()` only for non-QML resources.
* Prefer target-based configuration.
* Use explicit `PRIVATE`, `PUBLIC`, or `INTERFACE` visibility.
* Do not use global include paths when a target-specific include path is sufficient.
* Do not manually list generated MOC/RCC/QML compiler outputs.
* Do not use Qt 5 CMake APIs.
* Do not use qmake syntax.

### Core Target

Core is currently an independent CMake target:

```text
cs2importer_core
```

The application links against this target.

Do not add Core source files directly to the application target.

Core's public include root is:

```text
src/
```

Therefore Core headers are included as:

```cpp
#include "Core/Path/AssetPath.h"
```

rather than relative paths.

## Build

Standard local build:

```bash
cmake -S . -B build
cmake --build build --config Release
```

If `CMakePresets.json` is introduced, prefer presets for normal development and CI.

## Skills — Auto-Load Rules

Before making changes, read the relevant skill.

| Task                | Skill                              |
|---------------------|------------------------------------|
| C++ implementation  | `skills/qt-cmake-project/SKILL.md` |
| CMake/build changes | `skills/qt-cmake-project/SKILL.md` |
| QML implementation  | `skills/qt-qml/SKILL.md`           |
| C++ review          | `skills/qt-cpp-review/SKILL.md`    |
| QML review          | `skills/qt-qml-review/SKILL.md`    |
| UI/UX decisions     | `skills/qt-ui-design/SKILL.md`     |

When modifying CMake, also consult the relevant `qt-cmake-project` references, especially:

* `simple-project.md`
* `modular-architecture.md`
* `qml-integration.md`
* `resources.md`
* `common-mistakes.md`

## Do NOT

* Do not use `qt5_*` CMake APIs.
* Do not use `Qt5::` targets.
* Do not use `add_executable()` for the application.
* Do not put QML files in `qt_add_resources()`.
* Do not manually add generated files.
* Do not make Core depend on application code.
* Do not duplicate Core functionality in newly migrated importer code.
* Do not add compatibility APIs solely for legacy code.
* Do not migrate ModelImporter or ParticleImporter unless the task explicitly requests that migration.
* Do not migrate `MapImporter` as part of another importer's migration.
* Do not perform unrelated refactoring during a focused migration task.
* Do not assume that the current runtime/import behavior has already been rewritten around Core.

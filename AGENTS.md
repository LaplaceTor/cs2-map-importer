# CS2 Map Importer — Agent Instructions

## Project Overview

A Windows desktop GUI application that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.

* **Language:** C++17
* **Framework:** Qt 6.8+
* **Build system:** Modern CMake
* **Platform:** Windows
* **UI:** QML / Qt Quick Controls 2
* **QML style:** Fusion

The project is undergoing a staged architecture migration.

**Core has been implemented and is now the foundation for new application code.**

## Architecture

```text
src/
├── Core/                         # Reusable application infrastructure
│   ├── Asset/                    # Asset types and detection
│   ├── Error/                    # Import errors and exceptions
│   ├── FileSystem/               # Filesystem operations
│   ├── Logging/                  # Logging infrastructure
│   ├── Path/                     # AssetPath / FilesystemPath
│   ├── Process/                  # External process execution
│   ├── Temp/                     # Temporary files/directories
│   └── CMakeLists.txt
│
├── Main.cpp                      # Application entry point
├── Ui.h / Ui.cpp                 # QML-facing controller
│
├── ModelImporter.h/.cpp          # Migration target: Core-based
├── ParticleImporter.h/.cpp       # Migration target: Core-based
├── MapImporter.h/.cpp            # Legacy implementation for now
│
├── Miscellaneous.h/.cpp          # Legacy utilities
├── VmfBspProcess.h/.cpp          # Legacy map workflow
├── FileExtractFromVPK.h/.cpp     # Legacy map workflow
├── MaterialFix.h/.cpp            # Legacy map workflow
├── SoundscapeImport.h/.cpp       # Legacy map workflow
│
├── qml/
│   └── main.qml
└── CMakeLists.txt
```

## Migration Status

The migration is intentionally staged.

1. **Core** is complete and should be treated as the new foundation.
2. **ModelImporter** should be migrated completely to Core.
3. **ParticleImporter** should be migrated completely to Core.
4. **MapImporter** remains on the legacy implementation for now.
5. UI/QML and remaining legacy infrastructure will be refactored separately.

Do not combine these migration stages unless explicitly requested.

### Important

Do not redesign Core merely to accommodate legacy code.

When migrating an importer:

* Prefer adapting the importer to existing Core APIs.
* Only modify Core when a genuine defect or missing general-purpose capability is identified.
* Do not introduce compatibility APIs solely to make old code compile.
* Preserve existing importer behavior unless the migration explicitly changes it.

## Core Architecture Rules

`src/Core` is infrastructure. It must remain independent from application workflows and UI.

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

Use the Core API whenever an equivalent facility exists.

Examples:

* Filesystem operations → `Core::FileSystem`
* Asset classification → `Core::Asset`
* Asset-relative paths → `Core::Path::AssetPath`
* Filesystem paths → `Core::Path::FilesystemPath`
* Process execution → `Core::Process`
* Temporary resources → `Core::Temp`
* Logging → `Core::Logging`
* Import errors → `Core::Error`

Do not duplicate these facilities in importer code.

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
* Use the Core error model instead of the legacy `AppException`.

### Legacy code

Do not perform unrelated style cleanup while migrating functionality.

Preserve legacy behavior unless the migration explicitly requires behavioral changes.

## CMake Rules

The project uses modern Qt 6 CMake APIs.

* Require **CMake 3.28+**.
* Require **Qt 6.8+**.
* Use `qt_standard_project_setup()`.
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

Core is an independent CMake target:

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
| ------------------- | ---------------------------------- |
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
* Do not duplicate Core functionality in importer code.
* Do not reintroduce `AssetPath::type()`.
* Do not add compatibility APIs solely for legacy code.
* Do not migrate `MapImporter` as part of the ModelImporter/ParticleImporter migration.
* Do not perform unrelated refactoring during a focused migration task.

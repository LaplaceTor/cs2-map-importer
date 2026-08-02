# CS2 Map Importer — Agent Instructions

## Project Overview

A **desktop GUI application** that imports Source 1 game assets (maps, models, particles) into Counter-Strike 2.
It wraps external CLI tools (source1import, cs_mdl_import, resourcecompiler, vpkedit, vtfcmd, bspsrc) and orchestrates multi-step import workflows.

- **Language:** C++17 + QML
- **Framework:** Qt ≥ 6.8
- **Build system:** CMake ≥ 3.10
- **Platform:** Windows
- **QML style:** Fusion

## Architecture

```
src/
  Main.cpp              # Entry point — creates Backend, exposes to QML via context property
  Ui.h / Ui.cpp         # Backend (QObject) — single QML-facing controller class
  Miscellaneous.h/.cpp  # Static utilities: logging, process execution, file ops, Options struct
  MapImporter.h/.cpp    # Map import workflow logic
  ModelImporter.h/.cpp  # Model (.mdl) import workflow logic
  ParticleImporter.h/.cpp # Particle (.pcf) import workflow logic
  VmfBspProcess.h/.cpp  # VMF/BSP decompile & preprocessing
  FileExtractFromVPK.h/.cpp # VPK archive extraction
  MaterialFix.h/.cpp    # Material path fixup
  SoundscapeImport.h/.cpp # Soundscape import
  qml/
    main.qml            # Entire UI (single-file QML, ~750 lines)
```

## Code Conventions

### C++

| Rule | Example |
|------|--------|
| PascalCase for methods & properties | `GetCs2Basefolder()`, `SetAddonName()` |
| camelCase for member variables | `cs2Basefolder`, `isGoing` |
| Qt types preferred over STL | `QString`, `QStringList`, `QDir` |
| One class per .h/.cpp pair | `MapImporter.h` + `MapImporter.cpp` |

### CMake

- Use `qt_add_executable` (NOT `add_executable`).
- Use `qt_add_qml_module` for QML files (NOT `qt_add_resources` for QML).
- Use `qt6_*` / `Qt6::` targets (NEVER `qt5_*`).
- `CMAKE_AUTOMOC` and `CMAKE_AUTORCC` are ON.

## Build & Run

```bash
cmake --preset default
cmake --build build --config Release
```

## Skills — Auto-Load Rules

**IMPORTANT:** Before making code changes, the agent MUST read the relevant skill(s) below by invoking the Skill tool or reading the SKILL.md file.

| When to load | Skill | Path |
|-------------|-------|------|
| Writing or modifying **C++ code** | `qt-cmake-project` | `skills/qt-cmake-project/SKILL.md` |
| Writing or modifying **QML code** | `qt-qml` | `skills/qt-qml/SKILL.md` |
| Reviewing C++ changes | `qt-cpp-review` | `skills/qt-cpp-review/SKILL.md` |
| Reviewing QML changes | `qt-qml-review` | `skills/qt-qml-review/SKILL.md` |
| Modifying **CMakeLists.txt** or build config | `qt-cmake-project` | `skills/qt-cmake-project/SKILL.md` |
| UI/UX design decisions | `qt-ui-design` | `skills/qt-ui-design/SKILL.md` |

### Skill reference index

| Skill | Type | Description |
|-------|------|-------------|
| `qt-cpp-review` | Review | Linting + deep-analysis for Qt C++ (memory, threads, correctness, performance) |
| `qt-qml-review` | Review | QML linting (47+ rules) + analysis for bindings, layout, delegates, performance |
| `qt-qml` | Conceptual | QML best practices — bindings, scoping, modules, JS interop, types |
| `qt-ui-design` | Conceptual | UI/UX audit for Qt/QML targets |
| `qt-cmake-project` | Conceptual | Qt 6 + CMake setup — executables, QML modules, resources |
| `qt-qml-docs` | Process | Generate Markdown docs from .qml sources |
| `qt-cpp-docs` | Process | Generate Markdown docs from C++ sources |
| `qt-qml-test` | Process | Generate Qt Quick Test cases |
| `qt-qml-test-run` | Tool | Build & run qmltestrunner, parse JUnit XML |
| `qt-qml-profiler` | Tool | Run qmlprofiler, analyze hotspots |
| `qt-figma-token-extraction` | Process | Extract Figma design tokens → QML singletons |
| `qt-figma-component-generation` | Process | Generate QML controls from Figma components |

## Repository Layout

```
cs2-map-importer/
├── src/                    # Application source (C++ & QML)
│   ├── qml/main.qml        # UI
│   ├── Main.cpp            # Entry point
│   ├── Ui.h/.cpp           # Backend controller
│   └── *.h/.cpp            # Workflow modules
├── icons/                  # App icon & .rc
├── skills/                 # Agent skills (read-only reference)
├── CMakeLists.txt          # Build definition
├── CMakePresets.json       # Build presets
└── *.txt                   # External tool usage docs
```

## Do NOT

- Do NOT use `qt5_*` CMake macros or `Qt5::` targets.
- Do NOT use `add_executable` — use `qt_add_executable`.
- Do NOT add QML files via `qt_add_resources` — use `qt_add_qml_module`.
- Do NOT create additional QML module URIs without updating CMake.
- Do NOT use STL containers where Qt equivalents exist (prefer `QString` over `std::string`).

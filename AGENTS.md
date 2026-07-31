## Skills

| Skill | Type | Description |
|-------|------|-------------|
| `qt-cpp-review` | Review | Deterministic linting + 6 parallel deep-analysis agents for Qt C++ code. Covers model rule compliance, memory ownership, thread safety, correctness, error handling, and performance. |
| `qt-qml-review` | Review | Deterministic QML linting (47+ rules) + parallel deep-analysis agents for bindings, layout, loaders, delegates, states, and performance. |
| `qt-qml` | Conceptual | QML best practices for writing, reviewing, fixing, and refactoring. Corrects systematic LLM pre-training biases around bindings, scoping, modules, JS interop, and types. |
| `qt-ui-design` | Conceptual | UI design and audit for Qt/QML, web, and embedded (MPU/MCU) targets. Covers screen layout, navigation, and UX review with platform-aware defaults for geometry, viewing distance, input, and locale. |
| `qt-qml-docs` | Process | Generates Markdown reference documentation for QML components and applications from .qml source files. |
| `qt-cpp-docs` | Process | Generates Markdown reference documentation for Qt/C++ source files — classes, modules, utilities, headers, and entry points. |
| `qt-qml-profiler` | Tool | Runs `qmlprofiler` on a 2D QML / Qt Quick application, parses the `.qtd` trace, and analyzes hotspots against the source code with frame-time, memory, and pixmap-cache summaries. Does not cover Qt Quick 3D. |
| `qt-qml-test` | Process | Generates Qt Quick Test cases (`tst_*.qml`) for QML components using `TestCase`, `SignalSpy`, and `tryCompare`. Handles single files and batches. Does not cover CMake or runner setup. |
| `qt-qml-test-run` | Tool | Builds and runs Qt Quick Test (`qmltestrunner`) tests for a CMake project, parses the JUnit XML, and writes a Markdown report. Opt-in CMake test-infrastructure wiring with `--wire-up`. Companion to `qt-qml-test`. |
| `qt-figma-token-extraction` | Process | Extracts design tokens, text styles, and variables from a Figma design system and produces a design-tokens.json plus ready-to-use QML singletons. |
| `qt-figma-component-generation` | Process | Extracts component metadata from a Figma design system and generates production-ready QML controls mapped to Qt Quick Controls 2 patterns. Requires tokens from `qt-figma-token-extraction`. |
| `qt-cmake-project` | Conceptual | Sets up and manages Qt 6 projects built with CMake — fresh projects, executables, libraries, QML modules, plugins, folder layout, and static resources. Corrects systematic LLM biases around qmake-isms and the legacy `qt5_*` macros. |

### Skill types

- **Review** — structured code review workflows combining
  deterministic linters with deep AI analysis
- **Process** — workflows and decision frameworks
  (architecture, build, test, documentation)
- **Conceptual** — mental model corrections for areas where
  LLMs consistently fail (declarative QML, C++/QML boundary,
  Widgets patterns, UI design)
- **Tool** — guidance on Qt CLI tools and testing solutions

## Repository Structure

```
skills/                           # All skills live here
  qt-cpp-review/                  #   Each skill is a directory
    SKILL.md                      #   with a SKILL.md entry point
    references/                   #   and optional reference docs
      lint-scripts/
      qt-review-checklist.md
    platforms/                    #   Platform-specific variants
  qt-qml-review/
  qt-qml/
  qt-ui-design/
  qt-qml-docs/
  qt-cpp-docs/
  qt-qml-profiler/
  qt-qml-test/
  qt-qml-test-run/
  qt-cmake-project/
```

# CS2 Map Importer

A modern, high-performance Windows desktop application with a Graphical User Interface (GUI) to import maps and assets (models, particle systems, materials, soundscapes) from Source 1 games into Counter-Strike 2 (Source 2). The tools provided by Valve for importing assets aren't very user-friendly, so this program was created to streamline and automate the entire pipeline.

This project was previously a Python script (forked from sarim's importer) and has now been completely re-engineered into a clean, strictly layered C++20 application featuring a reactive QML-based UI (Qt Quick Controls 2, Fusion style).

## Usage

1. Launch `cs2importer`.
2. Select your **Counter-Strike 2** folder and your target **Source 1 Game** folder.
3. Switch between tabs depending on the import task:
   - **Map:** Select a **VMF** or **BSP** file, specify the destination Addon Name, and configure launch options.
   - **Model:** Convert and import Source 1 `.mdl` assets.
   - **Particle:** Select a Source 1 `.pcf` file, choose blend/diffuse options, and import into your CS2 addon.
4. Click **START** to begin the import workflow. The real-time task log panel will display stage progress and technical diagnostics.

## Build Requirements

To build this program, you will need:
- **Operating System:** Windows 10/11 (64-bit) — *This application is exclusively designed for Windows*
- **Compiler:** **C++20** compatible compiler (MSVC 2022 / Visual Studio 2022 recommended)
- **CMake:** Version 3.28 or higher
- **Qt6:** Version 6.8 or higher (with QML / Quick Controls 2)
- **Java Runtime (JRE / JDK):** Java 21+ or GraalVM 24 (required for BSP decompilation using BSPSRC)

## Build Instructions

Clone the repository recursively to ensure all git submodules (including `sourcepp` and dependencies) are cloned:

```bash
git clone --recursive https://github.com/LaplaceTor/cs2-map-importer.git
cd cs2-map-importer
```

If you have already cloned the repository without `--recursive`, initialize submodules manually:

```bash
git submodule update --init --recursive
```

### Building with CMake

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Or using CMake Presets:

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
```

## 3rd Party Software & Credits

### Embedded Native Libraries (via Git Submodules)
- [sourcepp](https://github.com/craftablescience/sourcepp) (`vpkpp`, `bsppp`, `vtfpp`) — Native, in-process parsing and extraction of Valve package archives (VPK, BSP pakfiles) and VTF texture decoding.
- [cryptopp](https://github.com/weidai11/cryptopp) — Cryptography and hash infrastructure, modernized for C++20 and MSVC.

### External Tools
- [bspsrc](https://github.com/ata4/bspsrc) — Decompiles Source engine BSP maps back into VMF map files.
- Valve CS2 Tools (`resourcecompiler.exe`, `source1import.exe`) — Valve's official content compiler and asset importer included with Counter-Strike 2.

# CS2 Map Importer

A user-friendly tool with a Graphical User Interface (GUI) to import maps from Source 1 (CS:GO / Counter-Strike: Source) into Counter-Strike 2 (Source 2). The tools provided by Valve for importing maps aren't very user-friendly, so this program was created to streamline the process.

This project was previously a Python program (forked from sarim's importer) but has now been fully rewritten as a standalone C++ application with a modern QML-based UI.

## Features

- **Modern GUI:** Clean, easy-to-use graphical interface built with Qt6 and QML.
- **Automatic Decompilation:** Select a `.bsp` file as input, and it will automatically decompile it using `bspsrc` (requires Java) and prepare it for import.
- **VMF Patching:** Automatically fixes `dispinfo` blocks and inserts missing required structural elements (e.g., `versioninfo`, `viewsettings`, `cordon`) in decompiled `.vmf` files.
- **VPK Signature Check Disabling:** Automatically moves and restores `vpk.signatures` during the import process.
- **Multi Source 1 Game Support:** Supports all source 1 game.
- **Import Options:** Includes options like `-usebsp`, `-usebsp_nomergeinstances` (for better geometry importing), and `-skipdeps` (to skip importing dependencies for quicker iterations).
- **Log Output:** Built-in console log output and automated log file generation.

## Requirements

To build and run this program, you will need:
- **C++17** compatible compiler
- **CMake** (version 3.10 or higher)
- **Qt6** (Core, Gui, Qml, Quick, QuickControls2 modules)
- **Java** (Required only if you intend to decompile `.bsp` files)

## Build Instructions

You can build this project using CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

On Windows, the CMake configuration will create a GUI application without a console window. On Linux/macOS, it will create a standard executable.

## Usage

1. Launch `cs2importer`.
2. Select your **Counter-Strike 2** folder.
3. Select your **Source 1** folder
4. Choose whether to import a **VMF** file or a **BSP** file.
5. Provide an Addon Name (defaults to the map name).
6. Configure any additional launch options (`-usebsp`, `-usebsp_nomergeinstances`, `-skipdeps`).
7. Click **START** to start the import process. The log output will show the progress.

## Historical Context

The original Python version of this tool was a fork from sarim's program. It added features like automatic VPK signature check disabling, validation for CS:GO and CS2, automatic BSP decompiling, and script patching to fix known issues. That Python version is no longer supported and will not receive new features. You can still download the final Python version from the [releases page](https://github.com/LaplaceTor/cs2-map-importer/releases/tag/PythonFinal). This C++ Qt version brings those features into a native, standalone graphical application.

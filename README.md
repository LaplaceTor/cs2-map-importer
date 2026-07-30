# CS2 Map Importer

A user-friendly tool with a Graphical User Interface (GUI) to import maps from Source 1 Game into Counter-Strike 2 (Source 2). The tools provided by Valve for importing maps aren't very user-friendly, so this program was created to streamline the process.

This project was previously a Python program (forked from sarim's importer) but has now been fully rewritten as a standalone C++ application with a modern QML-based UI.

## Requirements

To build and run this program, you will need:
- **C++17** compatible compiler
- **CMake** (version 3.10 or higher)
- **Qt6** (version 6.8 or higher)

## Build Instructions

You can build this project using CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Usage

1. Launch `cs2importer`.
2. Select your **Counter-Strike 2** folder.
3. Select your **Source 1** folder.
4. Choose whether to import a **VMF** file or a **BSP** file.
5. Provide an Addon Name (defaults to the map name).
6. Configure any additional launch options.
7. Click **START** to start the import process. The log output will show the progress.

## 3rd party software using in this project

- [VPKEdit](https://github.com/craftablescience/VPKEdit) for extract files from vpk and bsp
- [VTFEdit-Reload](https://github.com/Sky-rym/VTFEdit-Reloaded) for extract vtf to normal image format
- [bspsrc](https://github.com/ata4/bspsrc) for decompile bsp to vmf


## Historical Context

The original Python version of this tool was a fork from sarim's program. It added features like automatic VPK signature check disabling, validation for CS:GO and CS2, automatic BSP decompiling, and script patching to fix known issues. That Python version is no longer supported and will not receive new features. You can still download the final Python version from the [releases page](https://github.com/LaplaceTor/cs2-map-importer/releases/tag/PythonFinal). This C++ Qt version brings those features into a native, standalone graphical application.

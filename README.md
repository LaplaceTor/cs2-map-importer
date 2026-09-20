# CS2 Map Importer

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/R5S426JG8P)
[![爱发电](https://img.shields.io/badge/爱发电-赞助支持-946ce6?style=for-the-badge&logo=afdian&logoColor=white)](https://afdian.com/a/laplacetor)

A user-friendly tool with a Graphical User Interface (GUI) to import maps from Source 1 Game into Counter-Strike 2 (Source 2). The tools provided by Valve for importing maps aren't very user-friendly, so this program was created to streamline the process.

This project was previously a Python program (forked from sarim's importer) but has now been fully rewritten as a standalone C++ application with a modern QML-based UI.

## Usage

1. Launch `cs2importer`.
2. Select your **Counter-Strike 2** folder.
3. Select your **Source 1** folder.
4. Choose whether to import a **VMF** file or a **BSP** file, and make sure all dependencies inside your **Source 1** folder if you choose a **BSP** file.
5. Provide an Addon Name (defaults to the map name).
6. Configure any additional launch options.
7. Click **START** to start the import process. The log output will show the progress.

## 3rd party software using in this project

- [VPKEdit](https://github.com/craftablescience/VPKEdit) for extract files from vpk and bsp
- [VTFEdit-Reload](https://github.com/Sky-rym/VTFEdit-Reloaded) for extract vtf to normal image format
- [bspsrc](https://github.com/ata4/bspsrc) for decompile bsp to vmf

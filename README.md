# CS2 Map Importer

A user-friendly tool with a Graphical User Interface (GUI) to import maps from Source 1 Game into Counter-Strike 2 (Source 2). The tools provided by Valve for importing maps aren't very user-friendly, so this program was created to streamline the process.

This project was previously a Python program (forked from sarim's importer) but has now been fully rewritten as a standalone C++ application with a modern QML-based UI.

## Features & Import Pipeline

When importing a map, the tool automatically performs a highly detailed and comprehensive sequence of extraction, patching, conversion, and compilation steps. Below is a detailed breakdown of the complete import pipeline and its automatic features:

### 1. BSP Decompilation & Embedded File Extraction
If a `.bsp` file is selected as input instead of a `.vmf` file, the tool performs the following:
- **Automatic Decompilation:** Automatically runs `bspsrc.jar` using Java to decompile the `.bsp` file into a clean `.vmf` file.
- **Embedded File Extraction:** Uses `vpkeditcli.exe` to unpack and extract any embedded assets (materials, models, particles) from the BSP file into the temporary map folder.
- **Auto-Moving Assets:** Automatically copies extracted materials, models, and particles to the specified Source 1 game directory so they are ready for the import tools.

### 2. VMF Structure Patching & Fixes
The decompiled or provided `.vmf` file is heavily patched to resolve structural issues and prevent Source 2 compiler errors:
- **Required Blocks Insertion:** Automatically checks for and inserts missing structural block elements:
  - `versioninfo` block (using the parsed mapversion from the VMF, or defaulting to "2").
  - `viewsettings` block (sets snap-to-grid, grid spacing to 64, and grid visibility).
  - `cordon` block (inserts default cordon boundaries of `-1024` to `1024`).
- **Visgroups Preservation:** Safely extracts and re-inserts visgroups block elements at the correct position.
- **dispinfo Patching:** Automatically fixes `dispinfo` (displacement) blocks by inserting missing required `offsets` and `offset_normals` rows to prevent compiler crashes on corrupted displacements.

### 3. VMF Entity Corrections
To ensure the imported map functions properly in the Source 2 engine, entities are repaired and updated:
- **Special Targetnames Fix:** Scans the VMF for special trigger targetnames (`!activator`, `!self`, `!caller`) and automatically generates corresponding placeholder `info_target` entities at the coordinate origin (`0 0 0`) with incremented IDs to prevent import errors.
- **Light Color Mode Correction:** Scans for `light` and `light_spot` entities and forces the `"colormode"` key-value to `"0"` (or appends it if missing).
- **Brush Solidity Conversion:** Scans for legacy brush-based entities (`func_illusionary`, `func_wall`, `func_wall_toggle`, `func_lod`, or optionally `func_detail` if configured) and converts their classname to `func_brush`. Solidities and input filters are automatically mapped (e.g. `func_illusionary` -> Solidity "1" (never solid), `func_wall_toggle` -> Solidity "0" (toggle solidity)).
- **Render Mode & Render FX Mapping:** Maps legacy render mode and render FX integer keys to their corresponding Source 2 string enums (e.g., `renderfx` `0` -> `kRenderFxNone`, `rendermode` `5` -> `kRenderTransAdd`).
- **Dynamic Prop Animation Adjustments:** Finds `prop_dynamic` entities and replaces the legacy `DefaultAnim` key with `IdleAnim` and appends `IdleAnimationLoopMode` set to `ANIM_LOOP_MODE_LOOPING`.
- **Performance Mode Enum Updates:** Updates `PerformanceMode` properties from legacy integers to modern enums (e.g., values `0` or `2` become `PM_NORMAL`; `1` or `3` become `PM_NO_GIBS`).
- **Client-Side Sprite Corrections:** Converts legacy `env_lightglow` and `env_sprite_clientside` entities to `env_sprite`, automatically setting `clientSideEntity` to `"1"` where appropriate.
- **Physbox Multiplayer Adjustments:** Automatically converts legacy `func_physbox_multiplayer` entities to the standard `func_physbox` class.

### 4. VPK Signature Check Management
- To ensure smooth execution of Source 2 command-line import utilities, the program automatically moves the `vpk.signatures` file out of the game's directory and safely restores it after the importing process finishes.

### 5. Model Compilation & Reference Material Extraction
The tool processes model (.mdl) dependencies discovered in the map's reference text:
- **Model Import execution:** Runs Valve's `cs_mdl_import.exe` utility on every model (`.mdl`) used in the map to convert them into Source 2 `.vmdl` files.
- **Reference Material Parsing:** Reads the generated `_refs.txt` output from the model importer to discover all materials referenced by the models.
- **Dev and Tool Material Isolation:** Automatically identifies and separates legacy development or tool materials (e.g., `materials/dev/`, `materials/tools/`) to process them individually.
- **Batch Material Import & Compile:** Generates temporary file lists to batch-import materials using `source1import.exe` and compiles them using `resourcecompiler.exe`.

### 6. Advanced Material Fixes & Auto-Generation
Extracted and imported materials undergo multiple post-processing steps:
- **Skybox Stitching & Reconstruction:**
  - Locates the map's configured `skyname`.
  - Extracts the individual 6 faces of the skybox (`up`, `bk`, `rt`, `ft`, `lf`, `dn`) using `vtfcmd.exe`.
  - Resizes each face to a standardized `1024x1024` size using ImageMagick (`magick.exe`).
  - Stitches the faces together into a single cubemap image (`cube.tga`).
  - Automatically writes a fresh `.vmat` file using the `sky.vfx` shader targeting the newly stitched skybox texture.
- **Key-Value Property Recovery (Missing KV Fix):** Automatically recovers material properties from original VMT assets. If `$translucent`, `$alphatest`, or `$additive` are enabled, it appends the modern counterpart flags (`F_TRANSLUCENT`, `F_ALPHA_TEST`, `F_ADDITIVE_BLEND`) to the destination `.vmat` if they were missed.
- **Conflict Resolution (Translucent vs. AlphaTest):** Resolves shader conflicts where both `F_TRANSLUCENT` and `F_ALPHA_TEST` are present. If `g_flOpacityScale` is configured, translucent is kept and alpha test is discarded; otherwise, translucent is discarded.
- **Complex Shader Variables Mapping:** Automatically translates legacy shader parameters into modern equivalents:
  - `F_VERTEX_COLOR` -> `F_PAINT_VERTEX_COLORS`
  - `F_FORCE_UV2` -> `F_SECONDARY_UV`
  - `F_SPECULAR_INDIRECT` / `F_SPECULAR_DIRECT` -> `F_ANISOTROPIC_GLOSS`
  - Correctly maps `F_BLEND_MODE` values (e.g., mode `1` -> `F_TRANSLUCENT`, mode `2` -> `F_ALPHA_TEST`, mode `4` -> `F_ADDITIVE_BLEND` & `F_TRANSLUCENT`).
- **Legacy Shader Upgrades:** Automatically upgrades legacy shaders `csgo_unlitgeneric.vfx` and `csgo_vertexlitgeneric.vfx` to the modern `csgo_complex.vfx`.
- **Color & Tint Property Mapping:** Translates legacy color variables (such as `$color2`) into modern vector variables (`g_vColorTint`), appending necessary alpha values (e.g. `1.0`).
- **Normal Map Auto-Generation:** If enabled, the tool identifies `.vmat` materials that contain a color/diffuse map but lack a normal map. It uses ImageMagick to perform a grayscale Sobel convolution algorithm to dynamically generate a normal map next to the diffuse texture, then updates the `.vmat` file to reference the generated normal map.
- **Overlay Material Adjustments:** Automatically scans all `info_overlay` entities and forces their referenced materials to use the specialized `csgo_static_overlay.vfx` shader. It remaps material layers and blend modes (translating alpha test and translucency into overlay blend modes), and generates a separate overlay material variant (`*overlay.vmat`) if required.
- **Client-Side Sprite Materials (Particles):** Automatically generates standard `.vtex` files for old sprite-based particle materials (referencing diffuse TGA files), compiling them with the `resourcecompiler.exe`.

### 7. Soundscapes & Sound Importing
- Parses and imports soundscape manifests (`soundscapes_manifest.txt`).
- Generates a compiled map sound list file (`*_sound_list.txt`) containing all referenced audio files.
- Automatically extracts sound assets from VPK archives if they are missing locally and copies the entire directory structure into the Source 2 sounds content directory.

### 8. Final Map Compilation
- Once all dependencies (models, materials, textures, normal maps, particles, sounds) are fully prepared and compiled, a final run of `source1import.exe` is executed on the updated `.vmf` file to compile the final map into CS2.

---

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

## Usage

1. Launch `cs2importer`.
2. Select your **Counter-Strike 2** folder.
3. Select your **Source 1** folder.
4. Choose whether to import a **VMF** file or a **BSP** file.
5. Provide an Addon Name (defaults to the map name).
6. Configure any additional launch options.
7. Click **START** to start the import process. The log output will show the progress.

## Historical Context

The original Python version of this tool was a fork from sarim's program. It added features like automatic VPK signature check disabling, validation for CS:GO and CS2, automatic BSP decompiling, and script patching to fix known issues. That Python version is no longer supported and will not receive new features. You can still download the final Python version from the [releases page](https://github.com/LaplaceTor/cs2-map-importer/releases/tag/PythonFinal). This C++ Qt version brings those features into a native, standalone graphical application.

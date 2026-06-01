# [CS2] Map Import Tool
The tools provided by Valve for importing maps aren't very user friendly, this tool makes it easier to use by providing an interface for it, as well as providing some ease of use features.

## Features
- Saves directories between uses, for quickly importing multiple maps in the same directory.
- Utilises Windows File Explorer so you don't need to manually copy/paste the directory paths you're using.
### Different from sarim's one
- Automatically remove .decode() in import scripts and disable vpk signature check.
- Add ability to validate csgo and cs2.
- Import maps always copy to what program in as a backup.
- Don't need to add cs2 path to your environment.
- Automatically check coloroma is install or not and install it if not.
- Automatically decompile if select bsp as input, and copy all contents in bsp out into csgo folder.
- Separate CS2 and CSGO folders.
- Import script patching to fix known issues.

## Prerequisites
While this tools makes the porting process much easier, there's still some work you need to do yourself:

- Python must be installed.
- If you select vmf as input and have any custom content in your map, make sure it's in your CS:GO's materials and models folders when you select vmf as input.

# SKGraphics

A fully configurable graphical enhancement mod for **SENRAN KAGURA SHINOVI VERSUS**.
Supports both **GOG 01.06** and **Steam 01.06** builds, including Steam's
“GOG Version” launch option, using one 32-bit Direct3D 9 proxy DLL.

> [!NOTE]
> **AI disclosure:** This mod was developed through vibe coding using GPT-6 Astra, with manual in-game testing by me. The source and test scripts are available for review and modification. Bundled SMAA is separately authored third-party work.

## Features

- Custom internal rendering resolution, including supersampling.
- FXAA-style antialiasing and SMAA 1x.
- Increased player-shadow resolutions up to 12288 and optional 3x3/5x5 filtering.
- Character outline visibility and thickness.
- Scene bloom applied before identified interface elements are drawn.
- Experimental ambient occlusion with an option to prevent characters from receiving AO.
- Texture dumping, DDS/PNG replacement and manual dump-category filtering.
- Optional loading of a separately installed 32-bit ReShade DLL.

## Installation

### Windows

1. Grab the [latest release](https://github.com/Zephyr-Lambert/SKGraphics/releases/latest).

2. Navigate to the game's installation folder.
   - For Steam: Right-click the game → **Manage → Browse local files**.  
     The path is usually `C:\Program Files (x86)\Steam\steamapps\common\Senran Kagura Shinovi Versus`.
   - For GOG Galaxy: Select the game → **Manage installation → Show folder**.  
     The path is usually `C:\Program Files (x86)\GOG Galaxy\Games\Senran Kagura Shinovi Versus`.

3. Extract the contents of the release ZIP into the game folder.  
   The folder should already contain `SKShinoviVersus.exe`.

4. Edit `SKGraphics.ini` to fit your preferences. Use the game's windowed or borderless mode for custom rendering resolution.

5. Done. Check the included `README.txt` for more information.

### Linux

1. Grab the [latest release](https://github.com/Zephyr-Lambert/SKGraphics/releases/latest).

2. Navigate to the game's installation folder.
   - For Steam: Right-click the game → **Manage → Browse local files**.
   - For Heroic Games Launcher: Open the game's page → **Options** drop-down → **Browse Files**.

3. Extract the contents of the release ZIP into the game folder.  
   The folder should already contain `SKShinoviVersus.exe`.

4. Set the launch options.
   - For Steam: Right-click the game → **Properties → General → Launch Options**.  
     Add `WINEDLLOVERRIDES="d3d9=n,b" %command%`.
   - For Heroic Games Launcher: Open the game's **Settings → Advanced → Environment Variables**.  
     Add `WINEDLLOVERRIDES` to the **NAME** field and `d3d9=n,b` to the **VALUE** field, then click **+**.

5. Edit `SKGraphics.ini` to fit your preferences. Use the game's windowed or borderless mode for custom rendering resolution.

6. Done. Check the included `README.txt` for more information.

If you already use other DLL overrides, keep them and add `d3d9=n,b` to the same semicolon-separated list.

## ReShade compatibility

> [!NOTE]
> Install ReShade before SKGraphics so its installer does not overwrite the mod's `d3d9.dll`.  
> If SKGraphics is already installed, temporarily move its `d3d9.dll` out of the game folder, then follow the steps below.

1. install ReShade normally.

2. Create a folder inside the game directory named `ReShade`.

3. Move ReShade's `d3d9.dll` from the game folder into the new `ReShade` folder.

4. Install SKGraphics using the instructions above. Its `d3d9.dll` must remain beside `SKShinoviVersus.exe`.

5. Open `SKGraphics.ini`. Under `[Compatibility]`, change `LoadReShade=0` to `LoadReShade=1`.

6. Open `ReShade.ini` and set the following values. Add missing sections if needed without duplicating existing ones:

   ```ini
   [PROXY]
   EnableProxyLibrary=0

   [INSTALL]
   BasePath=..
   ```


7. On Linux, use the following override settings:
   - **Steam launch options:** `WINEDLLOVERRIDES="d3d9=n,b;d3dcompiler_47=n,b" %command%`
   - **Heroic Games Launcher:** Set the existing `WINEDLLOVERRIDES` value to `d3d9=n,b;d3dcompiler_47=n,b`.

   The compiler override uses a native `d3dcompiler_47.dll` when installed; it does not install that runtime itself. Preserve any other DLL overrides you use.

8. Done. The game should now load both SKGraphics and ReShade.

When updating ReShade, move its newly installed `d3d9.dll` into the `ReShade` folder again and restore SKGraphics' `d3d9.dll` beside the game executable. ReShade depth effects are not guaranteed; see `README.txt` for limitations.

## Documentation

- [User guide](README.txt)
- [Configuration and option descriptions](SKGraphics.ini)
- [Resolution reference](SKGraphics_Resolutions.txt)
- [Texture category template](SKGraphics_TextureCategories.ini)
- [Changelog](CHANGELOG.md)
- [Technical notes and build profiles](docs/TECHNICAL_NOTES.md)
- [Review, testing and limitations](docs/REVIEW_NOTES.md)

## Building and testing

Build on Linux with Python 3, GCC supporting `-m32` object generation and GNU ld
supporting `i386pe` output:

```sh
sh build.sh
sh tests/run.sh
```

The build produces `d3d9.dll` in the repository root. Do not distribute
`build/unused.dll`; it is an import-library generation byproduct.
Host mock tests do not run the game or a real GPU. Optional tests requiring
your own game files are documented in the technical notes; those files are
not part of the repository.

This project was tested on Bazzite through Heroic/GE-Proton and
Steam/Proton, using an NVIDIA GeForce RTX 2060, AMD Ryzen 5 3600 (6 cores),
16 GB DDR4 RAM, and a 2560 × 1440 display. Native Windows 11 testing was
also performed using integrated AMD Radeon Graphics, an AMD Ryzen 7 5825U
(8 cores), 16 GB DDR4 RAM, and a 1920 × 1080 display. Other hardware and
runner combinations have not been verified. AO and outline transparency
remain experimental.

## Development and license

Original SKGraphics code uses [0BSD](LICENSE). Credit or a link back is
appreciated but not required by that license. See [THIRD_PARTY.txt](THIRD_PARTY.txt)
for SMAA's own copyright and license. Neither license grants rights to game
assets, ReShade or other people's texture packs. This is an unofficial mod.

SKGraphics 1.0
Graphics customization for SENRAN KAGURA SHINOVI VERSUS

ABOUT
SKGraphics is a 32-bit Direct3D 9 proxy DLL configured through SKGraphics.ini.
It adds custom internal rendering resolution, FXAA-style or SMAA antialiasing,
higher-resolution and filtered player shadows, character outline controls,
optional scene bloom, experimental ambient occlusion, and texture dumping and
replacement. No replacement game executable is needed.

SUPPORTED GAME BUILDS
- GOG 01.06.
- Steam 01.06, including Steam's "GOG Version" launch option.

One DLL detects the verified executable automatically. Both editions display
01.06 in-game but have different executable layouts. Unknown or modified builds
may not pass the checks for game-specific features; a matching menu version
alone does not guarantee compatibility.

Testing was performed by the project owner on Bazzite through Heroic/GE-Proton
and Steam/Proton, using an NVIDIA GeForce RTX 2060, AMD Ryzen 5 3600 (6 cores),
16 GB DDR4 RAM, and a 2560 x 1440 display. Native Windows 11 testing was also
performed using integrated AMD Radeon Graphics, an AMD Ryzen 7 5825U (8 cores),
16 GB DDR4 RAM, and a 1920 x 1080 display. Other hardware and runner combinations
have not been verified. Not all scenes or mod combinations have been tested.

INSTALLATION
Download SKGraphics-1.0-Release.zip from:
https://github.com/Zephyr-Lambert/SKGraphics/releases/latest
The automatically generated source archives do not contain the compiled DLL.
Steam: right-click the game -> Manage -> Browse local files.
GOG Galaxy: select the game -> Manage installation -> Show folder.
Heroic: open the game's page -> Options drop-down -> Browse Files.

1. Close the game. Back up an existing mod DLL and configuration if upgrading.
2. Extract these files beside SKShinoviVersus.exe:
   d3d9.dll
   SKGraphics.ini
   SKGraphics_Resolutions.txt
   SKGraphics_TextureCategories.ini
   README.txt
   THIRD_PARTY.txt
3. Use the game's windowed or borderless mode for custom rendering resolution.
4. On Wine/Proton, ensure the game loads the local d3d9.dll:
   Steam launch options:
   WINEDLLOVERRIDES="d3d9=n,b" %command%

   Heroic: game Settings -> Advanced -> Environment Variables:
   Name: WINEDLLOVERRIDES
   Value: d3d9=n,b

   Preserve any existing overrides by adding d3d9=n,b to their semicolon-separated
   list. Do not put %command% in Heroic's environment-variable value.
5. Start the game. Restart after changing settings; there is no live INI reload.

The mod uses the game's D3DX9_43 runtime to compile shaders and handle textures.
D3DX and ReShade are not bundled. If a runtime is missing, use the game's normal
prerequisite installer in the same Windows installation or Wine/Proton prefix.
Do not overwrite another mod's d3d9.dll without resolving the loading arrangement.
The supported ReShade arrangement is described below.

UPGRADING / REMOVING
When upgrading, normally replace only d3d9.dll. Keep your customized INI,
texture categories and texture folders. The bundled INI is a preset/template.
To remove SKGraphics, remove its game-folder d3d9.dll. Configuration and texture
files can be kept as backups or removed separately. The mod does not edit the
EXE, original game assets or saves on disk. Removing SKGraphics also removes its
ReShade loading chain; reinstall ReShade normally if you want to keep using it.

CONFIGURATION AND INCLUDED PRESET
Each option in SKGraphics.ini is followed by its explanation. Numeric values
are whole numbers. The supplied preset enables:
- 3840x2160 internal rendering (when compatible with the output aspect ratio).
- FXAA-style antialiasing.
- 8192 player-shadow resolution and 5x5 shadow filtering.
It retains the normal character outlines. Bloom, AO, texture dumping/replacement,
ReShade loading and logging are off in the supplied preset.

This is an enhanced preset, not a performance guarantee. If it is too demanding,
lower rendering resolution and use 4096 shadows. A large shadow map can consume
substantial memory even though only one animated player shadow is rendered.

Options and whole sections may be omitted. Missing enhancement switches default
to original rendering; missing Logging means no log writes. Missing outline
settings retain the original outlines. Numeric tuning defaults still apply when
an enhancement is explicitly enabled. Renaming/removing the entire INI therefore
uses original-looking settings and does not load ReShade through SKGraphics.

RENDERING AND ANTIALIASING
[Rendering] Width and Height change the 3D rendering dimensions, not the desktop
resolution. The pair must have exactly the same aspect ratio as the game output.
Windowed/borderless mode without game-level multisampling is required.
Invalid requests are ignored; a failed high-resolution device creation retries
at the original output dimensions. Some later allocations can still fail if
memory is exhausted. See SKGraphics_Resolutions.txt for examples and limits.

[Antialiasing] Method=0 disables AA, 1 selects FXAA-style smoothing, and 2 selects
SMAA 1x. FXAA can look softer; SMAA may be subtler, especially with supersampling.
AA processes the final image and can soften interface elements and text.

SHADOWS AND OUTLINES
[Shadows] Enabled controls only the shadow-resolution override. Filter is
independent, so filtering can be used with the game's original shadow size.
Allowed shadow sizes: 1024, 2048 (original), 4096, 8192, 12288.
The existing animated player shadow changes; enemy circle shadows retain their
normal behavior. The mod does not create additional animated shadows.

[Outlines] supports visibility, thickness and an optional fixed RGB color.
Defaults preserve the original appearance. Experimental Opacity=0..100 can be
added manually to this section (100 is opaque/default). It does not provide
consistent transparency around every silhouette; the original outline drawing
and depth behavior are retained. The option is omitted from the shipped preset.

BLOOM AND AMBIENT OCCLUSION
Bloom adds glow to identified 3D scenes before their interface/fade/copy passes.
It has been tested across gameplay, stage animations, VN scenes, the dressing
room and the hub menu. Strong bloom can accentuate intentional lighting changes.

AO is an experimental screen-space approximation of contact shading. It uses
visible depth and can affect distant scenery/skybox geometry. MaxDistance can
limit its range; it is a distance cutoff, not automatic skybox recognition.
ExcludeCharacters=1 prevents identified actors receiving SKGraphics AO. Their
depth still contributes to AO on nearby scenery. This costs extra draws and a
full-resolution mask. Unusual models, costumes or effects may differ.
DepthView and CharacterPreview are diagnostics; leave both at 0 for normal play.

TEXTURE DUMPING AND REPLACEMENT
Enable [Textures] Dump=1 to export encountered supported original textures to:
SKGraphics_Textures/dump
The folder is beside the mod DLL. Existing dump files are not overwritten.
DumpLimit and MaxDumpMiB bound new output per launch; 0 prevents new dumping.
These are disk-output limits, not total RAM/VRAM limits. Dumping can cause loading
stutters. Disable it after collecting the textures you need.

To replace a texture:
1. Keep the dumped original as a reference and edit a copy.
2. Save your edited DDS or PNG to SKGraphics_Textures/replace, using the same
   64-character hash as the filename (with .dds or .png).
3. Set Replace=1 and restart. Dumping need not remain enabled.

DDS takes priority when both extensions exist. UseReplacementSize=0 resizes the
replacement to the original dimensions; 1 retains its dimensions. Unsupported
textures and failed replacements retain the original. The hook operates on the
identified texture loader, not every possible render target or generated image.
No replacement textures or game assets are included in this download.

Manual dump categories:
Add the original 64-character hashes beneath sections you create in
SKGraphics_TextureCategories.ini. Use bare hashes without angle brackets.
Extensions and '=description' labels are optional. Category names may contain
spaces, such as [Main Menu], but not commas. Restart after editing.
DumpCategoryMode=Include dumps only selected categories; Exclude omits them.
DumpCategories is a comma-separated list. DumpUncategorized independently allows
or blocks hashes absent from every category, including in All mode.
Categories filter dumping only; they do not limit replacement loading.

RESHADE
SKGraphics supports a separately installed 32-bit ReShade DLL. The tested chain
is game -> SKGraphics -> ReShade -> system D3D9 / Proton DXVK.

With the game closed:
1. Install ReShade first for SKShinoviVersus.exe, selecting DirectX 9.
2. Create a ReShade subfolder and move ReShade's 32-bit d3d9.dll into it.
3. Install SKGraphics. Its d3d9.dll must stay beside SKShinoviVersus.exe.
4. Set [Compatibility] LoadReShade=1 in SKGraphics.ini.
5. Keep ReShade.ini beside the game EXE. In its corresponding sections, set:

   [PROXY]
   EnableProxyLibrary=0

   [INSTALL]
   BasePath=..

Add missing sections if needed; do not create duplicate sections. Remove an old ProxyLibrary=SKGraphics.dll
entry if present. The previous reverse proxy arrangement is obsolete.
Presets and reshade-shaders stay in their existing game-folder locations.
This arrangement was tested with 32-bit ReShade 6.7.3. Other versions may differ.
On Linux with ReShade:
- Steam launch options:
  WINEDLLOVERRIDES="d3d9=n,b;d3dcompiler_47=n,b" %command%
- Heroic: set the existing WINEDLLOVERRIDES value to:
  d3d9=n,b;d3dcompiler_47=n,b
Preserve other overrides. The compiler override uses a native d3dcompiler_47.dll
when installed; it does not install that runtime. SKGraphics uses D3DX9_43.

When updating ReShade, move its newly installed d3d9.dll into the ReShade
subfolder and restore SKGraphics' d3d9.dll beside the game executable.

LoadReShade=1 requires a valid ReShade/d3d9.dll; a missing backend can prevent
startup. ReShade sees the internal rendering resolution, so its menu/cursor can
appear small after downsampling. Basic postprocessing was tested. Sharing
SKGraphics' scene depth or guaranteeing ReShade depth effects is not supported.
ReShade effects can also process the HUD; they do not inherit SKGraphics' scene
boundaries or actor exclusions. ReShade maintains its own logging/settings.

TROUBLESHOOTING
- Nothing changes: check DLL placement/override, enabled options, and whether
  you kept your intended INI. For resolution, check mode and exact aspect ratio.
- Game fails to start: back up and temporarily remove this DLL. If the problem
  is mod-related, check a conflicting d3d9.dll or invalid ReShade backend.
- Slowdowns or allocation failures: lower render/shadow resolution; disable AO
  or dumping. High dimensions are particularly demanding in a 32-bit game.
- Strange AO/depth/tints: set DepthView=0 and CharacterPreview=0; turn AO off to
  compare. Bloom and AO can be controlled separately.
- No new dumps: existing files are preserved. Check limits/category filters and
  whether a supported texture was encountered after enabling dumping.

For a useful report, set Logging=1 under [Diagnostics], restart, reproduce the
issue, and exit. Share SKGraphics.log with game edition, runner/OS, settings and
a description of the affected scene. The main log is overwritten each logged
launch, so save separate copies when comparing. Return Logging=0 afterward.
Logging off leaves old logs untouched. Leave capture probes off unless needed.
The source repository includes technical and review notes for deeper debugging.

DEVELOPMENT AND SOURCE
The custom injector and shaders were developed through vibe coding using GPT-6 Astra, with manual in-game testing by the project owner.
Source and build/test scripts are provided for inspection and modification.
Bundled SMAA is third-party work, not AI-generated project code. Automated tests
and this testing history do not constitute an independent security audit.

LICENSE AND ATTRIBUTION
The original SKGraphics code is offered under the BSD Zero Clause License
(0BSD), reproduced below. Credit or a link back is appreciated, not required
by that license. Bundled SMAA remains under its own license in THIRD_PARTY.txt.
The project license does not cover the game, replacement packs or ReShade.
This is an unofficial mod and is not affiliated with the game's developers.

BSD Zero Clause License
Copyright (C) 2026 SKGraphics contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

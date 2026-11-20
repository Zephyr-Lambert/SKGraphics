SKGraphics 2.0
Graphics customization for SENRAN KAGURA SHINOVI VERSUS

ABOUT
SKGraphics is a 32-bit Direct3D 9 proxy DLL configured through SKGraphics.ini
or its in-game settings menu (F9). It adds custom internal rendering
resolution, FXAA-style or SMAA antialiasing, higher-resolution and softened
shadows (including animated shadows for CPU characters and crowd enemies),
character outline controls, scene bloom, experimental ambient occlusion,
movie volume control, presets, and texture dumping and replacement.
No replacement game executable is needed.

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
Download SKGraphics-2.0-Release.zip from:
https://github.com/Zephyr-Lambert/SKGraphics/releases/latest
The automatically generated source archives do not contain the compiled DLL.
Steam: right-click the game -> Manage -> Browse local files.
GOG Galaxy: select the game -> Manage installation -> Show folder.
Default Galaxy path: C:\Program Files (x86)\GOG Galaxy\Games\Senran Kagura Shinovi Versus
Heroic: open the game's page -> Options drop-down -> Browse Files.

1. Close the game. Back up an existing mod DLL and configuration if upgrading.
2. Extract these files beside SKShinoviVersus.exe:
   d3d9.dll
   SKGraphics.ini
   SKGraphics_Presets.ini
   SKGraphics_Resolutions.txt
   SKGraphics_TextureCategories.ini
   README.txt
   THIRD_PARTY.txt
3. Use the game's borderless window mode for custom rendering resolution
   (fullscreen is not supported; see DISPLAY MODES below).
4. On Wine/Proton, ensure the game loads the local d3d9.dll:
   Steam launch options:
   WINEDLLOVERRIDES="d3d9=n,b" %command%

   Heroic: game Settings -> Advanced -> Environment Variables:
   Name: WINEDLLOVERRIDES
   Value: d3d9=n,b

   Preserve any existing overrides by adding d3d9=n,b to their semicolon-separated
   list. Do not put %command% in Heroic's environment-variable value.
5. Start the game. Press F9 to open the settings menu (see IN-GAME MENU).

The mod uses the game's D3DX9_43 runtime to compile shaders and handle textures.
D3DX and ReShade are not bundled. If a runtime is missing, use the game's normal
prerequisite installer in the same Windows installation or Wine/Proton prefix.
Do not overwrite another mod's d3d9.dll without resolving the loading arrangement.
The supported ReShade arrangement is described below.

UPGRADING / REMOVING
When upgrading from 1.x, replace d3d9.dll and add SKGraphics_Presets.ini.
Using the new SKGraphics.ini is recommended: 2.0 adds many settings, and the
new file explains each one. Copy over any values you want to keep from your old INI. Keep your texture categories and
texture folders.
To remove SKGraphics, remove its game-folder d3d9.dll. Configuration and texture
files can be kept as backups or removed separately. The mod does not edit the
EXE, original game assets or saves on disk. Removing SKGraphics also removes its
ReShade loading chain; reinstall ReShade normally if you want to keep using it.

IN-GAME MENU
Press F9 (change with [Compatibility] MenuKey) to open or close the menu.
1 / 2 select a setting, - / = change it (hold to repeat).

With a controller: hold LB + RB + Back + Start together to open or close it
(L1 + R1 + Share + Options on PlayStation-style controllers, L + R + - + + on
Switch-style). D-pad up/down select, left/right change (hold to repeat).
This works with Xbox-style (XInput) controllers, controllers mapped to them
by Steam Input or similar tools, and controllers Windows sees as joysticks,
such as PlayStation controllers.
On Windows, Switch controllers need Steam Input (or a similar tool); the game
itself does not support them otherwise.
[Compatibility] MenuController=0 turns controller control off.

The game still receives every key and button; nothing is blocked. Because
Start is part of the combination, the game also sees Start being pressed.
Live settings apply straight away. Settings marked "(restart)" are saved and
apply the next time the game starts. "Save to SKGraphics.ini" writes your
changes to the INI (its comments are kept); "Discard changes" reloads the INI.
Unsaved changes are lost when the game closes. Some settings, such as
rendering resolution, texture options and diagnostics, are INI-only.

Bloom and AO can only be switched on live if they were on when the game
started; otherwise they show "(restart)".

Movie volume changes apply from the next video played.

PRESETS
The menu's Presets section applies one of four presets:
- SKGraphics Lite (2560 x 1440)
- SKGraphics Standard (3840 x 2160)
- SKGraphics High (5120 x 2880)
- SKGraphics Ultra (7680 x 4320)
Applying a preset writes its values to SKGraphics.ini. Live settings change
straight away; resolution, shadow size and the rest apply after a restart.
Presets leave movie volume, texture, ReShade and diagnostic settings alone.
They also do not switch bloom or AO on or off; they only set their values to
suit the preset, so the effects look right whenever they are on.

Higher presets need a powerful GPU and much more video memory. High and Ultra
use 16384 shadow maps (about 2 GB of video memory for shadows alone), and Ultra
renders at 7680 x 4320. Ultra is very demanding.

Presets are defined in SKGraphics_Presets.ini, which can be edited or extended
(up to 8 presets). Their resolutions are 16:9; on other screen shapes they are
fitted automatically (see DISPLAY MODES).

CONFIGURATION AND INCLUDED SETTINGS
Each option in SKGraphics.ini is followed by its explanation. Numeric values
are whole numbers. The supplied INI matches the Standard preset:
- 3840x2160 internal rendering.
- FXAA-style antialiasing.
- 12288 player-shadow resolution with 5x5 shadow softening.
- Animated shadows for CPU characters and crowd enemies, 3x wider shadow area.
It retains the normal character outlines. Bloom and AO are off (pre-tuned).
Movie volume control, texture dumping/replacement, ReShade loading, live INI
reload and logging are off.

This is an enhanced setup, not a performance guarantee. If it is too demanding,
apply the Lite preset or lower the rendering resolution and shadow size.

Options and whole sections may be omitted. Missing enhancement switches default
to original rendering; missing Logging means no log writes. Missing outline
settings retain the original outlines. Numeric tuning defaults still apply when
an enhancement is explicitly enabled. Renaming/removing the entire INI therefore
uses original-looking settings and does not load ReShade through SKGraphics.

RENDERING AND ANTIALIASING
[Rendering] Width and Height change the 3D rendering dimensions, not the desktop
resolution. Use a 16:9 size (the game always draws a 16:9 picture).
Borderless window or windowed mode without game-level multisampling is required.

DISPLAY MODES
- Borderless window: recommended. The game fills the screen and the rendering
  resolution applies.
- Windowed: the rendering resolution applies, but the game stays in a window
  unless something else scales it (the Steam Deck does).
- Fullscreen: not supported. In exclusive fullscreen the game's image must
  match the display mode exactly, so the mod leaves the resolution unchanged.
On a screen that is not 16:9 (Steam Deck, 16:10 laptops, ultrawide monitors)
the game adds black bars around its 16:9 picture. The mod then enlarges the
rendering size on one axis to cover the bars, so the picture keeps the chosen
resolution: 3840x2160 becomes 3840x2400 on the Steam Deck and 5160x2160 on a
3440x1440 ultrawide. The log records this ("Rendering size fitted to output
shape"). If rendering resolution does not take effect, SKGraphics.log names
the reason (for example "unsupported mode" in fullscreen).

Invalid requests are ignored; a failed high-resolution device creation retries
at the original output dimensions. Some later allocations can still fail if
memory is exhausted. See SKGraphics_Resolutions.txt for examples and limits.

[Antialiasing] Method=0 disables AA, 1 selects FXAA-style smoothing (recommended),
and 2 selects SMAA 1x. SMAA is experimental: its effect is subtle in this game,
especially with supersampling. FXAA can look softer.
AA processes the final image and can soften interface elements and text.

SHADOWS AND OUTLINES
[Shadows] Enabled controls only the shadow-resolution override. Filter is
independent, so filtering can be used with the game's original shadow size.
Allowed shadow sizes: 1024, 2048 (original), 4096, 8192, 12288, 16384.
Larger sizes need much more video memory (16384: about 2 GB; 12288: 1.1 GB).
FilterRadius sets the softening width in shadow-map pixels, so a larger map
needs a smaller radius for the same look (the presets do this).

ActorShadows=1 also gives CPU-controlled characters animated shadows, and 2
adds crowd enemies. Their circle shadows are hidden while a real shadow is
drawn and return when a character is outside the shadow area. Coverage=1 or 2
makes the shadow area 2x or 3x wider, so shadows reach further from the player.
Crowd shadows cost the most performance.

[Outlines] supports visibility, thickness and an optional fixed RGB color.
Defaults preserve the original appearance. Experimental Opacity=0..100 (100 is
opaque/default) does not provide consistent transparency around every
silhouette; the original outline drawing and depth behavior are retained.
Opacity is INI-only.

BLOOM AND AMBIENT OCCLUSION
Bloom adds glow to identified 3D scenes before their interface/fade/copy passes.
Composite=2 (tone-mapped) keeps pixels without glow unchanged and lets
highlights roll off instead of washing out; Brightness=1 (luminance) makes
vivid but not truly bright colors glow less. The original 1.x look is
Composite=0, Brightness=0, which can over-brighten scenes.
It has been tested across gameplay, stage animations, VN scenes, the dressing
room and the hub menu. Strong bloom can accentuate intentional lighting changes.

AO is an experimental screen-space approximation of contact shading. It is
very demanding (by far the most expensive effect) and probably not worth it on
most systems. It uses
visible depth and can affect distant scenery/skybox geometry. MaxDistance can
limit its range; it is a distance cutoff, not automatic skybox recognition.
ExcludeTransformationBackgrounds=1 keeps AO off the identified transformation
backdrops. ExcludeCharacters=1 prevents identified actors receiving SKGraphics AO. Their
depth still contributes to AO on nearby scenery. This costs extra draws and a
full-resolution mask. Unusual models, costumes or effects may differ.
DepthView and CharacterPreview are diagnostics; leave both at 0 for normal play.

MOVIE VOLUME
[Movies] Enabled=1 lets Volume (0-100, 0 = mute) scale the volume of video
playback without changing other game audio. GOG and Steam 01.06. Volume
changes apply from the next video played. Off by default.

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
  you kept your intended INI. For resolution, check the display mode and the log.
- Game fails to start: back up and temporarily remove this DLL. If the problem
  is mod-related, check a conflicting d3d9.dll or invalid ReShade backend.
- Slowdowns or allocation failures: apply a lower preset, or lower render/shadow
  resolution; set ActorShadows lower; disable AO or dumping. High dimensions are
  particularly demanding in a 32-bit game.
- Menu does not open: check [Compatibility] Menu=1 and MenuKey; the game window
  must have focus. Another program may use the same function key.
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
The custom injector and shaders were developed through vibe coding using GPT-6 Astra
(1.x) and Claude (2.0), with manual in-game testing by the project owner.
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

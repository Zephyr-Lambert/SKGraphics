# Changelog

## 2.0 (2026-11-20)

### New

- **In-game settings menu** (F9 by default; `[Compatibility] MenuKey` to
  change). Most settings can be changed while playing: live ones apply
  straight away, the rest are marked "(restart)". Changes can be saved to
  `SKGraphics.ini` or discarded. Controllers work too: LB + RB + Back +
  Start opens it, the D-pad selects and changes settings
  (`[Compatibility] MenuController=0` to turn off). The game still receives
  every key and button.
- **Presets:** Lite (2560 x 1440), Standard (3840 x 2160), High (5120 x 2880)
  and Ultra (7680 x 4320), applied from the menu. Defined in
  `SKGraphics_Presets.ini` (up to 8; editable). The supplied `SKGraphics.ini`
  uses the Standard values.
- **Animated shadows for more characters:** `[Shadows] ActorShadows=1` adds
  real shadows for CPU-controlled characters, 2 also for crowd enemies. Their
  circle shadows are hidden when replaced.
- **Larger shadow area:** `[Shadows] Coverage=1/2` (2x / 3x wider).
- **16384 shadow map size** (1.0 went up to 12288).
- **Movie volume** (`[Movies]`, GOG and Steam): lower or mute video playback
  without changing other game audio. Off by default.
- **Bloom blend and brightness options:** tone-mapped blending (highlights
  roll off instead of washing out; untouched pixels stay the same) and a
  luminance brightness measure.
- **AO:** option to exclude the transformation backgrounds.
- **Optional live INI reload** (`[Compatibility] LiveReload=1`, off by
  default) for people who prefer editing the INI while the game runs.

- **Any screen shape:** on screens that are not 16:9 (Steam Deck, 16:10
  laptops, ultrawide monitors), the rendering resolution now works in
  borderless mode. The game letterboxes its 16:9 picture there; the mod
  enlarges the rendering size to cover the bars (3840x2160 becomes 3840x2400
  on the Deck), so the picture keeps the chosen resolution. Before, the
  resolution was left unchanged unless the shapes matched exactly.

### Fixed

- Bloom made scenes flash blue/green/yellow/red on some native Windows drivers
  (seen on AMD; present since 1.0).
- Shadow softening was rejected by native Windows D3D9 drivers and had no
  effect there (present since 1.0).
- SMAA looked nearly identical to no AA: one of its lookup textures was read
  with the wrong filtering. It is still a subtle effect in this game, so it
  is now labeled experimental; FXAA is recommended.

### Changed

- The supplied INI now matches the Standard preset; bloom and AO are off but
  pre-tuned, logging is off.
- Presets set bloom and AO values but never switch them on or off.
- Texture dumping saves up to 2000 new textures per launch by default (was 500).
- Debugging options are grouped at the end of the INI under a clear
  "troubleshooting only" heading.

### Technical details

- Source reformatted for readability with clang-format (`.clang-format` in the
  repository root): one statement per line, braces on every block, blank lines
  between functions. Formatting only; the rebuilt DLL is byte-identical to the
  Movie Volume Test 01 DLL apart from build timestamps and the PE checksum.
  Third-party SMAA files, HLSL shaders and generated `*_source.h` files untouched.
- Clearer names in the source: basic types (`U`/`I`/`W`/`P` became `UINT`/`INT`/
  `WCHAR`/`PVOID`), structures (`DeviceState`, `PresentParams`, `Viewport`,
  `SurfaceDesc`, `HookTable`) and generic helpers/globals (logging helpers are now
  `log_text`, `log_number`, `log_field`, `log_begin`, `log_emit` and so on).
  Code and data sections of the rebuilt DLL are identical; only the internal
  symbol-name table differs.
- Added `tests/compare_dll.py` to check that a cleanup leaves the DLL's code and
  data unchanged.

- Settings: shadow coverage moved from `[Diagnostics] ShadowCoverage` to
  `[Shadows] Coverage=0/1/2`. Retired `[Diagnostics] CPUShadowTest` and
  `CrowdShadowTest`; `[Shadows] ActorShadows=0/1/2` is the only selector.
  Their collection-only mode is kept as `[Diagnostics] ActorShadowCollectOnly=1`,
  which now also leaves circle shadows visible since no replacement is drawn.
  Debugging options stay in the normal DLL by design, for diagnosing reports
  from other hardware.
- Log: banner shows the version; resolution width/height only shown when
  the override is on; leftover test labels renamed; identical render-target
  creations, main-draw and shadow-draw samples logged once per distinct value;
  Present summaries list movie, shadow-filter and outline counters only when
  active.
- Fix: the crowd shader dump flag was reset every frame instead of once per
  device session, so the dump could repeat in the log.
- Tests: configuration keys for the new settings and the crowd dump flag.
- Bloom: new `[Bloom] Composite=0/1/2` (add, screen, tone-mapped) and
  `Brightness=0/1` (brightest channel, luminance). Tone-mapped mode copies the
  scene at full resolution, adds bloom in an expanded range and compresses it
  with 1 - exp(-x) on the brightest channel: pixels without bloom are unchanged
  and hues are preserved. Missing keys keep the original add/brightest-channel
  behavior. Tests cover pass order, blend states and constants for each mode.
- Live INI reload (`[Compatibility] LiveReload=1`; off by default in the release): the INI's write
  time and size are checked every 30 frames, and a change is applied once two
  checks agree. Live: bloom, AO amounts and exclusions, anti-aliasing, shadow
  coverage, actor shadows, movie volume. Bloom/AO can only be re-enabled live
  if on at startup; shader tracking and the INTZ depth swap now follow that
  startup availability. Restart-only keys log "Restart needed". New test.
- Log: actor shadow summary every 600 frames and only while capturing; circle
  suppression totals at most every 600 frames; bloom boundary once per type.
- In-game settings menu (`[Compatibility] Menu=1`, `MenuKey=9` for F9): live
  settings edited with 1/2 (select) and -/= (change, hold to repeat), saved to
  the INI with WritePrivateProfileStringW (comments kept) or discarded. Keys are
  read only while the game window has focus and are never blocked. Drawn at
  Present after AA with the fixed-function pipeline, an embedded public-domain
  8x8 font and full state save/restore; failures disable it until reset.
- Menu: restart-only settings (SMAA quality, shadow size/filter, outlines,
  movie volume switch, and Bloom/AO when off at startup) are editable, shown
  grayed with "(restart)", saved to the INI and applied at the next start.
  The list scrolls when it does not fit. Clearer labels. [Diagnostics],
  [Compatibility], [Textures], [Rendering] and depth debug options stay
  INI-only.
- Menu controller support through XInput (xinput1_4, 1_3 or 9_1_0, whichever
  loads): buttons from all connected pads are combined; an absent pad is
  polled again only every 120 frames. Read only while the game has focus.
- Menu controllers that are not XInput (for example PlayStation pads under
  Wine/Proton without Steam Input) are read through the winmm joystick API:
  D-pad from the hat, shoulders as buttons 5/6, Back/Start as 7/8, or
  Share/Options as 9/10 on Sony pads (vendor ID 0x054c).
- AA logs its first pass again after a live method change.
- Movie volume on Steam 01.06. The movie audio setter
  and its two call sites are now found by their byte patterns in the loaded
  game code instead of fixed GOG addresses (Steam's code is encrypted on disk
  and only readable while the game runs). Exactly one setter and exactly two
  matching call sites are required, otherwise nothing is changed; the game's
  ordinary-audio caller of the same setter is never touched. The log records
  where they were found.
- Fix: shadow softening was rejected by native Windows
  D3D9 drivers (D3DERR_INVALIDCALL, seen on AMD integrated graphics, present
  since 1.0). Two changes: each texld read r5.xyzw with only x/y written (z/w
  are now zeroed first), and the final lit/shadow blend no longer uses LRP
  with a destination overlapping its coverage source (now ADD + MAD, same
  result). DXVK validates neither. Tests check generated code for
  uninitialized temp reads and LRP; the semantic comparison against the
  original shaders still passes.
- Shadow softening: when the driver rejects the transformed shader, both the
  original and transformed code are written to the log (twice at most).
- Fix: bloom made the scene flash blue/green/yellow/red on a native Windows
  AMD driver (present since 1.0). Capturing and applying a D3D state block
  mid-frame caused it, even with nothing drawn. Bloom and AO now save and
  restore every state they change by hand. `[Diagnostics] BloomStateBlock=1`
  brings back the old method for troubleshooting.
- Fix: SMAA sampled its SearchTex lookup bilinearly. SMAA requires point
  sampling there; bilinear reads corrupt the edge-length search and made the
  effect nearly invisible. Edges, area texture and color stay bilinear.
- Menu presets from `SKGraphics_Presets.ini` (up to 8; four supplied: Lite,
  Standard, High, Ultra at 2560x1440 to 7680x4320). Applying writes the
  preset's `Section.Key=Value` lines into SKGraphics.ini; only Rendering,
  Antialiasing, Shadows, Bloom, AmbientOcclusion, Outlines and Movies may be
  changed. Live values update immediately, the rest after a restart.
- Menu: note that more settings are in SKGraphics.ini; outline opacity is
  INI-only again.
- Live reload: "Restart needed" log lines formatted as `setting=Section.Key`;
  changing shadow selectors no longer repeats the crowd shader dump.

## 1.0.1 — 2026-10-25

- Clarifies that bloom is experimental: it can over-brighten scenes and wash out
  colors. Bloom remains disabled in the supplied preset.
- Documentation-only update. Rendering behavior and active preset values are
  unchanged. Includes the same DLL as 1.0, which still reports 1.0 in logs.

## 1.0 — 2026-10-13

- First public release packaging of the tested feature set.
- Supports verified GOG 01.06 and Steam 01.06 executables in one DLL, including
  Steam's "GOG Version" launch option.
- Custom rendering resolution; FXAA-style AA and SMAA 1x.
- Player-shadow resolution through 12288, plus 3x3/5x5 shadow filtering.
- Character outline visibility, width and fixed color; experimental opacity.
- Pre-UI scene bloom and experimental AO with optional actor receiver exclusion.
- Texture DDS dumping, DDS/PNG replacement and manual dump-category filtering.
- Optional ReShade loading through the tested reverse chain.
- Logging off and original-looking fallback settings when INI entries are absent;
  a separately chosen enhanced preset is included.
- Current documentation consolidated; obsolete trial instructions omitted from
  the public packages. Original code under 0BSD; SMAA keeps its own notice.
- Rendering implementation unchanged from 0.10.13; only the DLL version banner
  changes for 1.0. Documentation/template comments do not alter configuration.

## Pre-release milestones

- **0.10.13:** game version labels changed to GOG 01.06 and Steam 01.06.
- **0.10.12:** verified per-build addresses, Steam texture import and scene
  boundaries; tested by the project owner on all three launch paths.
- **0.10.11:** temporary Steam runtime code diagnostic; not shipped in 1.0.
- **0.10.10:** pre-release review, protection rollback, quieter disabled logging,
  shared D3DX loading, and failure/path cleanup.
- **0.10.9:** opt-in defaults and logging; followed by approved enhanced INI preset
  and resolution reference documentation.
- **0.10.8:** optional 12288 shadow-map size with capability/allocation checks.
- **0.10.6–0.10.7:** actor classification preview, then AO receiver exclusion.
- **0.10.4–0.10.5:** ReShade coexistence investigation and corrected loading order.
- **0.10–0.10.3:** AO implementation, compiler compatibility, independent failure
  handling, and fixes for unwanted surface striping.
- **0.9–0.9.5:** bloom and scene-boundary fixes for HUD-free scenes, VN transitions,
  dressing room and hub menu; depth investigation and visualization.
- **0.8 series:** outline controls, followed by color and experimental opacity.
- **0.7 series:** identified shadow-receiver filtering and shader-read fixes.
- **Earlier prototypes:** custom rendering, FXAA/SMAA, shadow resolution, texture
  dumping/replacement and manual texture categories.

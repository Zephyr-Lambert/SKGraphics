# Changelog

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

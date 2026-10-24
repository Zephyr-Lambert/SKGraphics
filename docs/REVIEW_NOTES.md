# SKGraphics 1.0 review and validation

The 0.10.10 review covered the custom C proxy, hooks, in-memory patches,
shader transforms, textures/categories, postprocess state cleanup, paths,
configuration and logging. Later Steam profile changes received targeted
real-byte and mock tests. This is not an independent security audit, and does
not audit SMAA, Windows/Wine, DXVK, D3DX or ReShade.

## Changes made during review

- Disabled logging avoids diagnostic surface queries, signature bookkeeping and
  viewport sampling once no log is open or its budget is exhausted.
- Failed vtable page-protection restoration rolls back the pointer and retries
  protection restoration, without marking the hook installed.
- D3DX is loaded once successfully and shared across effects and texture work.
- Failed FXAA compilation releases partial bytecode; string logging accepts null.
- Near-limit paths are rejected before configuration/backend suffixes are added.

## Existing safeguards

- No network client, download/update feature, shell execution or elevation code.
- Executable patches require exact GOG/Steam build identity and instruction guards;
  texture imports also require the resolved original export. Unknown builds skip
  the game-specific hooks. Shadow overrides preflight caps and both allocations.
- Texture output paths use SHA-256 names. Categories never become paths. Existing
  dumps are preserved; incomplete writes are deleted. Output counts/bytes are bounded.
- Shader transforms require known hashes or validated bounded input; failed
  transforms retain the original. Effects preserve state and have failure paths.
- Missing enhancement/logging options do not enable them. Supplied preset differs
  intentionally from missing-key fallback behavior.

## Limits and trust boundaries

This is native code with the game's permissions. External DLLs and replacement
textures must be trusted; D3DX decoding is not sandboxed. Its dependency resolution
uses the existing Windows/Wine search behavior. Very large images, render sizes
and shadows can exhaust VRAM or 32-bit process resources. Successful preflight
does not reserve resources against later allocations.

Cached COM objects may retain device references until reset/recreation/exit.
ReShade can report lifetime warnings at exit. The intended path is this game's
primary device/render thread, not arbitrary multidevice or concurrent rendering.
Shared scratch arrays/fixed tables and non-atomic multi-slot hook installation
remain limitations. Repeated OS protection failures cannot always be repaired.

AO and outline opacity are experimental. Testing covers the systems listed
below, not every GPU/runner, costume, scene, binary or mod combination. Shader parser fuzzing does not test D3DX image decoding or
prove absence of vulnerabilities. No unsupported performance improvement is claimed.

## Evidence and validation

- 0.10.12: 17 host mock suites and 17 SHA-256 vectors passed. Actual GOG executable
  and Steam loaded-code tests verified detection, all permitted shadow sizes,
  operand-only changes, protection rollback and rejection of altered guards.
  Protected Steam disk code was rejected as intended.
- 0.10.10: 20,000 deterministic malformed shader cases passed ASan/UBSan.
  LeakSanitizer was disabled because the runner could not inspect task state;
  this was not a leak-test result.
- Project-owner 0.10.12 tests: standalone GOG, ordinary Steam, and Steam's
  "GOG Version" worked visually. Logs confirmed 3840x2160, FXAA, 8192 filtered
  shadows, AO with actor exclusion, bloom and a replacement in all three runs.
  Ordinary Steam also produced new texture dumps. These runs used default
  outlines; earlier custom-outline tests were on the standalone GOG build.
- 0.10.13 changed only names/comments. 1.0 changes the version banner, documentation
  and packaging without changing the rendering logic or active INI values.
- 1.0: rebuilt successfully; all 17 host mock suites and 17 SHA-256 vectors
  passed. Source comparison confirmed only the version banner changed from
  0.10.13; active INI entries are identical. Host tests do not replace GPU tests.

- Additional project-owner testing confirmed native Windows 11 operation on an
  AMD Ryzen 7 5825U (8 cores), integrated AMD Radeon Graphics, 16 GB DDR4 RAM,
  and a 1920 x 1080 display. The Linux test system runs Bazzite with an NVIDIA
  GeForce RTX 2060, AMD Ryzen 5 3600 (6 cores), 16 GB DDR4 RAM, and a
  2560 x 1440 display, through Heroic/GE-Proton and Steam/Proton. These are
  reported test configurations, not minimum requirements or benchmark results.

## Reproduction

```sh
sh build.sh
sh tests/run.sh
gcc -O1 -g -Wno-attributes -fsanitize=address,undefined -fno-omit-frame-pointer tests/parser_sanitize.c -o build/parser_sanitize
ASAN_OPTIONS=detect_leaks=0 build/parser_sanitize
```

Optional game-byte tests are described in TECHNICAL_NOTES.md. Game files are not
included and must not be uploaded with the public repository.

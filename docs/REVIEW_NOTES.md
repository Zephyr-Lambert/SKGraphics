# SKGraphics 2.0 review and validation

Scope: the custom C proxy, hooks, in-memory patches, shader transforms,
textures/categories, post-process state handling, paths, configuration,
logging and the in-game menu. This is not an independent security audit, and
does not audit SMAA, Windows/Wine, DXVK, D3DX or ReShade.

## Safeguards

- No network client, download/update feature, shell execution or elevation code.
- Executable patches require exact GOG/Steam build identity and instruction guards
  (movie volume: exactly one setter and two call sites found by byte pattern);
  texture imports also require the resolved original export. Unknown builds skip
  the game-specific hooks. Shadow overrides preflight caps and both allocations.
- Texture output paths use SHA-256 names. Categories never become paths. Existing
  dumps are preserved; incomplete writes are deleted. Output counts/bytes are bounded.
- Shader transforms require known hashes or validated bounded input; failed
  transforms retain the original. Effects preserve state and have failure paths.
- Missing enhancement/logging options do not enable them. The supplied INI
  differs intentionally from missing-key fallback behavior.
- The menu and presets only write keys of known sections to SKGraphics.ini;
  input is read, never blocked, and only while the game window has focus.

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

SMAA, AO and outline opacity are experimental. Bloom in add mode (Composite=0)
can over-brighten scenes and wash out colors. Testing covers the systems listed
below, not every GPU/runner, costume, scene, binary or mod combination. Shader
parser fuzzing does not test D3DX image decoding or prove absence of
vulnerabilities. No unsupported performance improvement is claimed.

## Validation

- Automated (host, no GPU): 30 mock test suites, 17 SHA-256 vectors, a
  semantic comparison of the generated shadow-softening shaders against the
  game's originals (6732 comparisons), and checks that generated shaders follow
  the native Windows validation rules. New tests were checked by deliberately
  breaking the code they cover. Earlier releases also passed 20,000 malformed
  shader cases under ASan/UBSan.
- In-game (project owner): GOG and Steam 01.06, including executables patched
  by a separate logo-skip mod; native Windows 11 and Linux (Heroic/GE-Proton,
  Steam/Proton) and Steam Deck. Covered: rendering resolution (16:9 and
  fitted 16:10), presets up to Ultra, FXAA/SMAA, shadow sizes up to 16384 with
  softening, CPU/crowd shadows and coverage, circle hiding, outlines, bloom
  modes, AO and its exclusions, movie volume, the menu with keyboard and
  Xbox/Switch/PS4 controllers, live reload and texture dumping/replacement.
- Test systems: AMD Ryzen 7 5825U with integrated Radeon graphics, 16 GB,
  1920 x 1080, Windows 11; AMD Ryzen 5 3600 with NVIDIA GeForce RTX 2060,
  16 GB, 2560 x 1440, Bazzite; Steam Deck. These are reported test
  configurations, not minimum requirements or benchmark results.

## Reproduction

```sh
sh build.sh
sh tests/run.sh
gcc -O1 -g -Wno-attributes -fsanitize=address,undefined -fno-omit-frame-pointer tests/parser_sanitize.c -o build/parser_sanitize
ASAN_OPTIONS=detect_leaks=0 build/parser_sanitize
```

Optional game-byte tests are described in TECHNICAL_NOTES.md. Game files are not
included and must not be uploaded with the public repository.

/* First texture coverage: main EXE's D3DXCreateTextureFromFileInMemoryEx import.
 * Original textures are loaded first: preserve source-info/palette outputs and
 * keep a valid fallback. Only the final returned COM texture may be replaced.
 */
#include "texture_hash.h"
typedef INT(API *TextureMemoryFn)(PVOID, const void *, UINT, UINT, UINT, UINT, UINT, UINT, UINT,
                                  UINT, UINT, UINT, PVOID, PVOID, PVOID *);
typedef INT(API *TextureFileFn)(PVOID, const WCHAR *, UINT, UINT, UINT, UINT, UINT, UINT, UINT,
                                UINT, UINT, PVOID, PVOID, PVOID *);
typedef INT(API *TextureSaveFn)(PVOID *, UINT, PVOID, PVOID);
static TextureMemoryFn texture_original;
static TextureFileFn texture_file;
static TextureSaveFn texture_save;
static WCHAR texture_root[900];
static UINT texture_root_len;
static UINT texture_lock[6], texture_ready, texture_loads, texture_dumps, texture_bytes,
    texture_replacements, texture_failures, texture_reported_limit;

static void texture_path(WCHAR *out, const char *folder, const char *hash, const char *extension) {
    UINT n = 0;
    for (; n < texture_root_len; n++) {
        out[n] = texture_root[n];
    }
    while (*folder) {
        out[n++] = (WCHAR)*folder++;
    }
    if (hash) {
        out[n++] = '\\';
        while (*hash) {
            out[n++] = (WCHAR)*hash++;
        }
        while (*extension) {
            out[n++] = (WCHAR)*extension++;
        }
    }
    out[n] = 0;
}

static void texture_note(const char *event, const char *hash, INT hr) {
    log_begin(event);
    log_text(" hash=");
    log_text(hash);
    log_hex_field("result", hr);
    log_emit();
}

static void texture_dump_original(PVOID tex, PVOID palette, const char *hash, WCHAR *name) {
    if (!texture_dump_enabled || !texture_save) {
        return;
    }
    if (!category_allows(hash)) {
        category_filtered++;
        return;
    }
    texture_path(name, "SKGraphics_Textures\\dump", hash, ".dds");
    if (GetFileAttributesW(name) != 0xffffffff) {
        return;
    }
    if (texture_dumps >= texture_dump_limit || texture_bytes >= texture_dump_mb * 1048576) {
        if (!texture_reported_limit++) {
            log_begin(
                "Texture dump session limit reached; increase DumpLimit/MaxDumpMiB or continue "
                "next launch");
            log_emit();
        }
        return;
    }
    PVOID buffer = 0;
    INT hr = texture_save(&buffer, 4, tex, palette);
    if (hr < 0 || !buffer) {
        texture_note("Texture DDS export failed", hash, hr);
        com_release(buffer);
        return;
    }
    PVOID *v = *(PVOID **)buffer;
    UINT size = ((UINT(API *)(PVOID))v[4])(buffer);
    PVOID bytes = ((PVOID(API *)(PVOID))v[3])(buffer);
    if (!size || !bytes || size > texture_dump_mb * 1048576 - texture_bytes) {
        com_release(buffer);
        return;
    }
    /* CREATE_NEW: never overwrite an existing dump. A failed partial write is
  * removed; the original texture itself is never modified. */
    PVOID file = CreateFileW(name, 0x40000000, 1, 0, 1, 0x80, 0);
    if (file != (PVOID)-1 && file) {
        UINT written = 0;
        INT ok = WriteFile(file, bytes, size, &written, 0);
        INT closed = CloseHandle(file);
        if (ok && written == size && closed) {
            texture_dumps++;
            texture_bytes += size;
            texture_note("Texture dumped DDS", hash, 0);
        } else {
            DeleteFileW(name);
            texture_note("Texture dump write failed", hash, (INT)0x80004005);
        }
    } else if (GetFileAttributesW(name) == 0xffffffff) {
        texture_note("Texture dump file creation failed", hash, (INT)0x80004005);
    }
    com_release(buffer);
}

static INT API texture_from_memory(PVOID device, const void *source, UINT bytes, UINT width,
                                   UINT height, UINT levels, UINT usage, UINT format, UINT pool,
                                   UINT filter, UINT mipfilter, UINT colorkey, PVOID info,
                                   PVOID palette, PVOID *out) {
    INT hr = texture_original(device, source, bytes, width, height, levels, usage, format, pool,
                              filter, mipfilter, colorkey, info, palette, out);
    /* Render/depth/dynamic and palettized textures are outside this prototype. */
    if (hr < 0 || !out || !*out || !source || !bytes || usage || format == 40 || format == 41 ||
        !texture_ready) {
        return hr;
    }
    if (!texture_dump_enabled && !texture_replace_enabled) {
        return hr;
    }
    SurfaceDesc d = {0};
    PVOID *v = *(PVOID **)*out;
    INT described = ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(*out, 0, &d);
    if (described < 0 || d.format == 40 || d.format == 41) {
        return hr;
    }
    char hash[65];
    WCHAR name[1024];
    texture_hash(source, bytes, hash);
    EnterCriticalSection(texture_lock);
    texture_loads++;
    if (texture_loads <= 16) {
        log_begin("Texture loader observed");
        log_text(" hash=");
        log_text(hash);
        log_field("width", d.w);
        log_field("height", d.h);
        log_field("format", d.format);
        log_field("sourceBytes", bytes);
        log_emit();
    }
    texture_dump_original(*out, palette, hash, name);
    if (texture_replace_enabled && texture_file) {
        texture_path(name, "SKGraphics_Textures\\replace", hash, ".dds");
        UINT attr = GetFileAttributesW(name);
        if (attr == 0xffffffff) {
            texture_path(name, "SKGraphics_Textures\\replace", hash, ".png");
            attr = GetFileAttributesW(name);
        }
        if (attr != 0xffffffff && !(attr & 16)) {
            PVOID replacement = 0;
            /* Preserve original metadata returned to the game; DDS/PNG need no palette.
    * Same-size mode uses actual original dimensions, not D3DX default flags. */
            UINT rw = texture_own_size ? 0xfffffffe : d.w, rh = texture_own_size ? 0xfffffffe : d.h;
            INT result = texture_file(device, name, rw, rh, levels, usage, d.format, pool, filter,
                                      mipfilter, colorkey, 0, 0, &replacement);
            if (result >= 0 && replacement) {
                PVOID original = *out;
                *out = replacement;
                com_release(original);
                texture_replacements++;
                texture_note("Texture replacement loaded", hash, result);
            } else {
                com_release(replacement);
                texture_failures++;
                texture_note("Texture replacement failed; original retained", hash, result);
            }
        }
    }
    if (texture_loads == 1 || texture_loads % 250 == 0) {
        log_begin("Texture summary");
        log_field("loads", texture_loads);
        log_field("dumped", texture_dumps);
        log_field("categorySkipped", category_filtered);
        log_field("dumpBytes", texture_bytes);
        log_field("replacements", texture_replacements);
        log_field("replacementFailures", texture_failures);
        log_emit();
    }
    LeaveCriticalSection(texture_lock);
    return hr;
}

static INT texture_deferred;

static void textures_install(void) {
    if (texture_ready || (!texture_dump_enabled && !texture_replace_enabled)) {
        return;
    }
    texture_deferred = 0;
    /* This import RVA was resolved from the provided EXE's import table. Guard
  * with the build signature AND exact resolved export address before writing. */
    unsigned char *b = GetModuleHandleW(0);
    if (!shadow_identify(b)) {
        texture_deferred = 1;
        log_begin("Texture hook skipped: executable signature does not match a supported build");
        log_emit();
        return;
    }
    PVOID lib = d3dx_library();
    texture_original =
        (TextureMemoryFn)(lib ? GetProcAddress(lib, "D3DXCreateTextureFromFileInMemoryEx") : 0);
    texture_file = (TextureFileFn)(lib ? GetProcAddress(lib, "D3DXCreateTextureFromFileExW") : 0);
    texture_save = (TextureSaveFn)(lib ? GetProcAddress(lib, "D3DXSaveTextureToFileInMemory") : 0);
    PVOID *slot = (PVOID *)(b + game_build->texture_iat);
    if (!texture_original || *slot != (PVOID)texture_original) {
        log_begin("Texture hook skipped: expected D3DX import not found");
        log_emit();
        return;
    }
    UINT n = GetModuleFileNameW(self_module, texture_root, 900);
    if (!n || n >= 800) {
        log_begin("Texture hook skipped: mod directory path too long");
        log_emit();
        return;
    }
    while (n && texture_root[n - 1] != '\\' && texture_root[n - 1] != '/') {
        n--;
    }
    texture_root_len = n;
    InitializeCriticalSection(texture_lock);
    WCHAR folder[1024];
    texture_path(folder, "SKGraphics_TextureCategories.ini", 0, 0);
    category_load(folder);
    texture_path(folder, "SKGraphics_Textures", 0, 0);
    CreateDirectoryW(folder, 0);
    texture_path(folder, "SKGraphics_Textures\\dump", 0, 0);
    CreateDirectoryW(folder, 0);
    texture_path(folder, "SKGraphics_Textures\\replace", 0, 0);
    CreateDirectoryW(folder, 0);
    UINT old, unused;
    if (!VirtualProtect(slot, 4, 0x04, &old)) {
        log_begin("Texture hook skipped: import protection failed");
        log_emit();
        return;
    }
    *slot = (PVOID)texture_from_memory;
    if (!VirtualProtect(slot, 4, old, &unused)) {
        *slot = (PVOID)texture_original;
        VirtualProtect(slot, 4, old, &unused);
        log_begin("Texture hook skipped: import protection restoration failed");
        log_emit();
        return;
    }
    texture_ready = 1;
    log_begin("Texture loader hook installed");
    log_field("dump", texture_dump_enabled);
    log_field("replace", texture_replace_enabled);
    log_field("ownSize", texture_own_size);
    log_field("dumpAvailable", texture_save != 0);
    log_field("replacementAvailable", texture_file != 0);
    log_emit();
}

"""Optional real-byte profile tests. Arguments: GOG EXE, Steam EXE, Steam capture.
Build bridge with:
gcc -shared -fPIC -Os -Wno-attributes -ffreestanding -fno-builtin \
    tests/build_profile_bridge.c -o build/build-profile-test.so
No supplied game binaries/captures belong in a release package.
"""
from pathlib import Path
import ctypes as C
import struct
import sys

def u32(b, p):
    return struct.unpack_from('<I', b, p)[0]

def image(path):
    disk = Path(path).read_bytes()
    pe = u32(disk, 60)
    size = u32(disk, pe + 24 + 56)
    out = bytearray(size)
    header_size = u32(disk, pe + 24 + 60)
    out[:header_size] = disk[:header_size]
    count = struct.unpack_from('<H', disk, pe + 6)[0]
    opt_size = struct.unpack_from('<H', disk, pe + 20)[0]
    for i in range(count):
        p = pe + 24 + opt_size + i * 40
        _, rva, raw_size, offset = struct.unpack_from('<4I', disk, p + 8)
        assert rva + raw_size <= size
        out[rva:rva + raw_size] = disk[offset:offset + raw_size]
    return out

lib = C.CDLL(str((Path(__file__).resolve().parent.parent / 'build/build-profile-test.so')))
lib.check_build.argtypes = [C.c_void_p]
lib.patch_build.argtypes = [C.c_void_p, C.c_uint, C.c_uint]
lib.build_field.argtypes = [C.c_uint]
lib.build_field.restype = C.c_uint

def check(data):
    return lib.check_build((C.c_ubyte * len(data)).from_buffer(data))

gog = image(sys.argv[1])
steam = image(sys.argv[2])
assert check(steam) == 0, 'Protected disk code must not enable hooks'
capture = Path(sys.argv[3]).read_bytes()
assert capture[:8] == b'SKCODE11'
stamp, base, rva, length, size, entry = struct.unpack_from('<6I', capture, 8)
assert (stamp, base, rva, length, size) == (0x62f1e22e, 0x400000, 0x1000, 0x34f771, len(steam))
assert len(capture) == length + 32
steam[rva:rva + length] = capture[32:]

for data, build, fields in (
    (gog, 1, [0x54845, 0x1b985, 0x3463b8, 0xd6ef, 0x18a03, 0xe4bb]),
    (steam, 2, [0x1b0cb1, 0x1b0a1e, 0x351374, 0x1a40bb, 0x1ae973, 0x1a4ed1]),
):
    assert check(data) == build
    assert [lib.build_field(i) for i in range(6)] == fields
    patch = fields[0]
    original = data[:]
    ptr = (C.c_ubyte * len(data)).from_buffer(data)
    for resolution in (1024, 2048, 4096, 8192, 12288):
        assert lib.patch_build(ptr, resolution, 0)
        expected = original[:]
        for at in (patch + 1, patch + 6):
            struct.pack_into('<I', expected, at, resolution)
        assert data == expected, 'Only the two size operands may change'
        data[:] = original
    for failure in (1, 2):
        assert not lib.patch_build(ptr, 8192, failure)
        assert data == original, 'Failed protection must retain/restore original bytes'
    checks = [patch, patch + 10]
    if build == 2:
        checks += [fields[i] - 6 for i in (1, 3, 4, 5)]
    for at in checks:
        data[at] ^= 1
        assert check(data) == 0, 'Modified guard must reject the build'
        data[at] ^= 1
    assert check(data) == build
    print(f'Build {build}: real-byte profile, patch-only operands, rollback and mismatch checks passed.')

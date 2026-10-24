#!/usr/bin/env python3
"""Check the compiled C SHA-256 implementation against Python's hashlib."""
import ctypes
import hashlib
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
source = root / 'build/hash_test.c'
source.write_text('typedef unsigned int U;\n#include "../src/texture_hash.h"\nvoid hash_test(const void*p,U n,char*out){texture_hash(p,n,out);}\n')
library = root / 'build/hash_test.so'
subprocess.run(['gcc', '-shared', '-fPIC', '-O2', str(source), '-o', str(library)], check=True)
fn = ctypes.CDLL(str(library)).hash_test
fn.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p]
fn.restype = None
vectors = [b'', b'abc', b'a' * 1000000]
for size in [1, 3, 55, 56, 63, 64, 65, 119, 120, 127, 128, 129, 4096, 65537]:
    vectors.append(bytes((i * 73 + 19) % 256 for i in range(size)))
for value in vectors:
    out = ctypes.create_string_buffer(65)
    src = ctypes.create_string_buffer(value)
    fn(src, len(value), out)
    assert out.value.decode() == hashlib.sha256(value).hexdigest(), len(value)
print(f'SHA-256 passed {len(vectors)} independent reference vectors.')

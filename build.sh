#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
python3 - <<'PYGEN'
from pathlib import Path
import json
ao=Path("src/ao.hlsl").read_text()
Path("src/ao_source.h").write_text("/* Generated from ao.hlsl. */\nstatic const char ao_source[] =\n"+"\n".join(json.dumps(line+"\n") for line in ao.splitlines())+";\n")
bloom=Path("src/bloom.hlsl").read_text()
Path("src/bloom_source.h").write_text("/* Generated from bloom.hlsl. */\nstatic const char bloom_source[] =\n"+"\n".join(json.dumps(line+"\n") for line in bloom.splitlines())+";\n")
lines=Path('src/fxaa.hlsl').read_text().splitlines()
Path('src/fxaa_source.h').write_text('/* Generated from fxaa.hlsl. */\nstatic const char aa_source[] =\n'+'\n'.join(json.dumps(line+'\n') for line in lines)+';\n')
source=Path('src/smaa_wrapper.hlsl').read_text().replace('// @SMAA_REFERENCE@',Path('src/smaa/SMAA.hlsl').read_text(encoding='latin1'))
Path('src/smaa_source.h').write_text('/* Generated reference SMAA + integration wrapper. */\nstatic const char smaa_source[] =\n'+'\n'.join(json.dumps(line+'\n') for line in source.splitlines())+';\n')
PYGEN
# Produce only an import library. unused.dll is NOT shipped or loaded.
{
 echo 'LIBRARY KERNEL32.dll'
 echo 'EXPORTS'
 for symbol in GetPrivateProfileStringW GetPrivateProfileSectionNamesW GetPrivateProfileSectionW GetFileAttributesW CreateDirectoryW DeleteFileW InitializeCriticalSection EnterCriticalSection LeaveCriticalSection GetModuleHandleW GetCurrentProcess FlushInstructionCache GetSystemDirectoryW LoadLibraryW GetProcAddress GetModuleFileNameW CreateFileW WriteFile VirtualProtect CloseHandle GetPrivateProfileIntW; do
  echo "$symbol=KERNEL32.$symbol"
 done
} > build/kernel32.def
ld -mi386pe --dll --out-implib build/libkernel32.a -o build/unused.dll build/kernel32.def
gcc -m32 -Os -Wall -Wextra -Werror -Wno-misleading-indentation -fleading-underscore -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 -c src/diagnostic.c -o build/diagnostic.o
ld -mi386pe --dll --entry _DllMain --image-base 0x10000000 --dynamicbase --nxcompat --subsystem windows --enable-reloc-section -o d3d9.dll src/exports.def build/diagnostic.o build/libkernel32.a

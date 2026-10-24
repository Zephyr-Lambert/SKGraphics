#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
for test in review_mock defaults_mock mock chain_mock character_mock character_mask_mock aa_mock smaa_mock shadow_mock texture_mock steam_texture_mock category_mock probe_mock scene_mock bloom_mock ao_depth_mock ao_mock; do
 gcc -Os -Wno-attributes -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -c "tests/$test.c" -o "build/$test-host.o"
 ld -e _start -o "build/$test-host" "build/$test-host.o"
 "build/$test-host"
 echo "Host $test passed (not a GPU test)."
done
python3 tests/hash_vectors.py

if [ "$#" -gt 0 ]; then
 gcc -shared -fPIC -Os -Wno-attributes -ffreestanding -fno-builtin tests/filter_bridge.c -o build/filter-test.so
 python3 tests/filter_semantics.py "$1"
 gcc -shared -fPIC -Os -Wno-attributes -ffreestanding -fno-builtin tests/outline_bridge.c -o build/outline-test.so
 python3 tests/outline_semantics.py "$1"
 python3 tests/outline_colors.py "$1"
fi

"""Compare two builds of d3d9.dll section by section.

Used to prove that a source cleanup (formatting, renaming) did not change
behavior. Code, data, imports and relocations must match exactly. The build
timestamp, PE checksum and the COFF symbol table (internal function names) are
ignored, because those change with every build or rename without affecting
what the DLL does.

Usage: python3 tests/compare_dll.py new/d3d9.dll reference/d3d9.dll
"""
import struct
import sys


def sections(path):
    data = open(path, 'rb').read()
    pe = struct.unpack_from('<I', data, 60)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    table = pe + 24 + optional_size
    result = []
    for i in range(count):
        entry = table + i * 40
        name = data[entry:entry + 8].rstrip(b'\0').decode()
        virtual_size, address, raw_size, raw_pointer = struct.unpack_from('<IIII', data, entry + 8)
        body = bytearray(data[raw_pointer:raw_pointer + raw_size])
        if name == '.edata':
            body[4:8] = b'\0\0\0\0'  # export directory timestamp
        result.append((name, address, virtual_size, bytes(body)))
    return result


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    new, reference = sections(sys.argv[1]), sections(sys.argv[2])
    if [s[:3] for s in new] != [s[:3] for s in reference]:
        print('DIFFERENT: section layout changed')
        return 1
    changed = [s[0] for s, r in zip(new, reference) if s[3] != r[3]]
    if changed:
        print('DIFFERENT: ' + ', '.join(changed))
        return 1
    print('Identical code and data (%d sections compared).' % len(new))
    return 0


if __name__ == '__main__':
    sys.exit(main())

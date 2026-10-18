#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare the pinned, attributed runtime in a fresh local directory; needs no game."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent

def fixtures(root):
    root.mkdir(parents=True)
    end = 0xffffffff
    dirs = struct.pack('<6I', 0, end, 24, 0, end, 0)
    dirs += struct.pack('<6I', 0, end, end, 40, end, 3) + b'dir\0'
    files = struct.pack('<IIQQII', 0, end, 0, 4, end, 8) + b'root.bin'
    files += struct.pack('<IIQQII', 24, end, 4, 6, end, 8) + b'data.bin'
    header = struct.pack('<10Q', 80, 80, 0, 80, len(dirs), 80 + len(dirs), 0,
                         80 + len(dirs), len(files), 80 + len(dirs) + len(files))
    valid = header + dirs + files + b'ABCDhello!'
    (root / 'valid.bin').write_bytes(valid)
    for name, offset, fmt, value in [('cycle', 108, '<I', 24),
                                    ('range', 80 + len(dirs) + 8, '<Q', 2**64 - 1),
                                    ('name', 100, '<I', 0xfffffff0)]:
        data = bytearray(valid)
        struct.pack_into(fmt, data, offset, value)
        (root / f'{name}.bin').write_bytes(data)
    (root / 'short.bin').write_bytes(valid[:60])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--vulkan-registry', type=Path, help='Existing vk.xml matching the installed Vulkan headers')
    args = parser.parse_args()
    metadata = json.loads((ROOT / 'UPSTREAM.json').read_text(encoding='utf-8'))
    for name, key in [('LICENSE.upstream', 'license_sha256'), ('mk8d.patch', 'patch_sha256')]:
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != metadata[key]:
            raise ValueError(f'{name} identity mismatch')
    revision = subprocess.check_output(['git', '-C', str(args.upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != metadata['commit']:
        raise ValueError(f'expected upstream {metadata["commit"]}, got {revision}')
    # Export committed bytes, so local upstream edits and Git newline settings cannot alter the input.
    archive = subprocess.check_output(['git', '-C', str(args.upstream), 'archive', '--format=zip', revision,
                                       'CMakeLists.txt', 'LICENSE', 'runtime', 'recomp', 'tests', 'tools'])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    (output / '.gitignore').write_text('*\n', encoding='utf-8')
    source = output / 'source'
    source.mkdir()
    with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
        for item in zipped.infolist():
            target = source / item.filename
            if not target.resolve().is_relative_to(source):
                raise ValueError('unsafe source archive path')
            if item.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zipped.read(item))
    subprocess.run(['git', 'apply', '--check', str(ROOT / 'mk8d.patch')], cwd=source, check=True)
    subprocess.run(['git', 'apply', str(ROOT / 'mk8d.patch')], cwd=source, check=True)
    fixtures(source / 'tests' / 'romfs-fixtures')
    if args.vulkan_registry:
        registry = args.vulkan_registry.resolve()
        import sys
        subprocess.run([sys.executable, '-B', str(source / 'tools' / 'gen_vk.py'), str(registry),
                        str(source / 'runtime' / 'src' / 'vk_gen.cpp')], check=True)
        metadata['vulkan_registry_sha256'] = hashlib.sha256(registry.read_bytes()).hexdigest()
    (output / 'ADOPTION.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print(source)

if __name__ == '__main__':
    main()

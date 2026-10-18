#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate private MK8D code and module images using an already prepared runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--prepared', type=Path, required=True)
parser.add_argument('--exefs', type=Path, required=True)
args = parser.parse_args()
root = args.prepared.resolve()
source = root / 'source'
private = root / 'private'
private.mkdir(exist_ok=False)
(private / 'analysis').mkdir()
(private / 'gen').mkdir()
npdm = args.exefs.resolve() / 'main.npdm'
npdm_bytes = npdm.read_bytes()
(private / 'main.npdm').write_bytes(npdm_bytes)
(root / 'NPDM-INPUT.json').write_text(json.dumps({'sha256': hashlib.sha256(npdm_bytes).hexdigest(), 'path': str(private / 'main.npdm')}, indent=2) + '\n', encoding='utf-8')
sys.path.insert(0, str(source))
from recomp import nso
manifest = {}
for name, base in [('main', '0x4000000000'), ('subsdk0', '0x4020000000'), ('sdk', '0x4010000000')]:
    path = args.exefs.resolve() / name
    raw = path.read_bytes()
    module = nso.load(str(path), name)
    # Confirm the NSO's per-segment identities before using its decompressed image.
    import struct
    flags = struct.unpack_from('<I', raw, 12)[0]
    for index, (offset, size) in enumerate([module.text, module.ro, module.data]):
        if flags & (1 << (index + 3)):
            expected = raw[0xa0 + 32 * index:0xc0 + 32 * index]
            if hashlib.sha256(module.image[offset:offset + size]).digest() != expected:
                raise ValueError(f'{name}: segment {index} SHA256 mismatch')
    (private / 'analysis' / f'{name}.bin').write_bytes(module.image)
    manifest[name] = {'source_sha256': hashlib.sha256(raw).hexdigest(), 'base': base}
    with (root / f'generate-{name}.log').open('w', encoding='utf-8') as log:
        subprocess.run([sys.executable, '-B', '-m', 'recomp', str(path), str(private / 'gen' / name),
                        '--name', name, '--base', base], cwd=source, stdout=log,
                       stderr=subprocess.STDOUT, check=True)
    print(f'{name}: generated; see private report.txt', flush=True)
(root / 'MODULE-INPUTS.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

# SPDX-License-Identifier: GPL-3.0-or-later
"""Authored ISA/descriptor fixtures; no game input or GPU required."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve()
flags = subprocess.CREATE_NO_WINDOW | subprocess.NORMAL_PRIORITY_CLASS if os.name == "nt" else 0
with tempfile.TemporaryDirectory(prefix="offline-nvn-test-") as temporary:
    root = Path(temporary)
    code = bytearray(0x100)
    control = bytearray(0x880)
    def put(data, offset, value):
        struct.pack_into("<I", data, offset, value)
    put(code, 0, 0x12345678)
    put(code, 0x30, 1 << 10)  # authored vertexB SPH
    struct.pack_into("<Q", code, 0x88, 0xe30000000007000f)  # authored scheduler word at0x80, then unconditional EXIT
    for offset, value in [(0,0x98761234),(0x6f8,0x60),(0x700,0x100),(0x704,0x100),(0x708,0x30)]:
        put(control, offset, value)
    code_path = root / "authored-code.bin"
    code_path.write_bytes(code)
    checks = 0
    def run(name, data, viewport, success):
        global checks
        ctrl = root / (name + ".ctrl")
        output = root / (name + ".spv")
        ctrl.write_bytes(data)
        result = subprocess.run([str(exe), str(code_path), str(ctrl), viewport, str(output)],
                                capture_output=True, text=True, timeout=30, creationflags=flags)
        assert (result.returncode == 0) == success, (name, result.stdout, result.stderr)
        if success:
            assert output.read_bytes()[:4] == struct.pack("<I", 0x07230203)
            report = json.loads(result.stdout)
            assert report["uniforms"] == report["storage"] == report["textures"] == 0
        else:
            assert not output.exists(), name
        checks += 1
    run("viewport-enabled", control, "1", True)
    run("viewport-disabled", control, "0", True)
    run("truncated-control", control[:100], "1", False)
    run("bad-magic", bytes(0x880), "1", False)
    bad = bytearray(control)
    put(bad, 0x6f8, 0xffffffff)
    run("bad-program-extent", bad, "1", False)
    run("missing-viewport-profile", control, "unknown", False)
print(f"offline shader authored cases={checks} PASS")

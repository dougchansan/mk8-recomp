#!/usr/bin/env python3
"""M1 probe: ask a running suyu what it actually sees in the target dump.

Answers the baseline questions the file logger cannot (issue #17), by going
through suyu's own MCP server rather than parsing an empty log.

Requires suyu.exe running with `-hacker` (or any explicit mode flag) so the
first-run ModeSelector modal does not block startup and the MCP listener comes
up. See scripts/run-gui.ps1.
"""

import json
import os
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402

ROM = os.environ.get("MK8R_ROM", "")


def call(name, args=None, timeout=300.0):
    out = rpc("tools/call", {"name": name, "arguments": args or {}}, timeout=timeout)
    if "error" in out:
        return {"_error": out["error"]}
    # Tool results arrive as MCP content blocks holding a JSON string.
    for block in out.get("result", {}).get("content", []):
        if block.get("type") == "text":
            try:
                return json.loads(block["text"])
            except json.JSONDecodeError:
                return {"_text": block["text"]}
    return out.get("result", out)


def show(label, value):
    print(f"\n=== {label} ===")
    print(json.dumps(value, indent=2)[:4000])


def main():
    show("system", call("get_system_info"))
    show("keys", call("get_keys_status"))
    show("firmware", call("get_firmware_status"))
    show("rom_info", call("get_rom_info", {"path": ROM}))
    show("emulator_state", call("get_emulator_state"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

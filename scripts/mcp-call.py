#!/usr/bin/env python3
"""Call one MCP tool, taking arguments as plain argv rather than JSON.

Passing JSON on a PowerShell command line does not survive: PowerShell strips
the inner double quotes when handing the argument to python.exe, so

    python mcp.py call launch_game_path '{"path":"F:\\game.xci"}'

arrives as {path:F:\\game.xci} and fails to parse. Taking each value as its own
argv element sidesteps the quoting entirely - a value with spaces is quoted once
by the shell and arrives intact.

  python scripts/mcp-call.py launch_game_path --path "F:\\Games\\Foo.xci"
  python scripts/mcp-call.py trigger_ui_action --action tas_start_stop
  python scripts/mcp-call.py get_emulator_state
"""

import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2

    name = argv[1]
    args = {}
    rest = argv[2:]
    i = 0
    while i < len(rest):
        key = rest[i]
        if not key.startswith("--"):
            print(f"expected --key, got {key!r}")
            return 2
        if i + 1 >= len(rest):
            print(f"missing value for {key}")
            return 2
        args[key[2:]] = rest[i + 1]
        i += 2

    out = rpc("tools/call", {"name": name, "arguments": args}, timeout=600.0)
    if "error" in out:
        print(f"error: {json.dumps(out['error'])}")
        return 1
    for block in out.get("result", {}).get("content", []):
        if block.get("type") == "text":
            print(block["text"])
            return 0
    print(json.dumps(out.get("result", out)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

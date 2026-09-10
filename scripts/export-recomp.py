#!/usr/bin/env python3
"""Drive suyu's AOT static recompiler over its MCP server.

The exporter is a modal Qt dialog with no CLI. Two calls are needed:

  1. trigger_ui_action{action=export_game} opens GameExportDialog and calls
     dialog.exec(), which blocks that MCP handler until the dialog closes. The
     call therefore never returns and its timeout is expected - fire and forget.
  2. trigger_ui_action{action=aot_test_export} finds the dialog via
     QApplication::activeModalWidget() and drives a real export past the file
     pickers (main.cpp:5529-5558).

Step 2 works because exec() spins a nested event loop, so the TCP server keeps
accepting connections while the dialog is up.

Export runs on the GUI thread with no cancellation and takes minutes on a real
title, so this polls the log rather than waiting on a response.

  python scripts/export-recomp.py [source|build]
"""

import json
import pathlib
import socket
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402

ROM = r"D:\Games\the target title.xci\the target title.xci"
OUT = r"G:\mk8-recomp\generated\target"


def fire_and_forget(name, args, wait=3.0):
    """Send a request we do not expect a reply to (a blocking modal handler)."""
    try:
        rpc("tools/call", {"name": name, "arguments": args}, timeout=wait)
        return "returned"
    except (TimeoutError, socket.timeout):
        return "blocked (expected)"
    except ConnectionResetError:
        return "connection reset - suyu probably crashed"


def main():
    fmt = sys.argv[1] if len(sys.argv) > 1 else "source"
    pathlib.Path(OUT).mkdir(parents=True, exist_ok=True)

    print(f"rom    : {ROM}")
    print(f"out    : {OUT}")
    print(f"format : {fmt}")

    print("\n[1/2] opening GameExportDialog ...")
    print("     ", fire_and_forget("trigger_ui_action", {"action": "export_game"}))

    time.sleep(3)

    print("[2/2] triggering aot_test_export ...")
    print("     ", fire_and_forget(
        "trigger_ui_action",
        {"action": "aot_test_export", "rom_path": ROM, "output_dir": OUT, "format": fmt},
        wait=10.0,
    ))

    print("\nExport is running on suyu's GUI thread. Watch progress with:")
    print("  python scripts/watch-export.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())

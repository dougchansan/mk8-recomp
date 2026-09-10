#!/usr/bin/env python3
"""Minimal client for suyu's built-in MCP server.

suyu's Qt frontend starts a JSON-RPC listener on 127.0.0.1:9742 whenever the GUI
is up and no game is running (src/suyu/main.cpp:5036-5079). No flag or env var
enables it. That listener is the only scriptable way to reach the AOT exporter,
which is otherwise a modal Qt dialog with no CLI.

Framing is raw JSON, one request per connection: the server reads whatever
arrives, parses it as a single object, and writes one response back
(mcp_server.cpp:456-500). There is no Content-Length header and no batching.

Export handlers block the server's event loop for minutes, so read timeouts here
are generous by default.

  python scripts/mcp.py tools
  python scripts/mcp.py call get_emulator_state
  python scripts/mcp.py call trigger_ui_action '{"action":"export_game"}'
"""

import json
import socket
import sys

HOST, PORT = "127.0.0.1", 9742


def rpc(method, params=None, timeout=1800.0):
    req = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params is not None:
        req["params"] = params

    with socket.create_connection((HOST, PORT), timeout=10.0) as s:
        s.settimeout(timeout)
        s.sendall(json.dumps(req).encode())
        chunks = []
        while True:
            # The server writes one response and does not close, so stop as soon
            # as what we have parses as a complete JSON object.
            chunk = s.recv(1 << 16)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                return json.loads(b"".join(chunks))
            except json.JSONDecodeError:
                continue
    raise RuntimeError("connection closed before a complete response arrived")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2

    cmd = argv[1]
    if cmd == "tools":
        out = rpc("tools/list", timeout=30.0)
    elif cmd == "init":
        out = rpc("initialize", timeout=30.0)
    elif cmd == "call":
        name = argv[2]
        args = json.loads(argv[3]) if len(argv) > 3 else {}
        timeout = float(argv[4]) if len(argv) > 4 else 1800.0
        out = rpc("tools/call", {"name": name, "arguments": args}, timeout=timeout)
    else:
        print(f"unknown command: {cmd}")
        return 2

    json.dump(out, sys.stdout, indent=2)
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

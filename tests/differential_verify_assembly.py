"""Verify each generated .inst matches the following independent mnemonic."""
import struct
import sys

data = open(sys.argv[1], "rb").read()
if not data or len(data) % 8:
    raise SystemExit("invalid instruction-pair byte count")
words = struct.unpack("<" + "I" * (len(data) // 4), data)
for i in range(0, len(words), 2):
    if words[i] != words[i + 1]:
        raise SystemExit(f"encoding mismatch pair {i // 2}: {words[i]:08x} != {words[i + 1]:08x}")
print(f"{len(words) // 2} GNU assembler instruction pairs verified")

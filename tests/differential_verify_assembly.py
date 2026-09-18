"""Check every encoding in the case table against a disassembler.

A mis-encoded word is the one failure the differential cannot find by itself.
It would decode to some other legal instruction, both engines would agree about
that instruction, and the case would report a pass while testing nothing of what
its name claims. So the words are checked against an independent decoder before
anything is run.

Input is the output of `differential-generate --encodings`: one
`hexword<TAB>expected disassembly` per line. Capstone is used rather than the
GNU assembler because the harness also builds on Windows, where no aarch64
assembler is installed; the check is stricter than the old round trip, since it
compares operands and not just the encoded bytes.
"""
import sys

try:
    import capstone
except ImportError:
    sys.exit("capstone is required: python -m pip install capstone")


def normalise(text):
    # Disassemblers differ on decimal vs hex immediates and on spacing, and
    # neither difference is a mis-encoding. Everything else must match.
    text = text.lower().replace(", ", ",").replace("\t", " ").strip()
    out = []
    for token in text.replace(",", " , ").split():
        if token.startswith("#"):
            body = token[1:]
            try:
                out.append("#" + hex(int(body, 0) & 0xFFFFFFFFFFFFFFFF))
                continue
            except ValueError:
                pass
        out.append(token)
    return " ".join(out)


def main(path):
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    failures = 0
    checked = 0
    with open(path, "r", encoding="utf-8") as handle:
        for lineno, line in enumerate(handle, 1):
            line = line.rstrip("\n")
            if not line.strip():
                continue
            word_text, _, expected = line.partition("\t")
            if not expected:
                sys.exit(f"{path}:{lineno}: expected 'hexword<TAB>mnemonic'")
            word = int(word_text, 16)
            actual = "<undefined>"
            for insn in md.disasm(word.to_bytes(4, "little"), 0x1000):
                actual = f"{insn.mnemonic} {insn.op_str}".strip()
                break
            checked += 1
            if normalise(actual) != normalise(expected):
                print(f"{word:08x}: expected {expected!r}, disassembles to {actual!r}")
                failures += 1
    if not checked:
        sys.exit("no encodings were checked")
    if failures:
        sys.exit(f"{failures} of {checked} encodings do not match their declared instruction")
    print(f"{checked} encodings match their declared instruction")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: differential_verify_assembly.py ENCODINGS")
    main(sys.argv[1])

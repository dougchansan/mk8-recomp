#!/usr/bin/env python3
"""Record a human visual verdict in the local playtest matrix."""

import argparse
import csv
import pathlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix")
    parser.add_argument("title_id")
    parser.add_argument("verdict", choices=("PASS", "FAIL", "NEEDS_INPUT"))
    parser.add_argument("--note", default="")
    args = parser.parse_args()
    path = pathlib.Path(args.matrix)
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        fields, rows = reader.fieldnames, list(reader)
    if not fields or "HumanVerdict" not in fields:
        raise SystemExit("matrix has no HumanVerdict column")
    if "ReviewNote" not in fields:
        fields.append("ReviewNote")
    matches = [row for row in rows if row["TitleId"].lower() == args.title_id.lower()]
    if len(matches) != 1:
        raise SystemExit(f"expected one title, found {len(matches)}")
    matches[0]["HumanVerdict"] = args.verdict
    matches[0]["ReviewNote"] = args.note
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"{args.title_id}: {args.verdict}")


if __name__ == "__main__":
    main()

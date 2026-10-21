"""Summarize sampled asynchronous GPU phases; missing samples are not zero work."""

import argparse
import collections
import importlib.util
import itertools
import json
import re
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "timing", Path(__file__).with_name("native_timing_report.py")
)
timing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timing)
HEADER = re.compile(
    r"^measure gpu phase: submit (\d+) present (\d+) slot (\d+) part (\d+) sample (\d+) ready (\d+) complete (\d+) segments (\d+) dropped (\d+) duration (\d+) ticks (\d+)$",
    re.MULTILINE,
)
SEGMENT = re.compile(
    r"^measure gpu phase segment: submit (\d+) present (\d+) slot (\d+) part (\d+) sample (\d+) index (\d+) duration (\d+) ticks (\d+) label (.*)$",
    re.MULTILINE,
)
META = re.compile(
    r"gpu phase: every (\d+) limit (\d+) bits (\d+) period_ns ([\d.eE+-]+)"
)


def category(label):
    if re.match(r"\d+x\d+ L\d+ fs ", label):
        return "render_pass"
    if label.startswith("compute "):
        return "compute"
    return {
        "texture uploads": "upload",
        "texture copies": "copy",
        "counter query copy": "counter_copy",
        "counter reduction": "counter_reduce",
        "outside passes (clears, uploads, copies)": "outside",
    }.get(label, "other")


def analyze(context, submit_ids=None):
    # CLI input is decoded bytes to preserve capture byte offsets. Normalize
    # Windows log endings here, after slicing, before anchored record matching.
    context = context.replace("\r\n", "\n")
    groups, errors, invalid = {}, [], set()
    for match in HEADER.finditer(context):
        values = list(map(int, match.groups()))
        key = tuple(values[:5])
        if submit_ids is not None and key[0] not in submit_ids:
            continue
        if key in groups:
            errors.append({"kind": "duplicate_header", "key": key})
            invalid.add(key)
            continue
        groups[key] = {
            "ready": values[5],
            "complete": values[6],
            "expected": values[7],
            "dropped": values[8],
            "duration_ns": values[9],
            "ticks": values[10],
            "segments": {},
        }
    orphan = 0
    for match in SEGMENT.finditer(context):
        values = list(map(int, match.groups()[:8]))
        key = tuple(values[:5])
        if submit_ids is not None and key[0] not in submit_ids:
            continue
        if key not in groups:
            errors.append({"kind": "orphan_segment", "key": key})
            orphan += 1
            continue
        group = groups[key]
        index = values[5]
        if index in group["segments"]:
            errors.append({"kind": "duplicate_segment", "key": key, "index": index})
            invalid.add(key)
            continue
        group["segments"][index] = {
            "duration_ns": values[6],
            "ticks": values[7],
            "label": match[9],
        }
    for key, group in groups.items():
        if group["ready"] not in (0, 1) or group["complete"] not in (0, 1):
            errors.append({"kind": "invalid_flags", "key": key})
            invalid.add(key)
        if not group["ready"]:
            continue
        segments = group["segments"]
        count = group["expected"]
        if not count or set(segments) != set(range(count)):
            errors.append({"kind": "missing_or_excess_segments", "key": key})
            invalid.add(key)
            continue
        if sum(s["ticks"] for s in segments.values()) != group["ticks"]:
            errors.append({"kind": "tick_nonconservation", "key": key})
            invalid.add(key)
        if (
            abs(sum(s["duration_ns"] for s in segments.values()) - group["duration_ns"])
            > count
        ):
            errors.append({"kind": "duration_nonconservation", "key": key})
            invalid.add(key)
        if bool(group["complete"]) != (group["dropped"] == 0):
            errors.append({"kind": "cap_flag_mismatch", "key": key})
            invalid.add(key)
    coarse, ambiguous_coarse = {}, set()
    for match in timing.GPU_PART.finditer(context):
        submit, present, slot, part, ready, ns = map(int, match.groups())
        if submit_ids is None or submit in submit_ids:
            key = (submit, present, slot, part)
            if key in coarse:
                ambiguous_coarse.add(key)
            coarse[key] = {"ready": ready, "duration_ns": ns}
    by_sample = collections.defaultdict(list)
    for key in groups:
        by_sample[(key[0], key[1], key[4])].append(key)
    qualified, missing_parts = [], 0
    for sample, keys in by_sample.items():
        submit, present, _sequence = sample
        expected = {key for key in coarse if key[:2] == (submit, present)}
        actual = {key[:4] for key in keys}
        if not expected or expected != actual:
            missing_parts += 1
            errors.append({"kind": "coarse_part_coverage", "sample": sample})
            continue
        if actual & ambiguous_coarse:
            errors.append({"kind": "ambiguous_coarse_records", "sample": sample})
            continue
        if any(not coarse[key]["ready"] for key in actual):
            errors.append({"kind": "unavailable_coarse_part", "sample": sample})
            continue
        if all(
            key not in invalid
            and groups[key]["ready"]
            and groups[key]["complete"]
            and coarse[key[:4]]["ready"]
            for key in keys
        ):
            qualified.append(keys)
    labels, categories, sample_ms = {}, {}, []
    for keys in qualified:
        sample_ms.append(sum(groups[key]["duration_ns"] for key in keys) / 1e6)
        for key in keys:
            for segment in groups[key]["segments"].values():
                for table, name in (
                    (labels, segment["label"]),
                    (categories, category(segment["label"])),
                ):
                    item = table.setdefault(name, {"duration_ns": 0, "occurrences": 0})
                    item["duration_ns"] += segment["duration_ns"]
                    item["occurrences"] += 1

    def totals(table):
        return sorted(
            [
                {
                    "label": name,
                    "sum_ms": item["duration_ns"] / 1e6,
                    "occurrences": item["occurrences"],
                    "mean_ms_per_sampled_submit": item["duration_ns"]
                    / 1e6
                    / len(qualified),
                }
                for name, item in table.items()
            ],
            key=lambda item: item["sum_ms"],
            reverse=True,
        )

    unavailable = sum(not group["ready"] for group in groups.values())
    capped = sum(bool(group["dropped"]) for group in groups.values())
    truncated = "measure: truncated" in context
    status = (
        "incomplete"
        if errors or unavailable or capped or truncated
        else "available"
        if groups
        else "missing"
    )
    metadata = [
        {
            "every": int(m[1]),
            "mark_limit": int(m[2]),
            "timestamp_bits": int(m[3]),
            "period_ns": float(m[4]),
        }
        for m in META.finditer(context)
    ]
    return {
        "status": status,
        "metadata": metadata,
        "reported_parts": len(groups),
        "qualified_sampled_submits": len(qualified),
        "sampled_submit_ms": timing.distribution(sample_ms),
        "unavailable_parts": unavailable,
        "capped_parts": capped,
        "invalid_parts": len(invalid),
        "missing_sample_part_sets": missing_parts,
        "orphan_segments": orphan,
        "truncated": truncated,
        "errors": errors,
        "categories": totals(categories),
        "labels": totals(labels),
        "limits": "Only complete sampled submits with matching coarse parts contribute to totals. Means divide by sampled submits, never all presentations. Queue timestamps include dependencies/stalls and observer overhead. Render-pass labels name the first shader, not every draw. CPU submit watermarks do not prove simulation-frame ownership or display scanout.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", type=Path)
    parser.add_argument("--events", nargs=2, type=int, metavar=("FIRST", "LAST"))
    args = parser.parse_args()
    raw = (args.case / "native.log").read_bytes()
    context = raw.decode("utf-8", errors="replace")
    ids = None
    scope = None
    if args.events:
        a, b = args.events
        events = json.loads((args.case / "result.json").read_text())["events"]
        if not 1 <= a < b <= len(events):
            parser.error("Events must be increasing one-based reviewed racing captures")
        first, last = (
            events[a - 1]["native_log_bytes"],
            events[b - 1]["native_log_bytes"],
        )
        if not 0 < first < last <= len(raw):
            parser.error("Invalid capture byte boundaries")
        frames = [
            tuple(map(int, m.groups()))
            for m in timing.FRAME.finditer(
                raw[first:last].decode("utf-8", errors="replace")
            )
        ]
        if len(frames) < 2:
            parser.error("No recorded intervals in the selected captures")
        lo, hi = frames[0][2] + 500_000_000, frames[-1][3] - 500_000_000
        frames = [f for f in frames if lo <= f[2] <= f[3] <= hi]
        if len(frames) < 2 or any(
            b[0] != a[0] + 1 for a, b in itertools.pairwise(frames)
        ):
            parser.error(
                "Boundary guards leave too few intervals or presentation IDs are missing"
            )
        ids = set()
        for previous, current in itertools.pairwise(frames):
            if current[1] < previous[1] or current[1] - previous[1] > 10000:
                parser.error("Invalid submit watermarks")
            ids.update(range(previous[1] + 1, current[1] + 1))
        scope = {
            "captures": [a, b],
            "boundary_guard_ms": 500,
            "intervals": len(frames),
            "submit_ids": len(ids),
        }
    result = analyze(context, ids)
    result["scope"] = scope
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

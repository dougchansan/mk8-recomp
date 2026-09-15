#!/usr/bin/env python3
"""Compare FPS sample sets from bench-ab.ps1.

The attract sequence alternates between a static title screen and a demo
race, so the distribution is bimodal and a mean is close to meaningless - it
mostly reports how much of each phase a run happened to catch. Percentiles are
compared instead, and the two clusters are split out, because the low cluster is
the race and that is the workload anyone cares about.

Any number of arms can be compared. Ratios are taken against a reference arm
(--reference, default the last one listed), so hybrid/baseline/static line up
against one denominator rather than against each other pairwise.

  python scripts/bench-compare.py hybrid.json baseline.json
  python scripts/bench-compare.py --metric vps --reference baseline *.json

Arms whose recorded recompiler state contradicts their name are called out.
An arm named "static" that reports jit_transitions > 0, or zero static blocks,
did not measure what its filename claims - the numbers are still printed,
because hiding them invites re-running until the answer looks right, but they
are marked so they cannot be quoted as a static result by accident.
"""

import json
import pathlib
import statistics
import sys


def pct(xs, p):
    if not xs:
        return float("nan")
    xs = sorted(xs)
    i = min(int(round(p / 100.0 * (len(xs) - 1))), len(xs) - 1)
    return xs[i]


def split_clusters(xs):
    """Separate the heavy (race) and light (title screen) phases.

    1-D 2-means. A largest-gap split was tried first and failed on real data:
    within-phase spread is wide enough (the baseline's title screen ranges
    1100-2200) that no single gap stands out against the total range, so the
    guard rejected genuinely bimodal data as one cluster. k-means keys off the
    grouping rather than off one gap.
    """
    if len(xs) < 6:
        return sorted(xs), []
    s = sorted(xs)
    lo, hi = s[0], s[-1]
    if hi < 1.5 * lo:
        return s, []                       # one phase only
    a, b = lo, hi
    for _ in range(50):
        left = [x for x in s if abs(x - a) <= abs(x - b)]
        right = [x for x in s if abs(x - a) > abs(x - b)]
        if not left or not right:
            return s, []
        na, nb = statistics.fmean(left), statistics.fmean(right)
        if abs(na - a) < 1e-6 and abs(nb - b) < 1e-6:
            break
        a, b = na, nb
    return left, right


def phases(xs, metric):
    """Return (heavy, light) for the metric, not (low, high).

    fps and vps are rates, so the race is the low cluster. frame_ms is a cost,
    so the race is the high one. Splitting without re-orienting labelled the
    race as the title screen on every frame_ms run.
    """
    low, high = split_clusters(xs)
    if not high:
        return low, []
    return (high, low) if metric == "frame_ms" else (low, high)


def load(path, metric):
    """Read one arm. Accepts the current record and the older bare list."""
    raw = json.loads(pathlib.Path(path).read_text())
    name = pathlib.Path(path).stem
    if isinstance(raw, list):
        if metric != "fps":
            raise SystemExit(
                f"{path} is an old fps-only sample file; --metric {metric} needs a re-run")
        return name, [float(x) for x in raw], {}
    return name, [float(x) for x in raw.get("metrics", {}).get(metric, [])], raw.get("recomp", {})


def arm_warnings(name, recomp):
    """Flag an arm whose recorded liveness does not match the name on the tin."""
    if not recomp:
        return ["no recompiler state recorded; arm validity unverified"]
    out = []
    first, last = recomp.get("static_blocks_first"), recomp.get("static_blocks_last")
    transitions, available = recomp.get("jit_transitions"), recomp.get("jit_available")
    lowered = name.lower()
    if "static" in lowered or "nojit" in lowered or "hybrid" in lowered:
        if not first and not last:
            out.append("executed no static blocks")
        elif first is not None and last is not None and last <= first:
            out.append("static block count did not advance during sampling")
    if "static" in lowered or "nojit" in lowered:
        if transitions:
            out.append(f"fell back to the JIT {transitions} time(s)")
    # A JIT being present is expected for a strict arm and disqualifying for a
    # nojit one. Warning on both would make the static arm permanently noisy and
    # train everyone to read past the line that matters.
    if "nojit" in lowered and available:
        out.append("a JIT was available; this is not a no-JIT host")
    if "baseline" in lowered and last:
        out.append("baseline executed static blocks; SUYU_RECOMP_DIR leaked into the arm")
    return out


def describe(name, xs, recomp, metric):
    print(f"\n{name}: n={len(xs)}  [{metric}]")
    for w in arm_warnings(name, recomp):
        print(f"  !! {w}")
    if not xs:
        return
    print(f"  min {min(xs):7.1f}   p25 {pct(xs,25):7.1f}   median {statistics.median(xs):7.1f}"
          f"   p75 {pct(xs,75):7.1f}   max {max(xs):7.1f}")
    heavy, light = phases(xs, metric)
    if light:
        print(f"  heavy phase (race):  n={len(heavy):2d}  median {statistics.median(heavy):7.1f}")
        print(f"  light phase (title): n={len(light):2d}  median {statistics.median(light):7.1f}")
    else:
        print("  single cluster - no phase split detected")


def main():
    args = sys.argv[1:]

    def opt(name, default):
        if name in args:
            i = args.index(name)
            v = args[i + 1]
            del args[i:i + 2]
            return v
        return default

    metric = opt("--metric", "fps")
    reference = opt("--reference", None)
    paths = args
    if not paths:
        raise SystemExit(__doc__)

    arms = [load(p, metric) for p in paths]
    if reference is None:
        reference = arms[-1][0]
    names = [a[0] for a in arms]
    if reference not in names:
        raise SystemExit(f"--reference {reference} is not one of {names}")
    ref = arms[names.index(reference)]

    for name, xs, recomp in arms:
        describe(name, xs, recomp, metric)

    # frame_ms is a cost, not a rate, so "faster" inverts. Keeping the ratio
    # oriented so that higher always means better avoids reading a 1.30x frame
    # time as an improvement.
    lower_is_better = metric == "frame_ms"
    _, ref_xs, _ = ref
    ref_heavy, ref_light = phases(ref_xs, metric)

    print(f"\nratio against {reference} [{metric}], higher means faster:")
    for name, xs, _ in arms:
        if name == reference or not xs or not ref_xs:
            continue
        heavy, light = phases(xs, metric)

        def ratio(a, b):
            if not a or not b:
                return None
            r = statistics.median(a) / statistics.median(b)
            return 1.0 / r if lower_is_better else r

        parts = [("overall", ratio(xs, ref_xs)),
                 ("heavy", ratio(heavy, ref_heavy)),
                 ("light", ratio(light, ref_light))]
        cells = "  ".join(f"{k} {v:6.2f}x" for k, v in parts if v is not None)
        print(f"  {name:<24} {cells}")

    print("\nCaveat: the phase split assumes every arm caught both phases in "
          "similar proportion. Check the n= counts above before trusting the "
          "overall median, and do not quote an arm carrying a !! warning.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

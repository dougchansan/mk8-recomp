#!/usr/bin/env python3
"""Compare two FPS sample sets from bench-ab.ps1.

The attract sequence alternates between a static title screen and a demo
race, so the distribution is bimodal and a mean is close to meaningless - it
mostly reports how much of each phase a run happened to catch. Percentiles are
compared instead, and the two clusters are split out, because the low cluster is
the race and that is the workload anyone cares about.

  python scripts/bench-compare.py hybrid.json baseline.json
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


def describe(name, xs):
    print(f"\n{name}: n={len(xs)}")
    if not xs:
        return
    print(f"  min {min(xs):7.1f}   p25 {pct(xs,25):7.1f}   median {statistics.median(xs):7.1f}"
          f"   p75 {pct(xs,75):7.1f}   max {max(xs):7.1f}")
    low, high = split_clusters(xs)
    if high:
        print(f"  heavy phase (race):  n={len(low):2d}  median {statistics.median(low):7.1f}")
        print(f"  light phase (title): n={len(high):2d}  median {statistics.median(high):7.1f}")
    else:
        print("  single cluster - no phase split detected")


def main():
    a_path, b_path = sys.argv[1], sys.argv[2]
    a = json.loads(pathlib.Path(a_path).read_text())
    b = json.loads(pathlib.Path(b_path).read_text())

    describe(pathlib.Path(a_path).stem, a)
    describe(pathlib.Path(b_path).stem, b)

    al, ah = split_clusters(a)
    bl, bh = split_clusters(b)

    print("\nratio (first / second), higher means the first arm is faster:")
    if a and b:
        print(f"  median overall  {statistics.median(a)/statistics.median(b):6.2f}x")
    if al and bl:
        print(f"  heavy phase     {statistics.median(al)/statistics.median(bl):6.2f}x")
    if ah and bh:
        print(f"  light phase     {statistics.median(ah)/statistics.median(bh):6.2f}x")

    print("\nCaveat: the phase split assumes both arms caught both phases in "
          "similar proportion. Check the n= counts above before trusting the "
          "overall median.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

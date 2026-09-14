# Static rendering fix

The AArch64 emitter decoded `FRECPE` and `FRSQRTE` as integer-to-float
conversions. A float's bits could therefore become a large numeric value instead
of a reciprocal estimate, corrupting 3D transforms while 2D overlays remained
readable. The fix implements native architectural estimates, including FPCR
handling and sticky FPSR exception flags. It does not add JIT fallback.

The same patch corrects FABS/FNEG versus FCMLT decoding and clears the upper bits
when loading an S-register pair. Default GNU module build flags now retain
`-foptimize-sibling-calls` for generated block chaining.

## Upgrade existing static modules

1. Update this checkout and its pinned submodules, or install the fixed emulator
   release from [the release page](https://github.com/dougchansan/suyu-v0.0.4/releases).
2. If building from source, rebuild the emulator **before exporting**. The
   emitter is compiled into it; changing the header alone does not update exports.
3. Re-export every module from your existing local dump with that emulator.
4. Rebuild the generated modules and replace the old loaded modules. Use a fresh
   build directory or explicitly set `RECOMP_OPT_FLAGS` to
   `-O1 -foptimize-sibling-calls` on GNU builds, since CMake caches old values.
5. Verify an actual 3D scene. Frame counts alone do not detect this regression.

For the configured Linux source workflow, `./scripts/reexport-rebuild.sh`
rebuilds the emulator, exports, and rebuilds modules. Preserve your target paths
and any extra discovery roots used by your existing build.

## Validation

Controlled local A/B runs rendered corrupted geometry with the original code,
clean geometry with native estimates, and corrupted geometry again after
restoring the original. The native-estimate-only comparison held the other
modules constant. Ordinary JIT lookup misses remained; this was not a JIT-free
validation. Manual menu transitions were not comprehensively tested.

The native lane core matched 649,600 Dynarmic values. Values and FPSR matched
the software reference; differences from the JIT backend's FPSR behavior are
not claimed resolved. Generated-C integration passed 133,920 checks each on
GCC/UBSan and MSVC, alongside decode, normalization, unary and pair-load tests.

Only synthetic tests, emulator source and tooling are distributed. Generated
game modules, captures, dumps, keys and firmware are not release assets.

## Run the synthetic regressions

From the repository root with GCC installed:

```bash
set -e
mkdir -p local/geometry-tests
for name in simd_normalization_decode_test simd_fp_estimate_decode_test \
            simd_fp_unary_compare_test simd_pair_load_test; do
  g++ -std=c++20 -O2 -I third_party/suyu/src "tests/$name.cpp" \
      -o "local/geometry-tests/$name-gen"
  "local/geometry-tests/$name-gen" > "local/geometry-tests/$name.c"
  gcc -std=c11 -O2 -fsanitize=undefined "local/geometry-tests/$name.c" \
      -lm -o "local/geometry-tests/$name"
  "local/geometry-tests/$name"
done
```

`fp_estimate_integration_test.cpp` uses the same generator/compiled-C pattern,
with a vector-file argument to the resulting executable. Generate those vectors
with `fp_estimate_oracle.cpp`, linked against the checkout's Dynarmic software
reference. Its inputs are deterministic synthetic IEEE values, independent of
any game. MSVC can compile the same generators and emitted C.

# Experimental CPU aliasing and shared memory

This optional R236 patch targets the historical R235 integration at `d223385`.
The current default is R245; combining the experiment with its thread changes
has not been verified. This patch is not
enabled by `prepare.py` and is not a demonstrated smooth-60 implementation.
Water Park performance remains unresolved. No game code, assets or binary is
included.

The Windows clang-cl path enables restricted references for the host CPU context,
matching the existing non-Windows annotation. Integer guest loads and stores up
to eight bytes use relaxed atomics so shared-memory polling remains observable
under that optimization. Existing acquire/release, exclusive-monitor and barrier
helpers remain unchanged. Restricted references alone fail an authored polling
control and must not be enabled without the memory-access change.

Apply `cpu-aliasing.patch` to a fresh source folder prepared from the `d223385`
integration, using `git apply --check` before `git apply`. Its exact SHA-256 and
baseline are recorded in `EXPERIMENT.json`.

Use a fresh build directory. Rebuild all generated CPU modules and runtime
objects against the changed `cpu.h`; leave `NX_PREBUILT_MODULE_DIR` and
`NX_RUNTIME_OBJECT_DIR` empty. Reusing the earlier R1 CPU archives does not test
this change. Existing generated title sources are compiled unchanged.

Windows validation rebuilt 400 static CPU objects and 89 runtime objects. All
41 component suites, 2,507 GPU checks, 2,511 allocation-specific GPU checks,
300-check sync/cutoff controls and 259 native readback checks pass. The new CPU
suite covers unaligned integer accesses, canaries, shared polling and exact
floating/vector memory bits. Other platforms remain untested.

A short original-game control averages 57.45 FPS across 3,840 course intervals,
with 520 above 20 ms. A longer run verifies Stadium first/15 points, required
GPU-fenced readback and Water Park lap 2/3. Its selected Stadium range averages
56.74 FPS and Water Park 40.53 FPS, with foreground changes during the flight.
Real muted XAudio2 consumption reports no dropped buffers and two underruns.
These runs do not isolate a causal speedup or prove sustained smooth 60, correct
120, complete-cup coverage, physical controls or subjective audio.

See the main [progress record](../../../../docs/native-runtime-progress.md).

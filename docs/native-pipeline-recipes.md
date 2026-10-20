# Native pipeline recipes and bounded prewarming

Status: opt-in prototype (`NX_PIPELINE_RECIPES`), default off; R248 source. This page is the
architecture record for removing first-use Vulkan pipeline creation from the
native replay without touching the static CPU code. Measured results and
hypotheses are labelled separately; numbers come from closed, owned runs.

## 1. Where pipelines are made today (R245)

| Object | Created by | Cache key | Thread | Lifetime |
| --- | --- | --- | --- | --- |
| `ShaderInfo` (SPIR-V, reflection, c[1] constants) | `shader_for` at `nvnProgramSetShaders`, from `<data root>/spirv/<hash>.spv` + `results.tsv` | 16-hex SHA-1 prefix of the Maxwell code block | game thread | process |
| `VkShaderModule` | `module_of` on a pipeline miss | `(ShaderInfo*, vertex integer kinds, unwritten FS inputs, fixed point size)` | replay | process |
| `VkDescriptorSetLayout` | `set_layout` | binding array bytes (value) | replay | process |
| `VkPipelineLayout` (`ProgramLayout`) | `program_layout` | vector of `ShaderInfo*` | replay | process |
| graphics `VkPipeline` | `graphics_pipeline` inside `draw` | `PipelineKey` bytes (contains `ShaderInfo*`) | replay, under the GPU lock | process |
| compute `VkPipeline` | `compute_pipeline` inside `dispatch_call` | `(ShaderInfo*, VkPipelineLayout)` | replay | process |
| `VkPipelineCache` | `pipeline_cache()` | driver blob `pipelines_<vendor>_<device>.bin` | replay | saved every 10 windows |

The graphics key holds the vertex/fragment `ShaderInfo*`, topology, point size,
16 vertex attributes and streams, 8 blend targets, colour/mask/depth/polygon
state, colour and depth `VkFormat`s, depth clamp, R11G11B10 targets,
alpha-to-coverage and the ultrawide scale. Viewport, scissor, blend constants,
stencil reference/masks and depth bias are dynamic state. Rendering uses
`VK_KHR_dynamic_rendering` (`VkPipelineRenderingCreateInfo`) unless
`NX_RENDER_PASS_OBJECTS=1`.

Module code is not just the offline SPIR-V: the vertex module is patched for
integer attribute formats (runtime vertex state), the fragment module for
inputs the paired vertex stage does not write, clip-z, fixed point size and the
optional ultrawide constant. So a module's identity depends on the pairing and
on runtime vertex formats, not only on the shader hash.

## 2. Measured opportunity (existing R245 logs)

Two closed R245 6000-presentation controls, warm driver seed
`pipelines_1002_7550.bin` (12,492,396 B, SHA-256 `c58c469d…`) copied into
each case:

| Run | Graphics pipelines created | Creation time (timer around `vkCreateGraphicsPipelines` only) | Largest window |
| --- | --- | --- | --- |
| R245 default priority | 988 | 1,124 ms | window 22: 636 in 1,048 ms (20.9 FPS window) |
| R245 native priority | 950 | 988 ms | window 21: 548 in 837 ms; window 85: 9 in 74 ms |

After window 22 the steady windows created under 3 ms per 60 frames, apart
from isolated misses (window 85 of the second run: 8.3 ms per pipeline). The
driver cache grew by 0.4–0.5 MB during each run, so even the "warm" seed did
not cover everything those runs used.

What this does and does not show:

- Measured: first-use creation is a real ~1 s burst near the course load, and
  occasional mid-race misses cost tens of milliseconds.
- Measured (R242): the steady race frame spends ~4.9 ms of real GPU execution;
  the 423 late intervals of the 57.28 FPS control are spread across the race.
  Pipeline creation explains at most the few windows above, not the steady
  shortfall from 60.
- Not measured before this work: shader-module patching/creation time (outside
  the timer), compute pipeline creation, layout creation, driver-cache hit
  ratio, and whether the burst lands in a loading screen or in rendered play.

## 3. Design

### 3.1 Recipes are hints, correctness is in-process

A recipe is a value description of one graphics pipeline from a previous run:
the two stage identities plus a canonical encoding of every `PipelineKey` field
except the pointers. Prewarming builds pipelines from recipes on worker threads;
a draw that misses the per-process pointer cache encodes its own key the same
way and adopts a prewarmed pipeline only when the value key matches exactly.

The pipeline is created by the same function, from the same key, in the same
process (same environment switches, device and features) as the draw would
have used. A stale or foreign recipe can only waste prewarm work or miss; it
cannot change what a draw binds. This is why the recipe header records the
translation profile for diagnostics only.

### 3.2 Stage identity

The 16-hex shader hash names the `.spv` file but does not prove its content.
Each stage also carries a 20-byte SHA-1 digest over: a domain tag, the stage,
the SPIR-V words, and the reflection fields pipeline creation reads (input and
output locations/components/types/arrays; binding set/binding/type/count).
Live `ShaderInfo`s get their digest once (they never move or die). A prewarm
stage is rebuilt from `<data root>/spirv/<hash>.spv` and reflected with the
same `reflect_spirv`; a digest mismatch rejects the recipe.

Reflection is sufficient for: descriptor-set layouts (set, binding, type,
count, stage), vertex input locations, fragment outputs, and all module
patches. Runtime metadata is necessary for: vertex formats/strides/divisors,
render-target formats, blend/depth/raster state, topology and point size
(all captured in the recipe), and for the per-draw descriptor contents
(`results.tsv` c[0] offsets, texture handle words, bound buffers and
textures), which recipes never capture.

### 3.3 Layouts

Descriptor-set layouts stay value-deduplicated (`set_layout`, now behind a
mutex because workers use it). The binding computation, including the
`NX_DYNAMIC_UBO` dynamic-offset policy, is shared by `program_layout` and the
prewarm path. Prewarm creates its own `VkPipelineLayout` per distinct tuple of
set-layout handles. A draw binds descriptor sets with its own program layout:
both layouts use the identical set-layout handles and no push constants, which
Vulkan defines as compatible for every set ("Pipeline Layout Compatibility").
Per-frame descriptor sets, dynamic offsets, uploads and texture views remain
runtime work in `nvn_sets.cpp`; only the reusable layouts are prewarmed.

D3D12 root signatures are not relevant here: there is no D3D12 backend.

### 3.4 Prewarm scheduling and adoption

- Recipes load at the first pipeline-cache use (first draw), after the driver
  cache is opened, and are queued in first-use order of the capturing run.
- Workers: `NX_PIPELINE_PREWARM_WORKERS` (1..4, default 2), below-normal
  priority, named threads. Each entry is Queued → Building → Ready/Failed.
- A draw miss with a Ready entry adopts it. A Building entry is waited for
  (never longer than compiling it again). A Queued entry is claimed and built
  inline by the draw, so the draw never waits behind other recipes.
- Unknown combinations take the unchanged inline path and become new recipes.
- Prewarm validates what `graphics_pipeline` would `fatal` on (unsupported point
  size, fragment output type mismatch) and skips those recipes instead.
- Disabled automatically with `NX_RENDER_PASS_OBJECTS=1` (render pass objects
  are created under the GPU lock) and with the SPIR-V tool diagnostics.
- Stop: a stop flag is checked between pipelines; the process exit path waits
  briefly for in-flight driver calls. Harness termination needs nothing.
- When the queue drains, the prewarm shader modules and stage copies are
  destroyed (Vulkan permits destroying modules once their pipelines exist).

### 3.5 Persistence

`pipeline_recipes_v1.bin` beside the driver cache (`NX_PIPELINE_CACHE_DIR` or
the data root). Header: magic, schema, key-encoding version, record count,
payload length, payload SHA-1, translation-profile hash and device identity
(diagnostic). Records are explicitly encoded little-endian fields with
bounds-checked counts; nothing is a pointer, handle or guest address.

- Schema or encoding change: the file is ignored and rewritten.
- Shader change: digest mismatch, recipe dropped at the next save.
- Corruption or partial write: payload SHA-1 fails, the file is ignored.
- Atomic replacement: write `.tmp`, flush to disk, `MoveFileExW`
  (`REPLACE_EXISTING | WRITE_THROUGH`).
- Bounded growth: at most 8,192 recipes; recipes used this run come first,
  then older ones by their previous order.

Portable recipe compatibility (shader digests, NVN-derived state, `VkFormat`s)
is separate from the driver's `VkPipelineCache`, which stays opaque and
vendor/device/driver specific. Never share a writable copy of either between
runs; test cases copy immutable seeds.

### 3.6 Instrumentation

With recipes or `NX_PIPELINE_STATS=1`: per-window line with draw-path misses,
adoptions, waits, inline claims, total first-use stall time (module + layout +
pipeline), worker build time, driver-cache hits from
`VkPipelineCreationFeedback` (core in Vulkan 1.3) and compute pipelines.
The existing `created over N frames` line is unchanged.

## 4. Rejected or deferred approaches

- **Serializing `PipelineKey` bytes** (suyu/yuzu style raw struct dump): carries
  pointers and padding, breaks silently with layout changes. Rejected in favour
  of explicit field encoding.
- **Recording Vulkan create-info instead of NVN state**: would duplicate the
  translation (and its environment switches) outside the code that defines it,
  and could drift. Rejected.
- **Blocking boot until every recipe is built** (suyu's load screen): visible
  delay and no benefit when the driver cache is warm. Rejected; prewarm is
  background and draws claim what they need.
- **Prewarm at `nvnProgramSetShaders` from live stages only**: simpler stage
  identity, but the lead time is unknown and possibly zero. Deferred; digest
  validation makes the startup approach equally safe.
- **Graphics pipeline libraries / `VK_EXT_shader_object`**: would cut the cost
  of genuinely unseen combinations, but changes the pipeline model and needs
  per-driver validation. Deferred until recipes show what remains.
- **Asynchronous pipelines with draw skipping** (as some emulators do):
  rejected outright: it drops draws.
- **Canonicalising keys (ignoring unread attributes, `BlendTarget::target`,
  unused streams)**: would reduce distinct pipelines but changes default cache
  identity. Deferred; measure duplicates first.

## 5. Verification plan

1. Unit test (no GPU): encode/decode round trip of every field, file round
   trip, truncated/bit-flipped/foreign-schema files rejected, atomic replace,
   bounded growth ordering.
2. GPU fixture: a prewarmed pipeline built from a separate copy of an authored
   stage is adopted (identical handle) by `graphics_pipeline`; a stage with a
   different digest is not; the adopted pipeline renders the same pixels as the
   inline path; full existing GPU/sync/cutoff/readback suites unchanged.
3. Title runs with identical executable, save and input (no captures):
   cold driver cache, warm seed, recipe capture, prewarm + cold, prewarm + warm.
   Report creation counts, adoption coverage, first-use stall time, burst
   window frame time, interval distribution, process memory.

## 6. Results (R247/R248, RX 9070 XT)

### Regressions

R248 passes 43 component suites (new: 611 CPU-only recipe checks: every key
field round-trips, truncated/bit-flipped/foreign files are refused whole,
atomic replacement, bounded merge order) and a 3,212-check GPU fixture (the
2,507 R245 checks plus 705 recipe checks). The GPU part prewarms authored stages
on real workers, adopts the prewarmed handle from a separate stage object,
rejects a recipe whose stage content changed, exercises the claim path, saves
and reloads the file, frees the prewarm modules after the queue drains, and
renders identical pixels with the adopted and an inline-built pipeline. The
whole 2,507-check suite also passes with recipes in prewarm, capture and
cache-off modes and with statistics only; sync and zero-cutoff controls pass
300/300, native readback 259/259, and clang `--analyze` reports no warnings in
the five changed sources. An independent read-only review found no defects;
its one suggestion (a named value-key prefix) is in R248.

### Title runs

Same executable per revision, same saved profile and scripted input, 6,000
presentations, async GPU, no captures, default thread priority, foreground
unchanged in every run. "Selection" is delivery windows 36–99 (3,840
intervals), the span R245 reported 57.28 FPS over; it starts after the
window-21 course-load burst. "Late" counts presentations over 20 ms. Window
FPS values are means over 60-presentation windows, not frame percentiles.

| Run | Driver cache | Recipes | Draw-path pipelines (ms) | Burst window worst presentation | All FPS / late | Selection FPS / late |
| --- | --- | --- | --- | --- | --- | --- |
| R247 A | cold | capture | 889 (12,866) | 5,637 ms | 47.56 / 1,613 | 50.42 / 1,237 |
| R247 B | warm | off | 981 (1,141) | 1,351 ms | 52.91 / 1,267 | 52.59 / 1,042 |
| R247 C | warm | prewarm (seed A, 889) | 61 (476) | 791 ms | 56.32 / 561 | 57.60 / 373 |
| R248 1 | warm | off | 949 (827) | 1,199 ms | 54.98 / 930 | 55.67 / 683 |
| R248 2 | warm | prewarm (seed C, 949) | 141 (1,104) | 488 ms | 56.74 / 456 | 57.31 / 267 |
| R248 3 | cold | prewarm (seed C) | 55 (1,035) | 797 ms | 56.37 / 559 | 56.65 / 395 |
| R248 4 | warm | off | 963 (740) | 1,138 ms | 54.51 / 1,039 | 54.50 / 833 |
| R248 5 | warm | prewarm (seed C) | 10 (116) | 541 ms | 56.40 / 664 | 56.73 / 469 |

Measured:

- Cold first use is expensive: 889 pipelines cost 12.9 s on the draw path,
  one presentation took 5.6 s, and 43 windows spent over 16.7 ms creating.
- The warm driver cache alone leaves ~0.7–1.1 s of draw-path creation and a
  ~1.1–1.35 s worst presentation in the burst window.
- Prewarm finishes before the burst in every run: 1.5–1.9 s after the first
  draw with a warm driver cache (2 workers, 2.9–3.7 s of worker time) and 7.7 s
  with a cold one (15.4 s of worker time). Draws waited for a building recipe
  once per run at most (≤3.5 ms); no recipe was claimed, failed or rejected.
- With full coverage (R248 5) only 10 pipelines remain on the draw path
  (116 ms) and none inside the selection.
- The burst window's worst presentation falls from 1.14–1.35 s to 0.49–0.80 s.
  What remains there is mostly not pipeline creation (the burst still creates
  7–36 new pipelines, 89–421 ms, plus loading work this change does not touch).
- Coverage follows content, not chance: misses relative to the seed are keys
  with other vertex formats/strides on the same shaders (different meshes), a
  colour target format, or a depth format. R248 2 met 140 new keys (84 at once
  in window 43, 533 ms); each saved file merges what it saw (1,089 recipes).
- 66 draws in the capture run already reused a pipeline through the value key:
  distinct shader hashes with identical SPIR-V and reflection.
- Memory: prewarm adds ~290 MB of private bytes while it runs; at the end of
  the runs prewarm cases sit 30–90 MB above recipe-off cases (4.69–4.75 GB versus
  4.65–4.66 GB), within the unchanged 6 GB guard. Prewarm modules (561) are
  released when the queue drains.

Not established:

- Steady frame rate. Prewarm runs averaged 56.7–57.6 selection FPS against
  52.6–55.7 for recipe-off controls in the alternating order, but recipe-off
  selections spent only 65–228 ms creating pipelines (≤0.06 ms/frame), far less
  than that difference. Desktop/scene variance between flights is of the same
  order (R245's recipe-off control measured 57.28). With two to three runs per
  arm this is not a causal FPS gain.
- Sustained smooth 60: every run still has hundreds of presentations over 20 ms
  in the selection with median windows at the 16.7 ms pacing interval. Those
  intervals are not explained by pipeline creation.

### Path to 60, 120 and 240

- 60 FPS: first-use pipeline stalls are now removable with a covering recipe
  file; the remaining late intervals need their own attribution (game-thread
  CPU, replay CPU, the ~4.9 ms of GPU execution and its fence waits, texture
  uploads/ASTC decode at load).
- 120 FPS (8.33 ms): the measured GPU execution fits, but replay plus game CPU
  is not yet measured against that budget, and MK8D's simulation runs at 60 Hz.
  Correct 120 requires decoupled presentation with interpolation or a verified
  higher simulation rate; presenting duplicate frames is not 120.
- 240 FPS (4.17 ms): the measured 4.9 ms of GPU execution alone exceeds the
  budget; not reachable without reducing GPU work.

### Next steps

- Canonical recipe keys: zero vertex attributes and streams the vertex stage
  does not read before encoding (most observed misses differ only there), after
  proving byte-identical create infos.
- Seed management: ship nothing title-specific; document generating a local
  seed with `NX_PIPELINE_RECIPES=capture`, and merge seeds from several flights.
- Compute pipelines (two per run here) and the masked-clear family stay inline.

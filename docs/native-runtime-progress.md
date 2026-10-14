# Native runtime progress

Updated 2026-10-14, 07:36 UTC. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

## Verified foundations and their limits

| Evidence | Verified result | Limit |
|---|---|---|
| Native NVN builders in the actual-game reference replay | 90,320 native calls; 6,001-frame replay reached EOF; CPU JIT 0, imports 0; observed racing | Still uses reference graphics. This is not standalone native gameplay. |
| Native Vulkan triangle fixture | 1,152 red pixels and 2,944 black pixels | Synthetic rendering; no actual-game draw/present. |
| Textured native fixtures | Five draws, 20,480 pixels checked and ten rejection cases | Synthetic resource/draw coverage only. |
| Native-owned pool copy | 64-byte copy and 29 error cases | Bounded memory ownership/validation test. |
| Persistent native images | Three-draw producer/consumer fixture checks 4,096 pixels and 71 rejection cases; retained native image is sampled directly | Synthetic Stage A. No intermediate target readback/reupload; actual-game state and content history remain required. |
| Standalone actual CPU execution | Native SM/LM/APM and applet/domain bootstrap reach a real copied CommonStateGetter event; 12,596,937 blocks and 72 SVCs reach the first event wait | Establishes CPU/service-boundary progress. Window focus, the wait resume chain and later platform services are still being integrated. |
| Public source/API tests | Synthetic API v2 identity/sealed-binding negotiation, full-range guards and bounded BufferInit checks pass | The miniature API fixture does not attest a complete actual title module or establish every required NVN contract. |
| Scheduled native API v3 | Nine return kinds, cancellation/commit races, stale completion and no-fallback behavior pass synthetic tests; emitted production-profile transport passes 144 guard mutations | CPU-owner bridge verified separately; real DeviceInitialize remains gated on provider effects and ownership isolation. |
| Native texture conversion | 210 independent tiling cases and CLI checks pass; five recorded source textures decode with matching hashes after dependency simplification | CPU backing is not authoritative for all GPU writes. GPU-produced target contents are excluded; no frame-equivalence claim. |
| Native window observations | Real owned Win32 window, bounded pump/wait, overflow/close handling and owner lifetime pass strict compiler analysis and tests | Six inactive observations and zero active observations establish no positive foreground activation or guest lifecycle wakeup. |
| Real shader translation and pipeline acceptance | Actual shader translation and pipeline acceptance verified separately | No actual-game native draw or presentation established. |

Synthetic fixtures are deliberate tests of implemented behavior. Their success cannot substitute for game rendering, service coverage, audio, input or persistence.

## Actual-game capture status

Capture 2 is diagnostic evidence: 31 draw records and a first submission were observed. The old harness aborted on a foreign-job check, so that capture is **not a replay pass**.

Capture 3 finished the bounded 6,001-frame reference replay with exit 0 and no harness invalidations. Its final frame shows Stadium racing. It captured 237 graph nodes, five submitted draws with eligible snapshots, ordered uniform updates and 44,230,424 resource bytes; observer exceptions and unmatched returns were zero. Resource/shader/texture interpretation still needs validation, so the capture reports `replay_ready=false`. These are inputs for native rendering; the successful replay still used the reference GPU path.

Capture 4 also completed 6,001 frames with exit 0 and no harness invalidations. Its new bounded sidecar verifies the live default device provider, flags 0, successful reference initialization and five actual texture-bank setup packets. The 512-event cap was reached after setup, so it does not establish complete later query/reinitialization coverage or native device initialization.

Decoded draw state establishes source-alpha blending, a destination-multiplication pass, float2/float3 vertices, linear repeat/clamp sampling and a negative-Y viewport transform. There is no Clear in the submitted graph, and full destination-independent overwrite is unproved. A faithful native replay requires the earlier attachment contents or a complete overwrite proof; an invented black target is not a valid substitute.

The next graphics gate is a faithful actual-game draw/submission/presentation path with complete inherited state and resources. Native audio, input, saves and overall gameplay closure remain pending. A functioning static CPU or native Vulkan fixture alone does not close those gates.

## Public checkpoint and prerelease

The checkpoint targets suyu source revision:

```
347fcf91d95cd39cb5c3db50f1b6d827ad699311
```

Source changes are represented by `src/patches/native-runtime.patch` against that exact base. `scripts/prepare-native-source.py` reproduced all 81 checkpoint files in a fresh detached worktree. `scripts/package-native-checkpoint.py` rebuilt the public tools and passed the CPU, service, entry/API, estimate, buffer and Vulkan checks using existing dependencies on the development host. [Verification](../native-checkpoint/verification.json) records binary hashes, compiler versions, flags and actual PE imports. These notes do not claim a verified setup on a fresh machine.

The [prerelease `native-foundations-20261014`](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-20261014) contains 13 Windows x64 synthetic verification tools, fixtures and corresponding source. It is **not a playable MK8D build**. This frozen v2 checkpoint includes the LM close-lifetime fix. The later v3, event/window, persistent-image and texture work described above is active campaign progress, rather than part of that immutable release. A subsequent source/build checkpoint must pass its own fresh verification before publication.

The development checkout also keeps a gitignored `local/native-builds/latest.json` index with exact public-package and private diagnostic executable hashes. Game-linked CPU/reference builds remain local; only tools with synthetic inputs and publishable corresponding source become release assets. See [build instructions](native-builds.md) for the current public package.

The public checkpoint excludes the private exact copy32 matcher and its tests because their provenance is unsuitable for this checkpoint. That omission does not erase the preserved private experiment. No game code, generated game archives, caches, NSOs or keys are published. Keep locally supplied title data separate from public source and release packages; see [LEGAL.md](../LEGAL.md).

## Completion gates

| Area | Remaining proof |
|---|---|
| Native graphics | Actual-game resource/state/shader/draw/submit/present coverage without Maxwell processing |
| Native platform | Required service, memory, scheduling and synchronization behavior without the general emulator runtime |
| Audio/input/storage | Audible output, usable controls and save/reload persistence through native adapters |
| Standalone package | Reproducible supported build and boot/menu/race/restart sequence with all required native dependencies accounted for |

Performance measurements remain useful diagnostics. Correctness and native dependency coverage determine completion. September 2026 suyu-hosted FPS figures in the [README](../README.md) are historical results, not measurements of a completed native port.

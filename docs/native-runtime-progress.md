# Native runtime progress

Updated 2026-10-14. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

## Verified foundations and their limits

| Evidence | Verified result | Limit |
|---|---|---|
| Native NVN builders in the actual-game reference replay | 90,320 native calls; 6,001-frame replay reached EOF; CPU JIT 0, imports 0; observed racing | Still uses reference graphics. This is not standalone native gameplay. |
| Native Vulkan triangle fixture | 1,152 red pixels and 2,944 black pixels | Synthetic rendering; no actual-game draw/present. |
| Textured native fixtures | Five draws, 20,480 pixels checked and ten rejection cases | Synthetic resource/draw coverage only. |
| Native-owned pool copy | 64-byte copy and 19 error cases | Bounded memory ownership/validation test. |
| Standalone actual CPU execution | Native SM/LM sessions and APM service creation succeed; IPC-close/handle lifetime fixed; current private run reaches the next required service, `appletOE`, after 12,567,698 blocks and 46 SVCs | Establishes CPU/service-boundary progress, not complete platform services or gameplay. APM commands have synthetic coverage; the title has not exercised them yet. |
| Public source/API tests | Synthetic API v2 identity/sealed-binding negotiation, full-range guards and bounded BufferInit checks pass | The miniature API fixture does not attest a complete actual title module or establish every required NVN contract. |
| Real shader translation and pipeline acceptance | Actual shader translation and pipeline acceptance verified separately | No actual-game native draw or presentation established. |

Synthetic fixtures are deliberate tests of implemented behavior. Their success cannot substitute for game rendering, service coverage, audio, input or persistence.

## Actual-game capture status

Capture 2 is diagnostic evidence: 31 draw records and a first submission were observed. The old harness aborted on a foreign-job check, so that capture is **not a replay pass**.

Capture 3 finished the bounded 6,001-frame reference replay with exit 0 and no harness invalidations. Its final frame shows Stadium racing. It captured 237 graph nodes, five submitted draws with eligible snapshots, ordered uniform updates and 44,230,424 resource bytes; observer exceptions and unmatched returns were zero. Resource/shader/texture interpretation still needs validation, so the capture reports `replay_ready=false`. These are inputs for native rendering; the successful replay still used the reference GPU path.

The next graphics gate is a faithful actual-game draw/submission/presentation path with complete inherited state and resources. Native audio, input, saves and overall gameplay closure remain pending. A functioning static CPU or native Vulkan fixture alone does not close those gates.

## Public checkpoint and prerelease

The checkpoint targets suyu source revision:

```
347fcf91d95cd39cb5c3db50f1b6d827ad699311
```

Source changes are represented by `src/patches/native-runtime.patch` against that exact base. `scripts/prepare-native-source.py` reproduced all 81 checkpoint files in a fresh detached worktree. `scripts/package-native-checkpoint.py` rebuilt the public tools and passed the CPU, service, entry/API, estimate, buffer and Vulkan checks using existing dependencies on the development host. [Verification](../native-checkpoint/verification.json) records binary hashes, compiler versions, flags and actual PE imports. These notes do not claim a verified setup on a fresh machine.

The [prerelease `native-foundations-20261014`](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-20261014) contains 13 Windows x64 synthetic verification tools, fixtures and corresponding source. It is **not a playable MK8D build**. This frozen v2 checkpoint includes the LM close-lifetime fix. Subsequent APM and scheduled native-API work remains in the active campaign and will receive a later verified checkpoint.

The public checkpoint excludes the private exact copy32 matcher and its tests because their provenance is unsuitable for this checkpoint. That omission does not erase the preserved private experiment. No game code, generated game archives, caches, NSOs or keys are published. Keep locally supplied title data separate from public source and release packages; see [LEGAL.md](../LEGAL.md).

## Completion gates

| Area | Remaining proof |
|---|---|
| Native graphics | Actual-game resource/state/shader/draw/submit/present coverage without Maxwell processing |
| Native platform | Required service, memory, scheduling and synchronization behavior without the general emulator runtime |
| Audio/input/storage | Audible output, usable controls and save/reload persistence through native adapters |
| Standalone package | Reproducible supported build and boot/menu/race/restart sequence with all required native dependencies accounted for |

Performance measurements remain useful diagnostics. Correctness and native dependency coverage determine completion. September 2026 suyu-hosted FPS figures in the [README](../README.md) are historical results, not measurements of a completed native port.

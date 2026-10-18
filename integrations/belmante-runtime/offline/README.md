# Offline NVN shader diagnostic

This GPL-3.0-or-later tool uses the existing suyu shader-only compiler through
`native_shader_bridge`; it does not load game CPU modules, submit GPU commands,
or translate shaders inside the game. The bridge source is retained in this
repository's historical native source patch (`native-runtime/shaders`). Use
matching headers and existing compiled dependencies, without installing packages.

Add this directory with `add_subdirectory` after the bridge target is defined in
a local prepared shader-tool build. Build with at most two compiler jobs. Run:

```
mk8_offline_nvn_shader own-code.bin own-control.bin 1 own-output.spv [2:byte-offset[:cube|:array2d|:cube-array|:2d-shadow|:array2d-shadow] ...]
python test_offline_shader.py path/to/mk8_offline_nvn_shader
```

The third argument explicitly selects viewport-transform state, 0 or 1. The tool
checks NVN code/control extents, copies the declared program and constants into
a checked native snapshot, and maps uniform bindings into the native runtime's
per-stage ranges. It supports vertexB/fragment stages, uniforms and an explicit
sampled-float profiles for normalized 2D, cube, color arrays and 2D/array depth comparisons. Optional arguments name driver-bank
texture slots, such as `2:40` or `2:112:cube`; these are compile-time type symbols, not captured
game handles. Control flow uses the original constant environment before those
symbols are supplied, and driver constant folding remains limited to bank 1.

A textured output includes a `.profile` sidecar. Keep it beside its SPIR-V file.
The runtime checks reflected resource shape and binding, then requires the real
bound texture to match the reflected dimension, format and non-MSAA profile before
upload or draw. Depth comparisons also require depth mode, format and comparison
sampler state. Cubemaps use six faces in storage sizing. Missing/incompatible
textures stop execution. The graphics tool supports inspected read-only storage-buffer bindings with explicit resource sidecars; writes and other resource classes remain unsupported. Live native physical backing and bounds are checked before upload. The
separate raw-compute tool below supports its inspected buffer/texture subset. Required absent metadata
fails; the tool does not select a default type for unspecified slots. Thirteen authored
parser/profile cases pass, including invalid/duplicate slots and viewport state.

The diagnostic profile uses SPIR-V 1.0, float attributes/outputs and triangle
topology. Those are explicit specialization assumptions. Translation and driver
module creation do not prove draw-state compatibility, pipeline validity,
rendering equivalence or gameplay. The local startup cache contains 3,894 pretranslated graphics/compute stages from the inspected
asset set, with Vulkan module acceptance. This is not full-game shader coverage;
startup passes application display-version metadata and continues into further RaceCommon assets. Game-linked inputs, SPIR-V and
builds stay private. Missing required translations remain fatal in the runtime.

## Older raw compute kernels

`mk8_offline_nvn_compute own-code.bin own-control.bin own-output.spv [2:byte-offset[:3d] ...]`
handles the inspected SDK53 raw compute layout: no code wrapper or SPH, program
start zero, actual control workgroup/shared sizes and actual inline constants.
The bounded local-memory-zero profile rejects local-memory operations. Eight
authored cases cover a minimal compute program and malformed metadata.

Outputs include native buffer-binding metadata (`.resources`) and a `.profile`
sidecar. The runtime checks required physical buffer backing, ranges, permissions,
workgroup metadata and hardware limits before dispatch. Missing resources and
pipeline failure stop execution. Textured compute has an explicit sampled-float
profile; 3D sampling currently requires complete single-level RGBA8 API texels,
with a native voxel readback regression. Raw tiled volumes and other compute
resource classes remain unsupported. Seven startup kernels translate locally;
module acceptance and synthetic upload checks do not prove title compute results.

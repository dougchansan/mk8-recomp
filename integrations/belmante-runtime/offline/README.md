# Offline NVN shader diagnostic

This GPL-3.0-or-later tool uses the existing suyu shader-only compiler through
`native_shader_bridge`; it does not load game CPU modules, submit GPU commands,
or translate shaders inside the game. The bridge source is retained in this
repository's historical native source patch (`native-runtime/shaders`). Use
matching headers and existing compiled dependencies, without installing packages.

Add this directory with `add_subdirectory` after the bridge target is defined in
a local prepared shader-tool build. Build with at most two compiler jobs. Run:

```
mk8_offline_nvn_shader own-code.bin own-control.bin 1 own-output.spv
python test_offline_shader.py path/to/mk8_offline_nvn_shader
```

The third argument explicitly selects viewport-transform state, 0 or 1. The tool
checks NVN code/control extents, copies the declared program and constants into
a checked native snapshot, and maps uniform bindings into the native runtime's
per-stage ranges. It supports vertexB/fragment stages and uniform resources.
Required absent metadata and other resources fail; the next actual textured
fragment requires texture-handle/type metadata and is currently rejected.

The diagnostic profile uses SPIR-V 1.0, float attributes/outputs and triangle
topology. Those are explicit specialization assumptions. Translation and driver
module creation do not prove draw-state compatibility, pipeline validity,
rendering equivalence or gameplay. Three actual shaders have translated locally;
modules were created/destroyed without drawing. Game-linked inputs, SPIR-V and
builds stay private. Missing required translations remain fatal in the runtime.

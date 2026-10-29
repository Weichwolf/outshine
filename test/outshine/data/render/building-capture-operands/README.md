# Submitted original-capture operands

Fixed input for #1724/#990, captured from the existing native foreground control
on A18 Pro/Metal. `manifest.json` records source commit, camera, source lineage,
code/array hashes and the instrumented executable hash. `operands.npz` preserves
the actual little-endian bytes after upload packing and at uniform submission.

| Array | Meaning |
|---|---|
| `vertex_positions_f32` | Submitted local Float32 vertices, shape `(94999, 3)` |
| `triangle_indices_u32` | Submitted triangle indices, shape `(46889, 3)` |
| `geometry_view_uniform_f32` | 56 Float32 values: current/previous/light matrices at 0/16/32; current/previous shifts at 48/52 |
| `mesh_anchor_f64` | CPU mesh anchor before the current shift is rounded |
| `gpu_placement_bytes` | One 144-byte `GpuPlacement`; current/previous Float32 matrices at byte 0/64, four UInt32 fields at 128 |

Matrices are column-major. Lit evaluates `View * (Model * p + Shift)`; Flat adds
Shift to the model translation before its matrix-vector product. The two
expression families require separate numerical qualification.

The dump was taken after `GeometryResidency::Fill` and inside
`GeometryRenderer::FrameUniformsOf` in a private temporary checkout; production
sources and shaders were restored afterward. Placement upload bytes equal the
rows used at draw time. Vertex/index streams equal the concatenated original
parts reconstructed by `field_background_inputs.original_inputs()`, with index
offsets. Native colour and depth exactly equal the existing frozen
`building-field-composition/captures.npz` original capture.

This supplies operands, not a qualified production compiler expression, a
conservative projection bound or World admission. It does not approve HDR
integrals, pose reuse, frame time or peak memory. Dump timings are not performance
evidence. Normal tests must not regenerate this input.

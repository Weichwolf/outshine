# Reusing qualified affine sphere transforms

Owner: #1063, parent #1048. This is a follow-up to the conservative caster
selection in #1058; its SIGGRAPH references and Float32 bounds contract apply.
The same placement matrix previously underwent Float32 qualification and the
same shear-safe scale calculation for every resident cluster in a batch.

`SphereTransform::From` now prepares that matrix and scale once per batch.
`Apply` retains the original per-sphere centre, guarded intermediate-magnitude
checks, error radius and outward packing, with the same arithmetic order.
`TransformedSphere` delegates to these same operations for existing consumers.
Preparation owns a matrix copy, not a reference to the input placement. Invalid
transforms keep unknown bounds and therefore full conservative shadow coverage.
There is no GPU resource, shader, binding, LOD or persistent cache change.

The existing native cost fixture is compiled with identical complete source
flags for the old and new implementations, using the original native Tokyo
snapshot from #1058. A private post-timing export writes each batch's range count,
index intervals, bound-presence flag and all four packed sphere floats; it writes
no C++ padding. All 8,214 batches and 50,330 ranges are bit-identical:
1,474,952 bytes, SHA256
`5e77b96f64f12a522583c893d2b163f98b06669d7c53dd9163fc111161b9d904`.
Both arms qualify 8,213 batches and retain 1,540,664 bytes of prepared vector
capacity. This is that fixture's heap, not a complete process/UMA peak claim.
The prepared transform uses automatic storage and adds no allocation.

On i5-1135G7, Clang22/libc++, four alternating fresh processes per arm each
perform ten complete rebuilds. Median of the four warm nine-rebuild medians:
6.149 ms original versus 4.995 ms prepared, an 18.8% reduction in this case.
All four paired warm medians improve. Timings retain their recorded host load;
no isolated device or frame-budget admission follows. Unchanged frames already
reuse the selector, so the gain applies to rebuilds, not every frame. Whole-table
invalidation remains a separate optimization opportunity.

Original input, private exact-output fixture, full compile/link/run commands,
eight raw process reports and the paired summary remain under
`/tmp/outshine-engine-affine-shadow-bounds-35916c6cb/` (`cost.py`, `cost.json`,
`cost-terminal.json`, `CompareCasterBounds.cpp`). The original caster snapshot
remains in the #1058 evidence directory. No source/assets/oracles are regenerated.

Three focused native consumers pass 2,069 checks: conservative clipping includes
2,000 sheared/translated Float32 trials plus explicit prepared-placement ownership;
caster selection and actual vertex-pose invalidation retain their original checks.
All 52 translation units in the existing GeometryRenderer dependency closure are
rebuilt with complete actual flags; unchanged archive members are reused.
Seven relevant product/test units pass focused clang-tidy with those same flags;
format and diff checks pass. This is not a fresh whole-engine build or broad gate.

The full fresh-process moving Iris run completes all 60 CPU/GPU samples and keeps
the exact original PNG hash from #1058; its image was opened. Its recorded loaded
host yields CPU p99 90.246 ms and GPU p99 53.578 ms, with all 60 frames unrefined.
That functional run does not establish a frame-time improvement or admission.
`runtime.json` binds the actual overlay build/source to its report; the client's
frozen shader-working-directory commit alone does not identify that overlay.
Master owns integration and bundled Metal/A18/Places counterchecks.

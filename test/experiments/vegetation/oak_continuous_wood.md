# Continuous wood follows authored axes

Owner: vegetation, [#687](https://github.com/Weichwolf/outshine/issues/687),
parent [#682](https://github.com/Weichwolf/outshine/issues/682).
The [prepared mature-size oak](oak_scaffold.md) showed transverse cap lines along
its main limbs. This replaces only the cached structural wood with the existing
continuous C1 sweeps. All90000 leaves, complete placements, eight physical Blade
meshes, source skeleton and procedural material inputs remain unchanged.
No library/native/shader API, species default or complete tree admission.

## Sources and representation

First local SIGGRAPH1985 [Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
PDFpages3/5 were opened optically: section rings, continuous limb frames and
separate ramiform junctions. We reuse the established C1 Hermite/PCHIP and
rotation-minimizing frames from [#423](continuous_limbs.md), not the paper's
C2/Frenet construction. The existing Ghidra ComputeWeldRing export at0x1c855c0
was revisited: conditional start-node ring delegation and later mesh-weld-ring
work are separate. Unknown virtual/property fields are not assigned invented
physical semantics. This is static workflow evidence, not a new numeric vendor
oracle or a proof that Modeler uses this algorithm. No vendor code/assets copied.

A full [#429 cell union](crown_union.md) would request15243 source bodies against
its existing8192 cap, before its own face limit. Do not widen those limits for
this tree. The continuous-body candidate needs195888 faces within its existing
262144-face capacity. Separate side limbs/short shoots remain separate solids;
this does not claim a welded whole tree or completed organic junctions.

## Preserve source anatomy before selecting continuations

Radius-only dominance produces a91-node root path. At source35 it turns into
side child180, losing the known trunk continuation36. An independent global
SciPy Hermite diagnostic with the same source-index derivative definition gives
sampled radius×curvature2.616215916638 at that turn. It violates the unchanged
tube sampler condition. A long-curve capacity extension would not fix this.

The fixed oak seed format appends each authored cubic's nodes consecutively;
its stored seed-parent prefix retains those seven original axes. The adapter
extracts199 directed continuation hints **from that documented source format**,
not from arbitrary graph IDs. Generic `dominant_paths(..., continuations=...)`
accepts only matching integer rows and actual parent→child edges. Declared
continuations take precedence; elsewhere the existing strength/alignment/ID rule
remains. The old default and its finite bounds are unchanged.

Now every one of5471 source edges belongs to exactly one of1301 limbs; maximum
path length65 fits the original CubicSpine and sampler limits. The independently
sampled root condition falls to.082989950162, not a bound over every limb or the
continuous surface. Production checks the existing sampled condition on all
limbs; complete saved triangle geometry is checked separately. No constructor,
frame, face, union or existing tolerance gate is raised. Unneeded long-curve
production code was discarded; the rejected global diagnostic remains executable.

Source nodal radii/root/terminal choices stay fixed. C1 centres interpolate the
source nodes and PCHIP supplies radii, so the solid **between** nodes changes;
old segment volume/envelope is not preserved. The original seed cubic ownership
is retained, not a claim of exact reconstruction of its analytic curve between
knots. Existing8-sided rings/two samples per source edge remain. Only two hard
caps per limb, with shared interior vertices/normals; side-fork caps still exist.
New geometry identity binds the old complete product and source-wood hash,
physical recipe, explicit continuation edges and sampling. Logical leaf-plan
identity stays fixed; old shading/visibility products cannot be assumed valid.

## Complete product and actual optical result

Source product identity starts889bbae13eea; source leaf-plan identity
029f1af642d33a6aad5ba2adc640ce77ce7ce9f4505770e78800c18342c59d34.
New product identity645ac64957aab862c79613516652bd7d8d17805d9af3a2396f41792de1dab1a8.
The producer reads prepared positions/parents and only the cached leaf-variant
column for identity; it does not expand leaf poses or regenerate materials.
Original short-shoot vertices, relative indices, corner normals, UVs and material
indices are copied exactly after the replaced body. Thirteen non-wood/non-entry
source files, including tables, blades, maps and skeleton, are byte-identical.

The2560×1440 whole pair was opened: left original segmented candidate, right
continuous candidate, same leaves/material/light, translations only separate
subjects. Whole improvement is modest. The matched near image was opened
against the original #685 near image: the large diagonal limb loses transverse
cap lines and has a continuous surface. Sharp side joins, generic bark and
repetitive blade fans remain; crown architecture is still too uniformly shaped.
No botanical Quercus, SpeedTree, forest or dynamic LOD/wind acceptance follows.

The near camera,2274 fine leaf IDs,87726 coarse leaves, target/eye, physical
source tables and resolution/samples match the original exactly. Complete
coarse context remains for visibility/shadows. Only wood changes; the previous
near image is a retained actual reference, not a new render or a retouched asset.
Procedural preview materials are still qualitative, not calibrated oak optics.

## Checks, work and measured costs

63 source and63 fresh installed-library controls pass. They include an
independent toy fork where thickness dominance violates the sampled tube rule,
explicit axis continuation retaining every edge, invalid-hint rejection and
whole-demand refusal before meshing, plus existing limb/collar/union/plant
controls. The default canonical wood also matches frozen c759d37d2 exactly on
a real original2100-node/499-limb source: all four body fields/75152 faces.

Fresh product decode forbids growth, foliage, Blade meshing and structural wood
meshing. A separate full cold check reverses every host demand, checks original
rows/petioles/frames, rebuilds the wood binding and checks all complete meshes.
The existing independent #429 topology checker verifies each of1301 new limbs:
one connected component, one vertex fan at all100546 body vertices, edge incidence
and opposite directions, Euler2 and positive centroid-relative signed volume.
Minimum limb volume8.74620377649e-7m³; maximum source-anchor component error
1.11e-16m with an absolute2e-14m check; earlier controls stay unchanged. The4301 original leafy shoots remain
separate; no global self-intersection, union occupancy or native Float32 proof.

All207 current library Python module files match the fresh installed wheel;
installed runs exclude `lib/src`. SHA256:
856db44125029baf3c67fa153d32ec36babd5a71867bb33e3c92c4209c7d9af1.
Source/installed full mesh/row checks and saved-file hashes agree exactly.

| Work / retained payload | Segmented | Continuous |
|---|---:|---:|
| Structural body faces |175072|195888|
| Structural cap triangles |87536|20816|
| Body vertices |98478|100546|
| Complete wood faces including cached shoots |312704|333520|
| Whole coarse instance triangle references |3372704|3393520|
| Complete wood arrays B |45811136|49963376|

The extra20816 triangles are.6172% of the old whole instance count; doubling
axial side sampling pays for curved sections while cap count falls. Output
includes1334080B UInt32 limb provenance. Known position/face/normal aggregation
copies32286816B payload, at least twice that for reads+writes; UVs/owners,
compression, curves and internal allocator traffic are additional, uninstrumented.
Face capacity is for the structural body; it is not a claim that complete wood
fits262144 faces or that runtime work shrank.

Frozen build/render6a34d5b8: source decode/plan.194s, new body2.981s,
export1.034s, process peak246837248B. Fresh finished pair decode.477s source/
.572s installed; separate full checks16.773/20.744s, peaks447418368/447664128B.
Verifier eee16a38a includes all topology/binding checks. Whole setup/render
4.094/52.888s; near2.709/57.547s, peak3493298176B. Linux/WSL x86_64,4CPU
affinity, Python3.13.5/NumPy2.5.4, one numerical thread, nice10 under shared load.
EEVEE4 whole/8 near samples, confirmed Mesa llvmpipe/LLVM19.1.7. No timing
ranking, GPU/frame/A18/RTX budget, native/world or runtime-cache-format admission.

## Reproduction

Use `.venv`, `TMPDIR=/tmp`, no bytecode, one BLAS/OMP thread and
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`. Build with
`oak_skin_experiment.py --inputs <prepared-685-root> --output /tmp/<fresh-skin>`;
render with `render_spatial_crown.py --inputs <skin> --output /tmp/<fresh-whole>`
and `oak_scaffold_near.py --inputs <skin> --output /tmp/<fresh-near>`.
Run `oak_skin_replay.py --inputs <skin> --output /tmp/<replay>.json` and
`source_limb_bend_probe.py --inputs <prepared-685-root> --output /tmp/<probe>.json`.
Original inputs, current PNGs, frozen logs and the rejected bend probe remain
under `/tmp/outshine-vegetation-oak-skin-proof-c759d37d2`. No server, published
images, vendor asset or native implementation is added. Local fork blends and
the much broader mature-form/species/stand requirements remain open at #682.

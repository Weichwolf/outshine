# Attached short shoots

Owner [#746](https://github.com/Weichwolf/outshine/issues/746), part of [#52](https://github.com/Weichwolf/outshine/issues/52).
`outshine_ref.vegetation.TwigClusters` promotes the existing checked
[attached-shoot method](../test/experiments/vegetation/twig_clusters.md).
The [mature source comparison](../test/experiments/vegetation/attached_crown.md)
shows its actual form improvement and costs. This module move adds no quality,
species, cache-format, native or device admission; existing recipes remain exact.

## Inputs and ownership

Provide sorted unique nonnegative `host_ids`, corresponding `host_counts`, the
complete canonical `variants` sequence, host `origins` and proper rigid `frames`,
a64-character `source_identity`, seed and explicit physical recipe parameters.
Coordinates are metres in one caller-owned plant/world-local frame; organ local
X is across/Y along/Z normal. The caller owns source/frame/reservation binding.
The class copies numeric inputs into immutable arrays. Unchanged versions:
`twig-clusters-1`, NumPy recipe version and original identity hashing.

Limits:1–65536 hosts,0–100000 leaves, at most128 leaves/32 children per host,
at most16384 complete children; leaf_limit4–16 and16 finite variations.
Host IDs are below2^58. Child ID=hostID×32+ordinal; canonical leaf IDs are prefix
indices0..N−1 in the supplied source sequence, with variants unchanged.
Invalid counts/frames/radii or excessive complete hierarchy refuse before detail.

## Demand and hierarchy

- `summary()`/`offsets`/`identity` need no leaf placements.
- `twigs(host_id, max_twigs)` emits bounded physical child roots/frames, IDs,
  counts, variation keys, parent parameters and radii; insufficient capacity
  returns None before child work.
- `group(host_id, max_leaves, reuse=True)` emits leaf IDs/variants, roots,
  proper frames, parent nodes and twig IDs. `tables(max_leaves, reuse=True)`
  assembles canonical whole order; both preflight their leaf count.
- `shoot(count, variation)` returns an existing Shoot recipe.
  `prototype(count, variation)` lazily caches local placement rows.
  At most16×leaf_limit prototypes are possible; requested host groups are cached.
  `reuse=False` retains the independent scalar path. A complete tables request
  can retain both host groups and whole copies; it is not a zero-copy renderer.

The class preserves the existing authored stratified carrier attachments,
inclinations, phyllotaxis, length variants and radius rules. It supplies actual
connected centreline hierarchy, not welded collars or a biological growth model.
New spatial recipes have their own identities; camera/request order does not
change the plan. Consumers control visibility, LOD, shading and retained detail.

## Curved tube radius basis

`outshine_ref.vegetation.twig_radius_basis` exposes `radius_basis(length, bend)`
and `apply_basis(basis, radii)`. This is the existing exact affine factoring of
the circular-axis kernel for fixed3 longitudinal segments,4 sides and tip/base
radius ratio .3. With fixed centreline C, vertices are C+rV; unnormalised side
normals are N0+rN1, normalised after evaluating the actual radius. Caps/topology
remain the original scalar kernel. No radius quantisation or thickening.
Each radius must be finite and0<r<=length/8. This bound is not a general tube,
surface regularity or adaptive-LOD admission. Basis arrays belong to the caller.

## Verification and unchanged limits

Independent library tests cover attachment equations, rigid equivariance,
petiole anchors, remainder/null cases, stable IDs, finite prototypes, input
ownership, early refusal and positive/negative/zero-bend scalar radius controls.
Experiment tests retain whole wood batching/source-preservation checks.
`twig_library_replay.py` is a standalone installed client for all16120 children,
32 prototypes,90000 leaves and96 actual radius cases against the previous
complete products. It needs no experiment implementation on its import path.
Full source/installed/rebuilt-product/image evidence belongs to the PR.

The promoted core bodies are AST-identical to the checked experiment except
imports. All consumers now import this implementation; old implementation files
are removed. Existing renderings/measurements remain scoped to their exact
products, not to newly claimed worldwide performance. Generic lamina/materials,
bark, mature forks, forest/temporal LOD, wind, shadows and native/device budgets
remain open. Library promotion does not change the compact-versus-mmap result.

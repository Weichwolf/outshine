# Strict surface coverage through an oriented boundary (#678)

The normal `SurfacePatch` constructor now tries a sufficient exact-edge proof
before general GEOS coverage validation. Same public API, geometry and invalid-input
requirements; no persisted validity flag, trusted-input bypass or SDK dependency.
This is a partial delivery for [#678](https://github.com/Weichwolf/outshine/issues/678),
which still owns avoiding repeated prepared-load composition/validation work.

## Method and sources

SIGGRAPH first: Lipman, [Bounded Distortion Mapping Spaces for Triangular Meshes,
2012, section5/theorem5.1](https://www.wisdom.weizmann.ac.il/~ylipman/BoundedDistortion/bounded_distortion_may_1.pdf),
PDF SHA256 `fa15ab2f209cd26ec4c4f03cd8bed0e48e2896e98d9eb24ea066da922dfee124`.
Then [Bijective Mappings of Meshes with Boundary and the Degree in Mesh Processing,
2014, theorem1/figure1](https://www.wisdom.weizmann.ac.il/~ylipman/2014_SIIMS_bijective_93975.pdf),
SHA256 `86146ec9c3cfe38a5315b0581fbe4921b400bcb9850d532db5b8e5980816fca7`.
PDF pages5and4 and their figures/equations were inspected visually. These are
mathematical references, not copied implementations or automatic manifold proofs.

All existing finite-coordinate, bounded-mesh, index, upward-face and exact
single-height-per-XY checks still run. Reuse their canonical XY/index mapping.
Sort all three directed edges per triangle by exact unordered endpoint IDs;
internal edges must occur twice in opposite directions. The remaining edges
must form exactly one simple closed CCW cycle. GEOS checks robust CCW orientation
of every triangle, in blocks of at most1024. Graph temporaries expire before
those ring clones are made. Shapes and STRtrees are still prepared normally.

Own planar-chain argument: internal oriented edges cancel. The single simple CCW
boundary has winding number one inside and zero outside. Every nondegenerate CCW
triangle contributes +1 to the summed winding number, excluding positive-area
overlap or missing interior area. Full internal edge pairing also excludes
unmatched interior edges. Positive triangle orientation alone is insufficient.
Holes, multiple/touching components, ambiguous edges, wrong orientation or any
unavailable proof fall back to unchanged general GEOS validation. Other constructor
checks, patch replacement boundaries, tolerances and attribute ownership remain.
This is neither a 3D solid/physics certificate nor a worldwide terrain proof.
[Shapely coverage semantics](https://shapely.readthedocs.io/en/stable/reference/shapely.coverage_is_valid.html)
and [orientation preconditions](https://shapely.readthedocs.io/en/stable/reference/shapely.is_ccw.html)
were checked against the primary documentation.

## Actual complete state and independent controls

Code/test/wheel commit `072e06c9372fda65e9594f37e2d7a0d22935754e`.
Use the [verified complete78-source snapshot and79th-source recipe](stair_resident_snapshot_comparison.md).
Input manifest SHA256:
`4e0c339d98df06e77cf1aecbf3f5c0386819dba3c188cbd0d325419ba2a9a917`.
All15stored patches/37645triangles are read, including inactive history.
13patches/34394triangles satisfy the proof; two/3251 still use general GEOS.
The31058active XYZ facets/attributes, masks, IDs, road/profile metadata, whole
supports/caps and logical headroom remain exact. Complete-state SHA256:
`09343d36a760a2cb8e6647287cf99e0ab0cd8ee9b2048ac3b4af2c796981891d`.

87source and87installed controls pass, including the13shared patch controls and
10new coverage controls. Independent GEOS and exact rational orientation cover
overlap, contained/duplicate faces, T-junctions, degenerate faces, holes, touching/
disconnected components, a twice-wrapped positive fan,32random triangulations and
their corruptions, a >3072-face case and256nearly collinear cases. Actual115reference
imports per control mode resolve only to the selected source/installed root and
match current module bytes. The newly built wheel SHA256 is
`7c0ce9ff4de8145dadff0297e3e43027716e3f7ab6b420100fde76990e18d571`.

Source and installed runs re-create the actual whole952-m Radweg with the new
normal library predicate, then cold-verify it with the general GEOS path forced.
All seven road/terrain/planning/catalogue artifacts are byte-identical to the
original79-source product.361points per precision give identical index/scan
queries and Double/Float32 heights/normals:9228index versus19334974scan face tests.
The installed generation/reader imports52current reference modules from its wheel.
Fresh actual79-source consumers agree in both modes:6648physical/logical spatial
volumes,124whole supports,128probes,65building bodies and4complete plants.
Each full consumer closure contains116current matching reference modules.
No original receipt or consumer product is overwritten.

The existing actual whole1280×720overview was opened again. Identical generation
artifacts preserve its geometry; no new visual improvement or new render is claimed.
Weak contrast, sparse source coverage and coarse terrain remain. No public image,
gallery or server is added.

## Equal-input cost comparison

Python3.13.5, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1, Memray1.20.0.
WSL2/i5-1135G7, four exposed logical CPUs/16190028KiB system RAM. Shared host load
includes other Buildings tests/renders. Measurements run at normal priority;
own controls/consumers run at nice10. No isolated or device timing claim.
Wheel construction temporarily used setuptools84.0.0; no new runtime package.

Both methods load identical whole state and152NPZ arrays/1894047materialized
input bytes. IO, hashing, decoding, immutable copies, trees and patch replay count;
imports, preparation, result hashing and source-profile recovery do not.
Baseline forces only the sufficient proof to return false; it incurs no unused
canonical-index copy. Small work counters are instrumented in both processes.

| Method at072e06c93 | Profiled s | Positive allocations | Requested cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| General GEOS |7.654046|16602463|2503153449|21641563|131235840|
| Normal library + general fallback |3.733386|6634590|933889898|21672830|128139264|

60.0385%fewer positive allocations and62.6915%fewer requested bytes. Tracked peak
**increases31267B**; RSS variability proves no RAM saving. A nonincreasing-peak
diagnostic was false, not silently accepted. These sums are allocation traffic,
not copied bytes or memory-bandwidth measurements; full copy traffic is unmeasured.
The canonical-face copy explicitly materializes903480B across all15patches.
The112935directed edge records are sorted; array temporaries and1024-ring blocks
remain real work. Peak accounting excludes OS/driver/GPU allocations.

Alternatives at36309b6c0 on the same exact state: polygon-coordinate reconstruction
requested952463838B/27726127Bpeak; reusing the mesh requested936356634B/26263255Bpeak.
The peak counterexample is retained: cloned exterior rings alone held6633632B.
Reuse already computed XY/indices, bound ring batches and end graph lifetimes
before orientation rather than keeping these adapters in the normal constructor.
No global optimality or uninstrumented speedup is claimed.

## Reproduction and remaining boundary

Use fresh explicit `/tmp/outshine-*` outputs. Set `TMPDIR=/tmp`,
`PYTHONDONTWRITEBYTECODE=1`, one BLAS/OMP thread and the normal experiment PYTHONPATH.
Run `stair_coverage_boundary_study --products SNAPSHOT --coverage general|library
--output REPORT [--profile TRACE]`. Other adapter modes remain comparisons.
Run `stair_coverage_boundary_verify --resident SNAPSHOT --reference ORIGINAL79
--fixture fixtures/stair-pier-detour-vienna.json --output FRESH_DIRECTORY`;
the fixture path is relative to the transport experiment directory. It defaults
to the normal library path and forces general GEOS in the independent cold reader.
Run `unittest test_surface_patches test_surface_patch_coverage` and the affected
transport/consumer controls. Build/install a wheel separately for installed checks.
Re-run the existing occupancy, Buildings and Plants readers on actual79products.

Original inputs/products remain at the preceding snapshot report's private roots.
Current receipts, wheel/import closures, raw cost traces, PDF inputs and compact
`final-evidence.json` are under `/tmp/outshine-transport-topology-81510acfd/`.
Repeated patch composition/union, sorting, array copies and preparation still cost
work. #678 therefore stays active: immutable prepared reuse is not solved by
this constructor optimization. No native cache, frame/device, full world-loading
or worldwide transport admission follows. All previous source-coverage and
spatial-precision limitations remain; master owns native integration.

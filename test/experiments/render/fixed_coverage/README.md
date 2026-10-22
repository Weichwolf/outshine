# Qualified patch coverage (#990)

This native experiment addresses the artificial internal-edge holes demonstrated
by `../conservative_coverage/run.py --tessellated`. It erodes a supplied complete
convex patch once, rather than eroding each of its constituent triangles. It is
a calibration/reference experiment, not an asset-page selector or a mesh-to-patch
algorithm. Product rendering, cache formats and shared GPU bindings are unchanged.

[UNCERTAINTY.md](UNCERTAINTY.md) adds interval-valued projection, an exact erosion
reference, an independent endpoint oracle and a bounded integer constructor. The
runner always includes their 20 fixed analytic/adversarial cases. Those cases
remain unqualified for page suppression, as do optional external inputs.

## Domain and construction

Each input is four finite IEEE binary32 clip-space corners of one completely
opaque, complete convex projected patch. Two triangles covering that quadrilateral
may have different depths: the minimum of all four reverse-Z corner depths is a
safe, deliberately loose bound for either triangle. Alpha-tested surfaces, holes,
unknown material coverage and missing geometry cannot be declared complete.
The caller must qualify these properties; two booleans do not establish them.

`Exact.h` decodes binary32 bits into GMP rational numbers, including subnormals.
Projection and viewport clipping use exact rational arithmetic. Eye/near/far-plane
crossings are rejected rather than reconstructed. Coordinates outside the viewport
are clipped to its rectangle. The resulting convex polygon has at most eight
corners. Quantization moves its vertices inward and verifies every grid vertex
against the original source half-planes; failure returns unknown. The convex hull
of these verified vertices is therefore contained in the supplied patch.

The grid scale starts at eight units per pixel and decreases until both viewport
dimensions fit in 16,384 grid units. For a counterclockwise grid edge `d = b-a`,
the minimum edge function over a whole pixel with grid-space upper-left corner `p`
is

```text
d.x*(p.y-a.y) - d.y*(p.x-a.x) + min(0,d.x*scale) + min(0,-d.y*scale).
```

Every edge must have a nonnegative minimum. Draw viewport and patch dimensions
must match. The CPU oracle evaluates this with
int64 arithmetic; the fragment shader uses int32. Grid differences are bounded by
16,384 and scale by eight, so the absolute intermediate bound is at most
`2*16384² + 2*16384*8 = 537133056 < INT32_MAX`. Header layout/offset assertions
and shader input checks preserve this restricted domain. CPU profile checks include
the maximum grid size; GPU calibration is specifically 64×64, not a 16K claim.

The reverse-Z depth bound is rounded down and compared against the exact rational
value. Zero and subnormal output depths are rejected to avoid an implicit GPU
denormal-mode requirement. Each accepted fragment writes the same bound to R32F
and D32F, with greater-than depth testing and a zero clear. Consequently overlapping
patches compose by maximum safe depth; draw order does not change the result.
Rasterization may miss fragments, but the integer fragment predicate determines
whether a generated fragment may claim whole-pixel coverage. Floating vertex
rounding is not used as an acceptance proof.

## Portable input and remaining qualification

`test/outshine/data/render/fixed_coverage/native-opaque-pairs.json` contains 128
adjacent triangle pairs from a recorded road-free native root on commit
`984ebe97ef7719b5543adf01b7e2515cef2cd2fc`, ground recipe 85. It includes original
local positions, current float32 placements, camera matrix/shift and derived clip
coordinates. The recorded source had 177,894 GPU-selected triangles in 1,369
framebuffer-frustum-visible opaque batches. Raw stored meshes had 480,516 triangles
and included unselected LODs; they are not substituted for selected geometry.
Pairs share an exact position edge with opposite winding and have convex projected
perimeters. Candidate ordering uses projected bounding area and retains at most 128
valid front-facing pairs. This limited greedy selection is not a root representation.

The source acquisition completed offline with zero road LoadKey/Resolve/asset loads
and zero provider/network starts, no pending world candidate, frame serial 1.
Provenance hashes identify the diagnostic acquisition source, binary and original
capture. Acquisition was a one-frame diagnostic of that frozen world, not a loading
benchmark. Original captured inputs remain in the associated temporary evidence.
The JSON is sufficient to replay the supplied clip-space calibration independently.

**The CPU emulation of the native vertex transform is not proved identical to the
actual GPU transform.** Vertex deformation, world/root/camera/light lifetime,
patch extraction/opacity qualification and all page-demand consumers remain open.
Terrain 91 has different real geometry; no recipe 85 capture proves its coverage.
The input and every output explicitly say `qualified_for_page_suppression=false`.
They must not authorize dropping a page. Small patch coverage alone does not prove
useful coverage of the complete root or avoided IO. GMP preparation is a reference
cost, not an approved runtime procedure; its allocations and peak memory are not
yet instrumented.

## Reproduction and checks

The optional experiment needs the system GMP development library (`libgmp-dev` on
Debian, `gmp` on Homebrew); it is not a new product dependency. Supply an already
built native archive and its actual compilation database, with that build’s
SDK/library environment active:

```sh
python3 test/experiments/render/fixed_coverage/run.py \
  --native-checkout /path/to/prepared/native/checkout \
  --compile-database /path/to/that/checkouts/compile_commands.json \
  --cases test/outshine/data/render/fixed_coverage/native-opaque-pairs.json \
  --output /tmp/outshine-fixed-coverage-unique-run
```

The runner preserves the native compilation/link flags and Metal validation setup.
It records source/archive/input/binary hashes, commands and terminal results.
Every case runs in both draw orders with R32F and D32F readback. Exact source
geometry checks accepted whole pixels and depth; a separate int64 grid oracle
checks the expected mask and hardware depth maximum. Closed patches, adjacent and
overlapping patches, a real hole, nonbinary projective coordinates, eight-corner
clipping and adversarial finite/nonfinite/degenerate inputs are included.
Zero-coverage rejection cases assert the expected rejection reason as well.
Preparation time, optional GPU timestamps, draws, triangles, logical uniform bytes
and readback bytes are reported separately. No scene frame-rate claim follows.

## Primary references

Durand et al., SIGGRAPH 2000, *Conservative Visibility Preprocessing using Extended
Projections*, sections 3.1–3.2: conservative depth bounds and erosion of complete
occluder polygons. The local paper and visually inspected diagrams are recorded in
`../conservative_coverage/README.md`.
[Author publication page](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm).
[GMP rational arithmetic](https://gmplib.org/manual/Rational-Number-Functions) supplies
the exact CPU reference operations. Neither source establishes native GPU vertex
transform equivalence or production page-admission correctness for this experiment.

# Native opaque patch candidate selection (#990)

This experiment compares two greedy ways to pair adjacent source triangles before
running the exact-input whole-patch calibration in `../fixed_coverage/`. The original
first-shared-edge method often pairs across a building corner while consuming a
triangle that could have formed a larger planar patch with its other neighbor.
Prefer the neighboring face with the most aligned normalized local-space normal
instead. The three-face self-check reproduces this concrete ordering failure.
This heuristic is not an optimal matching or a general improvement guarantee.

## Source preservation and qualification

Positions are welded only when all three original float32 bit patterns match.
The pairing library sorts undirected edge records; only groups of exactly two
oppositely directed source edges can pair. Nonmanifold groups and inconsistent
winding remain unpaired. Each face is used at most once. The probe independently
checks every emitted pair against the original oriented triangles, including the
opposite shared edge and unused source-face identity.

Normal alignment and approximate clipped area rank candidates only. They establish
neither coverage nor opacity. A candidate still needs a strictly convex projected
perimeter, valid clipping/depth and the exact rational/integer qualification from
`fixed_coverage`. A pair across a fold may be accepted only if its two real source
triangles actually cover that projected quadrilateral. No hull spanning a missing
source triangle is synthesized. Unknown candidates contribute no coverage.

The fixture is assumed fully opaque and uses the recorded native backface rule.
Opacity and CPU/GPU transform equivalence are not newly proved here. The probe's
CPU float matrix evaluation is a reference, not the actual GPU vertex result.
Both input and generated cases explicitly forbid page suppression. The experiment
changes no product renderer, asset DB, shader binding or runtime demand consumer.

## Discarding packets with no depth contribution

A fresh 64×64 reverse-Z envelope examines the exact whole-cell predicate of each
qualified packet. It scans an outward-rounded grid bounding rectangle and retains
only packets that increase at least one cell's depth. Empty packets contribute no
whole cells; dominated packets contribute no larger bound than earlier retained
packets. The CPU mathematical depth maximum is unchanged by induction. The fixed
state is 16,384 bytes, with no explicit heap allocation; cell tests and depth writes
are counted. This filter changes only the diagnostic coverage pass, not world PBR
geometry, shadow rendering or source content. It is one greedy pass, not a minimum
occluder-set solution.

The runtime shader uses the same bounded integer predicate. GPU raster completeness
is not newly established for arbitrary profiles/devices by the CPU envelope proof.
Dropping an occluder remains conservative even if hardware misses a fragment; it
can lose usefulness. The runner therefore also compares actual R32F and D32F
buffers before/after reduction, byte for byte, on the executing device. Device-
independent output equality or production page admission is not inferred from one
counterexample. Geometry/projection/opacity qualification remains a separate duty.

## Actual native counterexample

`test/outshine/data/render/patch_selection/native-mesh-subset.json` contains 17
complete GPU-selected meshes with 17,536 triangles, their original local positions,
current float placements and recorded camera matrix/shift. They originate from
road-free ground recipe 85 at `984ebe97e`, frame 1, no pending world candidate.
The original capture had 177,894 selected triangles in 1,369 opaque batches; road
LoadKey/Resolve/asset loads and provider/network starts were zero. Its hash is
recorded in the fixture. Original raw inputs remain in the associated evidence.

The fixed subset is the union of complete meshes contributing to the previously
recorded 128-pair controls. It is intentionally biased toward this known failure,
not a geographic/general admission corpus. Every selected source face remains in
its original order; all faces of those meshes, including competing neighbors, are
retained. Both policies receive exactly the same input and the same 128-patch cap.
The JSON is sufficient to reproduce pairing, CPU projection and GPU calibration
without any private capture directory.

On Iris Xe the first-edge reference retains 93/4096 cells; aligned normals retain
316/4096 cells, 3.40 times as many. Both use 128 patches/256 draws in the two draw orders
and 40,960 logical uniform bytes. The supplied subset does not establish complete
root coverage.
The reduced aligned case keeps 43 packets and the same 316 cells: both its R32F and
D32F buffers are byte-identical to the 128-packet case, including reverse ordering.
In the two draw orders this avoids 170 draws and 27,200 logical uniform bytes
(256→86 draws, 40,960→13,760 bytes). The first-edge case independently drops 128→24
packets while preserving all 93 cells and both buffers.

These are whole-cell coverage counts and actual avoided draws in the isolated
probe, not avoided asset reads, rendered world quality, FPS or a demonstrated
loading improvement. The 4096-cell grid also includes sky and other root classes;
it is not a denominator of expected opaque-root coverage. Terrain 91 has different
real geometry;
recipe 85 inputs do not prove it. Shadow/reflection/game/change demand and
world/root/camera/light lifetime remain open in #990.

Earlier bounded exploratory alternatives on the full 177,894-triangle capture did
not justify a more complex maintained path: clipped-area ranking alone retained
93 cells; a boundary-halfplane/seed/depth construction from connected subsets
retained 291 cells, or 284 after combining transform-identical opaque batches.
Aligned pairs after that combination retained 311 cells. These rejected prototypes
and original outputs remain local evidence; they are not production algorithms or
independently certified mesh-union implementations. The maintained comparison is
only the two source-preserving pair policies above.

## Projection uncertainty comparison

The aligned candidate ranking is also shared by two interval-valued controls:
exact rational erosion and fixed-width integer certification. Both use the
projection envelope from `../vertex_projection/` with its explicitly restricted
arithmetic model. Construction, independent oracle, supported domains and open
producer contracts are described once in
[UNCERTAINTY.md](../fixed_coverage/UNCERTAINTY.md).

On the same subset they retain 308 and 310 cells respectively, versus the
exact-input reference's 316. Both reduce 128 packets to 42 without changing their
own R32F/D32F buffers. This compares different safe interior proposals; the integer
method is not an exact implementation of the rational erosion polygon. Unknown
inputs remain needed. Preparation and ranking are reported separately, and no
world-load or page-admission claim follows from these isolated controls.

## Costs and reproduction

The selector records edge records, sort comparisons, neighbor candidates and
explicit allocation requests inside pairing. Normal vectors add one allocation per
nonempty mesh. Capacity bytes are logical capacities: the reported scratch sum is
an upper sum across buffers with different lifetimes, not an OS/GMP/GPU peak.
Candidate storage/sort work and CPU input/welding/projection/selection time are
separate. JSON, canonicalization-map, candidate-growth, GMP and driver allocation
counts/peaks are not fully instrumented. No app-memory admission follows.

Use the prepared native build's SDK/library environment. System GMP is needed only
for the existing exact reference; it is not a product dependency.

```sh
python3 test/experiments/render/patch_selection/run.py \
  --native-checkout /path/to/prepared/native/checkout \
  --compile-database /path/to/that/checkouts/compile_commands.json \
  --output /tmp/outshine-patch-selection-unique-run
```

The runner records commands, source/input/archive/binary hashes and terminal
results. It builds only the selector and the existing isolated native GPU probe
against the prepared archive. Self-checks cover the bent-neighbor ordering failure,
nonmanifold adjacency, inconsistent winding, invalid indices, empty input and
nonfinite positions. Envelope checks cover repeated/nearer/farther depth, an
empty subpixel patch, invalid profile and nonfinite depth. Every actual pair is
checked for source topology preservation.
The eight original/reduced cases run with the original 37 synthetic GPU cases,
20 uncertainty fixtures, eight CPU grid-profile checks, exact source/grid/D32
checks and reverse draw ordering.
`reduction-equivalence.json` records the actual original/reduced buffer comparisons.
Images and detailed evidence stay in the explicit temporary output directory.

The conservative-occluder basis remains Durand et al., SIGGRAPH 2000, sections 3.1–3.2
and the visually inspected figures documented in `../fixed_coverage/README.md`.
Normal alignment is a candidate heuristic; no paper or this fixed counterexample
establishes its global optimality or runtime-world performance.

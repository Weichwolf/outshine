# Joined roof coverage and partial emission

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12). This continues the
[source/roof-plan study](hierarchical_selection.md). The complete whole/cell/
hierarchical planning and world acceptance remain open. Four fewer experimental
roof triangles do not justify native adoption of this expensive selection path.

## Geometry and primary sources

SIGGRAPH-first basis: Durand et al.,
[Conservative Visibility Preprocessing using Extended Projections](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm),
updated paper sections 3.2/3.3, pages 3–5 and diagrams actually opened. Separate
triangle umbras can lose connectivity along shared diagonals. Our exact
height-slab construction is an independent derivation, not their projection-map
implementation. Lee/Preparata's
[polygon-kernel paper](https://www.cise.ufl.edu/~sitharam/COURSES/CG/kernel_preparata.pdf)
(JACM 1979, first two pages/diagrams checked) supplies the inner-halfplane
characterization. We do not implement or claim its optimal linear algorithm.

`roof_fusion.py` consumes explicitly opaque **accepted and retained** seed
triangles in their represented coordinates. It does not infer opacity from
bounds, ShellError or a filled hull. Rational arithmetic checks positive
projected triangle orientation, cancellation of interior directed edges, and
one simple projected boundary with at most eight edges. Duplicate/nonmanifold
edges, self-crossings, holes and disconnected patches fail the group; the
caller retains individual certified triangles instead.

The positive triangle chain has the winding of its simple boundary and covers
its projected interior once. Shared 3D edges agree in height, giving a complete
continuous height graph. The intersection of all inward edge halfplanes is its
convex **inner kernel K**, contained in that interior. Concave notches are not
filled. K can be empty; a valid certificate does not promise a nonempty useful
umbra. No kernel polygon or proxy mesh has to be emitted.

Let `w(x)=n·x+d` be the exact affine chart height and `l/u` the minimum/maximum
vertex heights. The graph stays inside that slab by interpolation. Every
observer-box corner must satisfy `w(e)>u`; a target satisfies `w(p)<l`. For every
kernel edge inequality `h(x)>=0` and both slab levels `s in {l,u}`, require

```text
(s - w(p)) h(e) + (w(e) - s) h(p) >= 0.
```

Dividing by positive `w(e)-w(p)` gives the ray section's boundary test at height
s. Convexity keeps the entire slab segment inside K, hence inside the covered
roof domain. Above/below signs and continuity give a front-facing intersection
with the actual roof graph. Each test is affine in eye for fixed target and in
target for fixed eye. Eight observer corners and the existing conservative
AABB queries suffice. Rational coefficients receive outward Float64 brackets.

Caps: 256 triangles per group, eight boundary edges, 129 halfspaces and the
shared local-coordinate/query limits. Replacing a group is conservative, but
is not claimed to dominate every original single-triangle certificate. This
reference supplies no native/GPU arithmetic proof. Geometry, representation,
material and transform changes invalidate reuse even inside the observer box.
Logical sources, contacts and shadow/reflection/interaction demand remain
independent of camera-primary removal.

## Controlled vertex representation

`building_roof_positions.py` keeps the earlier Float64 affine recipe as default.
A separate `snapped-f32` recipe models native millimetre ties-away rounding,
ordered affine placement and Float32 storage, then lifts those values exactly
for geometric testing. Explicit scalar plane evaluation keeps contour/triangle
batches consistent at rounding ties. This is a declared representation variant,
not an epsilon weld added merely to make a coverage union pass.

`verify_native_roof_positions.py` compares original singleton Shell controls
against SHA-bound actual native artifacts (StoredVertex stride **24 bytes**).
Parent bbd9aab743, source ordinal 141, cell 14 reproduces all **18 oriented roof
triangles / 14 positions exactly**. Source 36 has no supported planar roof and
remains an unsupported control. This proves one geometric case, not universal
mesher, material/attribute or GPU-transform parity. The #801 positions and this
comparison use the documented parent-offset ECEF frame.

## Real comparison and decision

The compiler chooses the **32 nearest known source envelopes by metadata**,
identically for both position recipes. Every seed source is retained. Both
single and joined variants consume the same complete accepted seed geometry;
group concatenation is checked elementwise against it. Fusion tries individual
pieces, then adjacent pieces of a shape, then adjacent shapes of a source.
Rejected groups fall back to their smaller certificates. No complete Fine mesh
is generated as a prior reduction input.

| Recipe / parent | Sources / roof parts | Single / piece / joined certificates | Hidden whole sources | Hidden contour plans |
| --- | --- | --- | ---: | ---: |
| Float64 / bbd9aab743 | 457 / 1,638 | 296 / 189 / 185 | 0 | 0 |
| Float64 / 6d41e63e73 | 1,754 / 5,794 | 343 / 229 / 227 | 0 | 1 |
| Float64 / 035e2150a3 | 4,141 / 11,392 | 110 / 54 / 51 | 0 | 0 |
| Snapped Float32 / bbd9aab743 | 457 / 1,638 | 293 / 187 / 181 | 0 | 0 |
| Snapped Float32 / 6d41e63e73 | 1,754 / 5,794 | 343 / 229 / 227 | 0 | 1 |
| Snapped Float32 / 035e2150a3 | 4,141 / 11,392 | 110 / 54 / 51 | 0 | 0 |

`building_roof_part_selection.py` compares world AABBs with original contour
support tests. Its two-million box/point-test limit retains unproved parts on
exhaustion. The 6d41 single-triangle contour run exhausts that cap in both
recipes; piece/joined runs finish. This is a bounded partial result, not an
exhaustive single-triangle comparison. Plane counts per test vary; fewer
certificates or finishing this cap does not establish lower CPU/device cost.

All three methods find the same one candidate: parent 6d41e63e73, source 1517,
cell 17, shape 0, band 0, area 115.54129065 m². `roof_part_comparison.py` emits
actual candidate triangles **after** selection, checks original vertex
provenance, advertised-bound containment and an actual certificate witness for
all emitted vertices. It then independently emits the complete source control
and the selected parts. Both recipes retain all logical source records and emit
**16,196→16,192 roof triangles**, including four genuine nondegenerate removed
triangles. Eight fixed-camera/limited-movement images per recipe preserve exact
1280×720 depth; six views are nonempty and two empty. This is roof-only software
geometry, not native Fine/world/shadow acceptance or the continuous native turn
gate. There is no additional primitive saving attributable to fusion here.

The practical result remains negative: no whole source is avoided, and extensive
clipping/query work saves only four roof triangles. No native algorithm is
recommended from this comparison. Single Python diagnostic runs include
compilation/query counters, without isolated time, allocation/copy/peak/scaling
or device-budget acceptance. The current proposed receiver is the explicitly
accepted Shell-wall contract in
[#12/6256008442](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6256008442),
which still requires actual retained triangles and consumer-transform bounds.
This study does not admit real walls or Fine openings as opaque by itself.

## Tests, positive fixture and reproduction

26 focused roof-domain/fusion/position checks pass. Exact segment/triangle
oracles cover interior observers, warped/interior-vertex graphs, rotations and
negative dominant axes. Counterexamples include real courtyards, a 2^-42 m
crack, a concave notch, a self-crossing positive triangle chain, duplicate or
reversed faces, unknown opacity, observer crossing/backside, numeric ties and
budget exhaustion. New fusion code remains experimental, not an installed
library feature; the consumers also run against the unchanged isolated common
library wheel.

The analytic roof/cuboid fixture emits its accepted roof seed first. Independent
triangle selection emits 14 triangles; joined selection emits two. All nine
observer corner/centre poses preserve exact 1280×720 colour/depth. With the roof
removed, 1,849–2,601 target pixels appear. Full/joined/target-only panels were
opened locally; this is not one of the geographic load cases.

```sh
PYTHONPATH=lib/src:lib/tests nice -n 10 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p 'test_*roof*.py'
PYTHONPATH=lib/src .venv/bin/python \
  test/experiments/render/building_roof_fusion_selection.py \
  --input "$native_surface_file" --vertex-recipe snapped-f32 \
  --output "$fresh_temp_result"
PYTHONPATH=lib/src .venv/bin/python \
  test/experiments/render/verify_native_roof_positions.py \
  --native "$native_selection_receipts" --surfaces "$native_surface_receipts" \
  --products "$original_frozen_products" --output "$fresh_native_control"
```

Private root: `/tmp/outshine-research-selection-930392b72/`. `fusion-recipes/`
retains six terminal runs, source snapshots, input hashes and complete counters;
`roof-part-emission-{f64,snapped}-6d41/` adds actual emission/coverage images and
candidate witnesses. `native-roof-position-controls/` retains the native
geometric control. Fixture/images and the actual scoped check receipts remain
local. No presentation images or native/cache/shader/GPU contracts change.

Two additional original geographic packages are frozen read-only: Flensburg
parent a169a8d0d6 (3,043 sources/64 form blocks) and Koerbersee 87b5710776
(14/11), each with its own original cached camera. Package SHA, exact index-snapshot
identity and complete source-cell sets pass under `geographic-inputs/`. A park-edge
classification is not inferred from building density. The recorded #801 exporter
attempt fails before main because LIBTIFF_4.7.1 is missing; binary SHA/command/exit
are under `geographic-surfaces/`. No new surface JSON is claimed. The native owner
has the minimal relink request after #804; independent research continues. The
three existing real parents still share Wien's camera and do not complete the
required geographic/whole-world gates.

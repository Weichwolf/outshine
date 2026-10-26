# Positive water-source membership without overlay construction

Owner: [#970](https://github.com/Weichwolf/outshine/issues/970).
This extends the [exact area reference](water_exact_membership.md) with a cheaper
predicate for **positive intersection area**, including containment. It returns
source membership only. It neither computes area weights/representative ranking
nor proves whole-face coverage, water compatibility or native admission.

## Why a zero-result fallback is insufficient

The unchanged frozen native source set contains48 original polygons. Its captured
union has4parts/10holes. A newly generated GEOS3.14.1 constrained-Delaunay mesh of
that union contains2278triangles. Independent rational area evaluation finds51
numeric membership disagreements: one empty GEOS result and50already-nonempty
results missing additional real sources. No extra numeric sources occur here.
A fallback limited to an entirely empty numeric membership misses those50cases.
This is a regenerated diagnostic cohort, not a replay of a complete native mesh.
`fixtures/basel-membership-partial.json` retains one additional fixed triangle,
its original-input hash and the observed numeric/exact source sets. Test indices
are provenance only; no source/place/face ID selects an algorithmic branch.

## Predicate and arithmetic contract

`prepare_source` owns an immutable original-edge array per valid Polygon or
MultiPolygon, including holes. Sources are bounded to8192coordinates and finite
XY within±1e8m. A triangle is validated by its exact orientation, so a positive
triangle whose Float64 area underflows is not silently declared empty.

A source boundary segment meets the open triangle iff none of the triangle's
three outward half-plane tests excludes both segment endpoints and its own line
strictly crosses the triangle. These are the four separating axes for this
segment/triangle case. If any source boundary meets the open triangle, valid
source interior occupies a positive area there. Otherwise no source boundary
crosses that connected open domain, so source membership is constant throughout
it. Its rational centroid decides via even/odd ray crossings. This also covers
identical boundaries, holes and disconnected valid multipolygons.

[OBBTree, SIGGRAPH1996, page8](../../../doc/references/buildings/siggraph/1996-obbtree-hierarchical-interference-detection.pdf)
was inspected visually for the separating-axis principle. This is a smaller2D
segment/polygon predicate, not that paper's3D OBB tree or its performance claim.
The exact-classification motivation remains the SIGGRAPH2022 reference in the
parent report; no foreign code or new dependency is introduced.

Orientation differences, interval products and subtraction are rounded outward
with binary64 `nextafter`. Products consider all four interval endpoint products.
Only an interval strictly above/below zero certifies a sign; uncertain cases use
bounded rational arithmetic over the original stored numbers. The centroid's
rational value and verified enclosure are retained for uncertain ray tests.
No unchecked rounded centroid, geometric epsilon, raster, threshold or coordinate
change grants membership. The filter requires IEEE binary64 arithmetic with
gradual underflow and no reassociation/fast-math substitution; native ports must
prove their arithmetic contract separately. The Python cancellation/subnormal
controls exercise this requirement on the measured host.

Both modes share the same operation/checked-bit budget. `exact=True` forces every
orientation through rationals and is the equivalent-output control. Bulk charges
are reserved before vectorized work; exhaustion raises without a Boolean result.
The cohort driver keeps a declined face's source list absent, marks unattempted
faces explicitly, and retains independently completed results. Source preparation,
GEOS validation/CDT/STRtree and opaque allocation costs are outside this predicate
counter. Existing face/global caps bound the finite diagnostic, not all GEOS work
or wall-clock time. No global source cache or new asset/GPU format is added.

## Evidence and cost scope

All2278source sets/native indices from the filtered predicate and forced-rational
predicate match the independent slab-area oracle. One oracle run plus eight fresh
normal/traced predicate processes have identical regenerated WKB hashes and
complete source results. The oracle's nonzero uncovered remainders remain separate;
correct source IDs are not permission to discard those residuals or relax native
coverage/admission rules. Geometry, original IDs and source payloads stay unchanged.

On Intel i5-1135G7/Linux, normal priority/shared host, three fresh complete cohort
runs measured5.241/5.276/5.436s with forced-rational orientations versus
3.627/3.758/3.712s with filtering (medians5.276/3.712s, about30%shorter).
Timing includes fixture decoding/hash checks, regeneration, source/index setup,
candidate queries, predicates and the identical numeric diagnostic. Imports,
serialization and optional plotting are excluded. This is a CPU experiment,
not native, Vulkan, frame or whole-world/device acceptance.

Both modes evaluate97698orientation predicates; rational orientation evaluations
fall97698→29264. Both perform314785boundary-box checks and277679ray-edge visits;
owned edge arrays total79328bytes. Separate traced peaks are9145743bytes for both
paths, dominated by common setup; RSS is about46MB. No memory saving, complete
allocator/copy accounting or generic speed guarantee is claimed. The area oracle
has extra outputs and is not an equivalent-output speed baseline.

Nineteen targeted tests (including the parent ten) cover independent area equality,
partial native membership, containment/identical/corner cases, holes/islands,
boundary-only contacts, winding, binary-preserving transforms, cancellation,
subnormal queries, immutable prepared data and local/global budget refusal.
The whole-cohort diagnostic was opened; it remains private.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/water "$PY" \
  test/experiments/water/compare_membership_cohort.py --method filtered --image \
  --output "${TMPDIR:-/tmp}/outshine-membership-cohort-filtered"
```

Use `--method rational` for the same-output control and `--method areas` for the
independent area/coverage oracle. Run `--trace` separately. Native source mapping,
area weights, whole-face coverage, coupled assets and Places remain with Master.

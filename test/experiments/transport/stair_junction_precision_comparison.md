# Actual junction precision and undeclared surface overlaps

Active owners: [#600](https://github.com/Weichwolf/outshine/issues/600) and
[#602](https://github.com/Weichwolf/outshine/issues/602), parent
[#434](https://github.com/Weichwolf/outshine/issues/434).
This is a reproduced failure and candidate comparison, **not an admitted
extension**. The verified [71-source world](stair_shared_root_comparison.md)
and its whole 1280×720 images remain unchanged. No visual improvement, native
integration, runtime/SDK dependency or completed contact contract is claimed.

## Fixed actual counterexample

Six original ordinary-ground sources are selected from the current resident
world. [The complete candidate recipe](fixtures/stair-six-source-contact-vienna.json)
retains the actual old root and four explicit secondary profiles. PBF SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`;
the same DEM and Vienna ENU frame as the preceding report. Original source
geometry, IDs, properties, old bodies/bridges/piers and all unused sources remain.

The [small five-vertex owner cell](fixtures/stair-quantized-junction-cell-vienna.json),
after limiting sections to analytic whole profiles,
has area 1.4991231045 m² and a valid Double boundary. All rounded Float32
vertices remain distinct, but the boundary becomes self-intersecting. Exact
rational orientation predicates confirm no proper crossing in Double and a
proper crossing after Float32 conversion. The original Double triangulation
also contains a face whose orientation inverts after conversion. This rules
out a triangulation-only solution **for these fixed rounded boundary vertices**.
It does not establish impossibility of other bounded constructions.

The two actual footway sources `1950369630:9` / `80922970:247` share original
node `[1512,2147]`; the first incoming/outgoing directions are nearly parallel.
Long numeric cross sections create two additional near-duplicate intersections
about 0.107 µm apart, and another 27.782 µm separation. Analytic knot normals
remove some numeric artifacts; physically bounded full profiles still leave
the genuine cell-boundary problem. Original source widths are unchanged.

SIGGRAPH starting point: Deering1995, [page3 / section4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
visually inspected: position precision depends on local modeling coordinates.
No compression implementation is used. The current
[Shapely2.2 constrained-triangulation documentation](https://shapely.readthedocs.io/en/stable/reference/shapely.constrained_delaunay_triangles.html)
requires input polygon edges to remain triangulation edges. The conclusion
about the self-crossing boundary follows from our exact predicates, not from
assuming another triangulator must fail.

## Reproducible candidate comparison

`stair_junction_precision_compare.py` uses the actual six whole sources and
unchanged old caps, and emits nine fresh diagnostic arrangements. The existing
arrangement receives an optional section provider; its default path is exact
to the original `f9918fa30` capture, including XY/faces/owners. New analytic
sections use existing complete end/interior profile primitives.

| New-node footprint | Sections | Vertices/faces | Double/Float32 XY |
|---|---|---:|---|
| original hull | legacy extended | 173/224 | rejects merged vertices |
| original hull | analytic extended | 171/220 | rejects merged vertices |
| original hull | analytic whole | 156/198 | rejects inverted facet |
| profile envelope | legacy/analytic extended | 197/263 | rejects merged vertices |
| profile envelope | analytic whole | 182/241 | rejects merged vertices |
| new level box | legacy/analytic extended | 148/175 | passes XY only |
| new level box | analytic whole | 131/153 | passes XY only |

Profile envelopes and level boxes are explicitly **different procedural
footprints**, not relaxed tolerances or equal-geometry optimizations. They add
0.000667579 / 13.956187174 m² to the full new-source union respectively;
no original union area is removed. Four new level-node shapes use the existing
height-plan boxes (maximum incident width/2 +0.75 m per axis). Old root and
secondary contact geometry remain unchanged. Every candidate has
`accepted_product=false`; no source-reuse, height, profile, world or routing
admission follows from an XY pass. No equal-quality timing claim is made.

## Independent remaining contact problem

Even before any footprint enlargement, new footway `1950369630:9` overlaps old
`1950369630:8` by **1.915864798 m²**; new `80922970:247` overlaps old
`1580280942:0` by **9.903916642 m²**. Neither pair has a shared original MVT
node. The existing exact-original-node contract correctly rejects this reuse.
The same missing areas persist with the enlarged new-node pads.

`stair_overlap_probe.py` exports actual masks, both source IDs, immutable
asset/owner bindings and intersecting whole old cap triangles. Source index,
complete scan and installed-library runs produce identical masks/cap payloads/
preconditions. Four source/asset queries return13 facets; full scan tests712;
exported arrays1976 B. Index-internal visits and total allocations/copied bytes
are unmeasured. These are diagnostic work counts, not runtime cost acceptance.

Both old regions are ground with exact0.3 m source depth. Maximum whole-old-
facet gradient Double/Float32 is0.039336045/0.039334059 and
0.000639973/0.000643865, below the new isotropic footway limit0.1. Heights span
163.872288…164.256933 m and163.866493…163.872288 m on intersecting whole facets.
These are **preconditions only**, not proved full new/old height compatibility.
Every record has `admitted_contact=false`; the original graph is not changed.
#602 owns the separate explicit procedural overlap-contact recipe, complete
profile/wall/height/clearance verification and eventual generated routing.

## Checks, reproduction and receipts

Frozen counter/probe `40e761be0`; reproducible comparison `7a69615bb`.
69 source /69 installed affected controls pass; six new controls cover exact
crossing, distinct-but-invalid rounded vertices, wrong face orientation,
candidate-only semantics, non-ground caps and excessive whole-facet gradient.
The unchanged205-module PR#592 wheel is used. All nine source/installed
candidate variants are identical, including file hashes. Full scan independently
matches the overlap index. Existing generation limits/tolerances and accepted
71-source receipts remain untouched. No library/native API change.

From the repository root, set `TMPDIR=/tmp`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/transport`. Both commands take `--resident
<53>` and ordered `--prior-module <55> --prior-module <57> --prior-module <65>
--prior-module <69> --prior-module <71>` from the linked accepted reports:

- `python -m stair_junction_precision_compare --fixture
  test/experiments/transport/fixtures/stair-six-source-contact-vienna.json
  --output /tmp/outshine-transport-<task>-<commit>/outshine-comparison`.
- `python -m stair_overlap_probe` with the same fixture/chain, `--method index`
  or `--method scan`, and a fresh explicit `--output` under `/tmp`.
- `python -W error -m unittest stair_junction_precision_checks
  stair_overlap_checks` for the fixed counters; the broader affected suites
  remain those listed in the preceding report. Use the installed target instead
  of `lib/src` for installed-library replay.

CPU90 s /AS3 GiB per new comparison/probe process; all private outputs only
`/tmp`, never `/var/tmp` or private build trees. Accepted evidence root
`/tmp/outshine-transport-junction-f9918fa30/`:
`outshine-compare-source/comparison.json` SHA256
`d29d46e2ae6a42c4d5c1f1b4dc58ca077bc2f8228ec0b57af049d2206a4aa4d5`;
indexed probe `ec2087073f3c0fad537c100158f7e17b7ae5d4aacc5f31b7c89437141098c73e`;
full-scan probe `0e7257deec523e688ab11b3feeeb0c674c996655dddc6a44c90fc228df0963cf`;
overlap equivalence `e5df6431e85139cb1038ebc02e82a59013fc3615e2399c61b440ac971ba652de`.
Both feature issues and the full world goal stay active. No new image is due
because no generated world content has been accepted in this delivery.

# Proved duplicate terrain contact removal

Owner/integration: [#1743](https://github.com/Weichwolf/outshine/issues/1743),
parent [#434](https://github.com/Weichwolf/outshine/issues/434).

The automatic six-/three-source proposals from #1738 passed their body/profile/
clearance checks but failed terrain coverage. Contact conformance left duplicated
thin triangles already covered by non-duplicate neighbours. The fixed four-point
counterexample is `test/outshine/data/transport/terrain-duplicate-contact.json`.
Both copies overlap the retained mesh; the two covering facets are jointly needed.

`stair_terrain_overlap.py` removes a duplicate group only with an exact cover proof.
All duplicate groups are excluded from possible coverers, avoiding circular proofs.
The fast certificate uses two adjacent triangles with a convex union: opposite
sides at their shared edge and all quadrilateral turns are tested on exact
binary64 rationals. A candidate whose vertices are corners of that convex union
is contained in it. Other cases use exact convex polygon subtraction. Zero
remaining area proves coverage by a finite closed union; no area epsilon is used.

Capacities:64groups,64neighbours/group, at most2016pair candidates/group,
16384halfplane clips and256retained pieces. Budget failure retains the rejection.
The existing16384vertex/32768facet limits apply. Coordinates, old assets, source
IDs, physical references and all subsequent gates remain unchanged. The procedure
only removes covered XY facets; physical heights still require the whole-field
reader. Already valid inputs keep their index array and emit no new work metadata.

Reference: Cherchi et al., *Interactive and Robust Mesh Booleans*, SIGGRAPH2022,
[local PDF](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf),
page2/figures2–3 also visually inspected: consistent arrangements and exact local
treatment of numerical special cases. This is a bounded planar terrain procedure.

## Whole product and reproduction

Use `.venv`, existing NumPy/Shapely/Clarabel/Memray/Blender and BLAS/OMP threads1.
`PYTHONPATH=lib/src:lib/tests:test/experiments/transport:test/experiments/assets:`
`test/experiments/render:test/experiments/buildings`.

The original107-source baseline and ordered prior modules are explicit inputs.
Run `stair_endpoint_frontier_study.py` with the versioned endpoint query, or the
existing local generator/reader on its three-source recipe. All outputs use fresh
system-temp paths. The automatic run on801e2f7bb now stops after two attempts:
six sources remain rejected for actual Float32 orientation, three sources pass.
The alternative joint product has110 sources; it is built from107, rather than
rewriting the separately retained108-source product.

The new body remains exact to the pre-repair body:92vertices/129top faces,
368closed body faces; D/Q volumes188.032663/188.033087m³ and normalized maximum
grades0.99195237/0.99195168. All27old assets/186arrays/957042B and124old support
contacts remain. Terrain294old facets→204points/346facets,60exact boundary vertices.
The fast proof removes2facets with4rational vertices,28pair searches/1convex test;
the first exact clipping version used6vertices/4triangle tests/24halfplane clips.

`stair_terrain_overlap_study.py --resident ... --prior-module ... --fixture ...`
`--local ... --before-body ... --output ...` reproduces saved layout/selection,
compares the actual pre-repair body and exports actual whole before/after scenes.
Render both scene JSONs with `render_transport_research.py`, open PNGs and compare
with `test/scripts/pixels.py`. Both1280×720 images were inspected: three connected
new paths appear; coarse terrain, diagnostic materials and open outer ends remain.
271996pixels differ,90321by more than1/255, including changed terrain tessellation.
The fast implementation preserves all24product fields and three metadata documents.
Freshly regenerated old valid control also preserves24fields/three documents.

Run `stair_terrain_overlap_cost.py --study ... --output ...` at normal priority;
optional `--reference-commit 2a992d0b8811e1702ec9e6d0292e069ce083a027` compares
the frozen full-clip proof. Intel i5-1135G7/Linux, Python3.13.5/NumPy2.5.4/Memray1.20.0,
nine rotating rounds, load4.31/4.44/5.67: exact/GEOS/full-clip CPU1.861/2.316/3.676ms,
events5408/5893/24597; requested552821/686488/2210445B; scoped peaks
244136/242672/242896B. Outputs agree on the real case. Peak is essentially unchanged.
The0.793ms area filter loses a positive-area island control. Already-valid exact
cost is0.662ms,479events,91398requestedB/38524B scoped peak. Full construction,
IO/heights/readers, total copies, process/UMA/native times remain outside this scope.
Known raw XY/index buffers and indexed coordinate copies are reported separately.

60targeted checks include real D/Q, convex/concave cover, full clipping, one-ULP
gap, tiny island, circular groups, winding/order, budgets and immutable valid data.
The larger actual Q failure remains red under the existing contract; regional
physical representations belong to #717. Native/SourceOf/routing/World/device/
UMA/p99 and full Buildings/Vegetation consumer admission remain separately open.

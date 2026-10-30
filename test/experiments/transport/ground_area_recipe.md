# Whole pedestrian floor on retained terrain

Owner #1824, Parent #434. Python only; normal World/routing authority remains
separate. First whole source: the actual pedestrian Polygon feature 1123829462
in the retained MVT snapshot, 142.584m², with original rings/properties/ordinal.
The fixed input `fixtures/ground-area-pedestrian-field-v1.json` contains five
actual terrain vertices/three facets and their immutable parent/query bindings.
The actual 218,795 B MVT is versioned; ordinary tests need no private temp inputs.

## Physical recipe and complete-domain fit

The existing field's `ground` flag means retained transport contact here;
**false** marks free terrain. It does not mean missing geometry. The source
frame, profile, whole feature and local terrain are checked before construction.
Existing SurfacePatches selects three faces with 30patch-bound tests and 3 face
tests; the held local mesh occupies 156 B. No whole terrain is remeshed.

The source polygon uses the existing 2^-11 m outward seed model: full original
coverage, same hole topology and at most 6 grid cells (2.93 mm) outward change.
The paving floor is an affine upper/lower pair with a closed boundary. Recipe:
maximum grade 0.1 from the actual pedestrian profile; nominal rise 5 cm, allowed
4–10 cm; nominal embed 2 cm, at least 1 cm. Grade and dimensions are procedural
assumptions, not observed source construction or accessibility certification.

One existing terrain-facet plane fails the whole-area relief requirement. A fit
using the full local triangle vertices also fails because those include points
outside this requested area. The accepted fit instead uses **all 15 vertices of
the terrain/whole-polygon common refinement**, not selected interior probes.
The extracted existing affine-sample kernel is unchanged mathematically; its
gradient fit/grade scaling and fixed-gradient minimax intercept are reused.
AffineGround retains its full old domain; 100 frozen original controls preserve
coefficient bytes and complete reports exactly after the small extraction.

For a candidate plane F, bound F-terrain over every refinement cell. The difference
is affine on each cell, so its extrema occur at that cell's vertices. If its
range is [low,high], set upper=F-low+nominal_rise and
lower=F-high-nominal_embed. This gives a parallel closed floor with constant
thickness, complete soil support and bounded rise without flattening terrain.
All original holes remain in the constrained cap triangulation.

The installed sample fit and a small bounded HiGHS minimax control both satisfy
the same whole physical recipe. HiGHS uses a conservative L1 grade constraint,
four unknowns and successful regular termination within 1 s; no feasible early
iterate is accepted. It is an explicit generation-only opponent, not a hidden
fallback or a hot-path solver. Its maximum rise is about 2.8 mm lower on this case,
but its construction costs more. The faster sample fit is the default.

SIGGRAPH first: Garland/Heckbert 1997 *Surface Simplification Using Quadric Error
Metrics*, local `doc/references/geometry/siggraph/1997-quadric-error-metrics.pdf`,
PDF pages 2–3 including equations/figures were inspected. Its plane-error
background does not give a maximum-height/support certificate; this recipe does
not implement QEM. Whole bounds below use the actual piecewise-affine terrain.

## Independent actual-storage checks

`ground_area_bounds.py` clips actual cap/terrain triangles using rational
half-planes, evaluates exact rational height differences at intersection
vertices and proves complete projected area coverage. Both original Double and
literal stored Float32 geometry are checked. Reported interval endpoints round
outward; grade comparisons use squared rational gradients, not a rounded norm.
The actual body must contain its exact oriented caps and vertical boundary walls;
closed winding, positive volume, whole source footprint/holes and recipe limits
are checked independently of fitting. These are geometry/storage statements,
not a GPU arithmetic or worldwide terrain-closure certificate.

Actual default Float32 rise is 0.050003–0.085482 m and embed 0.020003–0.055480 m;
grade is about 0.80 percent. The body has 8 vertices / 12 triangles, 336 B of geometric
payload. It adds one whole requested polygon; the remaining 70 area features stay
outside this finite request in the unchanged original dataset. Bridges, elevated
or uncertain platforms are not silently interpreted as ground pedestrian areas.

The diagnostic product stores typed primitive rows with exact size limits before
hashing/allocation and binds the caller's actual requested input hash. Its fresh
proof reader revalidates source/profile/frame and complete stored geometry.
It is not a proposed native DB format or a runtime cache-hit path. Six separately
saved/rehashed cases are refused: frame, source, recipe, hovering floor, shifted
footprint and truncated primitive row. Unit checks also refuse a sparse oversized
payload before any payload hashing/decoding.

## Existing complete world and pictures

The whole-scene guard binds the captured parent Buildings report/reader and
replays the exact three terrain face keys. Existing bounded transport volume
queries check the complete floor/clearance band: 250 metadata products, 3,827 face
tests, 39 retained materialized records and 0 conflicts. The known Float32 construction
guard remains active. Conservative boxes also separate all 69 old building bodies,
four SITE plants and the existing rail hardware. The profile reserves 4.5 m above
the actual floor; this does not grant a public route or vehicle access.

All 30 old terrain patches, 115 ground sources, buildings, plants, raised decks,
94 piers and hardware remain unchanged. Four actual 1280×720/16-sample EEVEE
whole/detail before/after PNGs on Mesa Intel Iris Xe were opened. The no-floor
whole control matches the prior rail scene in all 921,600 RGB pixels exactly;
only PNG metadata differs. Material reuses the held sealed-transport diagnostic
material; it is not a new measured source material. Images remain in system temp.

## Costs, reproduction and architecture

Frozen 12d813304, i5-1135G7/Linux, load 2.75/4.83/7.38. Seven normal-priority
rounds per method plus separate native Memray traces; imports/held inputs and
verification precede timing. Equal physical requirements do not require identical
planes or pixels. Owned restore copies all three ready meshes and plane arrays.

| Held-input scope | CPU median | Allocations | Requested | Scoped peak |
| --- | ---: | ---: | ---: | ---: |
| Whole-area preparation |1.037 ms|1,866|215,515 B|18,160 B|
| Affine construction |1.880 ms|3,902|407,831 B|14,417 B|
| HiGHS construction |4.737 ms|19,265|1,716,724 B|328,426 B|
| Restore equivalent owned product |0.129 ms|219|15,281 B|2,024 B|

Each construction/restore makes three Mesh objects and 576 B of known Mesh copies.
Other NumPy copies, source/MVT/whole-world preparation, serialization/IO, rendering,
physical traffic, process/UMA peaks and native/device times are separate/unmeasured.
These Python numbers do not qualify frame rate, p99, streaming or an 8 GB app budget.

Use the existing .venv with NumPy/GEOS/Shapely/SciPy and Blender bpy for diagnostics;
Memray only for allocation evidence. No new package is introduced. PYTHONPATH:
lib/src and test/experiments/{render,buildings,transport,assets,vegetation};
OPENBLAS_NUM_THREADS=1 and OMP_NUM_THREADS=1. All output paths must be fresh host
system-temp directories checked by the shared experiment_output helper.

```sh
python -B -m unittest ground_area_checks canonical_port_native_checks
python -B test/experiments/transport/ground_area_product.py produce --output "$product"
python -B test/experiments/transport/ground_area_product.py read "$product" --output "$read"
python -B test/experiments/transport/ground_area_cost.py --output "$cost"
```

Architecture acceptance: one existing numerical kernel is factored for two actual
sample-domain consumers; the old wrapper keeps its behavior. Recipe construction,
source input, rational proof, saved rows, complete-scene handoff and costs remain
separate small experiment modules. No native format/cache/scheduler/solver family
or source topology is introduced. Cached native blanks must eventually avoid
source preparation and cooking as #10 requires; this proof reader is not that
runtime consumer. Native floor/collision adoption, joins, accessibility, routing,
World/source closure, shading/LOD and both device budgets remain future contracts.

# Whole side bodies on retained raised transport

Owner #1883, Parent #434. Python only. The actual three raised rail sources
already have a finite flat bed,94piers and two complete hardware chains. This
adds four continuous concrete side bodies, preserving open track ends and the
whole115-ground/69-building/four-SITE scene, including its pedestrian floor.
Source IDs/properties and existing physical joint identity remain unchanged.

## Recipe and selected construction

Explicit procedural dimensions: height0.9m, thickness0.12m, lateral edge
margin0.05m, outer chain-end setback0.05m and central clear half-width1.5m.
These are model assumptions, not observed construction or safety/vehicle
certification. The actual3.8m bed leaves its declared central strip open. Whole stored strips
are checked across every chain; a Float32 inward-rounding opponent is rejected.
The setback affects only the two outer ends; internal source joins stay whole.
The original axis and existing bed/hardware/consumer geometry are retained.

The first GEOS band/CDT construction collapsed thin triangles at real source
knicks. The existing outward binary seed model admitted one chain but failed
its full coverage/topology contract on the other: rejected as the general path.
The selected method directly connects indexed inner/outer cross-sections and
reuses the existing cap-closing kernel. There is no polygon CDT, raster snap,
new solver, remeshing of old products or larger source budget in this path.
The existing rail miter/offset-fold preconditions limit corners; loops, folds,
unknown joins, insufficient bed/width/height and storage collapse are rejected.

The actual result has80vertices/144triangles in four positively closed bodies:
3,648B of retained Float64/UInt32 body arrays. Common object height datum remains
separate; bottom localZ=0 has the same encoded datum as the unchanged flat bed.
Double and stored Float32 cap triangles are checked against the corresponding
actual finite bed using exact rational triangle intersections:392candidate
pairs/824intersection vertices, zero gap over every whole lower cap. Existing
1,881sleeper and four rail footprints are disjoint in both stored precisions.
This is a local stored-geometry check, not a GPU arithmetic or World certificate.

SIGGRAPH first: the existing2008 *Interactive Procedural Street Modeling* paper
was inspected optically at PDFpage3; it concerns network construction, not this
profile proof. Galin et al., *Procedural Generation of Roads*, Eurographics2010,
`doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf`,
PDFpage8/section6/Figures17–18, was also inspected optically: explicit trajectory
and parametric cross-section are the relevant established approach. Bounds and
whole stored-body acceptance here come from the actual checks, not that paper.

## Inputs, reader and display

`raised_barrier_study.py` binds the existing independently read whole hardware
and source/Buildings snapshot, original chain directions/joint identities and
reserved height. No source reconstruction is hidden in a runtime cache claim.
It saves small body rows and appends only diagnostic display bodies to a pinned
complete old scene. All old mesh pins remain unchanged. The normal tests read
the existing versioned three-source fixture and need no private experiment root.

`raised_barrier_verify.py` checks requested recipe/frame/source/side/datum, bounded
archive headers before array decode, complete source sections and independent
cap/body topology plus rational whole contact and old-hardware separation.
It is an offline qualification reader, not a native source-free asset loader.
Real saved/rehashed frame/source/recipe, floating-body, shifted-profile and
truncated-array opponents are rejected. Unknown products retain the old scene.

Four actual1280×720/16sample EEVEE images were opened, Mesa Intel Iris Xe.
The before-whole image is pixel-identical to the retained complete scene;
1,274pixels change with the side bodies. Detail views show continuous sides and
retained rails/sleepers. Geometry and display payloads from current acceptance
are byte-identical to the frozen rendered products. Images remain host-temp only.

## Costs and reproduction

Measured source557a054a7, Linux/i5-1135G7/Python3.13.5/NumPy2.5.4;
load3.77/3.96/4.67. Seven CPU samples plus separate Memray traces per method.
Same complete qualified output: generation170.840ms, owned held-product
copy0.148ms. Allocations1,204,581/252; requested109,324,973/18,468B;
scoped peak2,069,163/2,768B. Generation includes rational whole contact and
hardware acceptance; this expensive diagnostic proof belongs outside runtime.
Known Mesh-constructor copies: generation at least13,632B, held output3,648B;
other temporaries remain in traced counts. Numeric held query inputs193,480B.
Separate diagnostic input binding/read preparation2.913s is excluded and reported.
Trace peak counts only scope allocations captured by the tracker; held buffers
are separate, and this is no total live-memory peak. No native cache/IO/frame/
p99/physical UMA/RSS or device-budget claim.

Use the repository `.venv`, BLAS/OMP1 and
`PYTHONPATH=lib/src:test/experiments/{render,buildings,transport,assets,vegetation}`
(expand each path explicitly). Freeze source before these programs:

```sh
python test/experiments/transport/raised_barrier_study.py --hardware "$hardware" --receipt "$receipt" --scene "$scene" --output "$product"
python test/experiments/transport/raised_barrier_verify.py "$product" --output "$read"
python test/experiments/transport/raised_barrier_cost.py "$product" --output "$cost"
```

All outputs must be fresh explicit system-temp directories. The existing
`render_transport_research.py` consumes the four emitted scene JSON files.
No new dependency, native API/cache format, worker, scheduler or lib change.
Own bounded architecture decision: indexed profile construction; independent
whole-body qualification; source/scene and costs in their respective drivers.
Native adoption, collision integration, complete routing/vehicle/structural
qualification, World and device budgets remain separate open contracts.

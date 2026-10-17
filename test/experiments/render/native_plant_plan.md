# Native canonical plant demand — #1 / #48

`src/generators/flora/PlantPlan` consumes the already prepared canonical axis
and leaf rows from the checked Python PlantPlan contract. It does not yet
regenerate a recipe, decode an asset, build geometry or replace world trees.
Those are separate integration steps, not implied by the CPU comparison.
[PlantRecipe](native_plant_recipe.md) now owns direct native recipe generation.

## Kept semantics

Axes retain IDs, signed parent IDs(-1 for root), seven-slot paths(-1 padding),
metre origins/lengths/radii, row-major right-handed orthonormal frames, bends
and parent parameters. Leaves retain separate IDs, owner-axis IDs, variants,
origins/nodes/frames and stem parameters. Prototype coordinates remain the
Python reference's Z-up frame; world Y-up conversion belongs at placement.

Construction moves the supplied vectors into immutable ownership. Canonical
parents precede children and leaves are grouped in axis-row order. Unknown
parents/owners, duplicate IDs/paths, invalid frames, nonfinite dimensions and
invalid variant/parameter ranges fail before a plan becomes available.
A summary inventories all descendant axes/leaves and blade variants and borrows
a contiguous range of prepared axis-row indices. Lexicographic canonical paths
put descendants together without changing the original breadth-first rows. A leaf
demand returns only the named axis's own group and refuses an insufficient
budget atomically. Queries return borrowed const spans; their owner must live.
There is no resampling, row copy, geometry generation or allocation per query.

ID lookup has expected constant time; summaries use prepared counts. Initial
validation/indexing sorts a flat row index for path uniqueness(O(A log A)), hash sets
and A×V variant accumulation, plus linear leaf traversal. On a64-bit host the
subtree range adds two size_t per axis:16A bytes beyond the earlier counts;
the temporary allocating path set is gone. Queries do not scan unrelated axes.
This is not a claim
of global optimality or a whole-world packed/mapped asset implementation.

## Independent fixed-row check

The existing #367/#374 `tree-entry-sapling-17.npz` and its checked SHA256 remain
unchanged. A test-only standard-library ZIP/NPY1 reader decodes that fixed
fixture; it is not a product NPZ importer or a new asset ABI. It sends the
canonical rows to `NativePlantPlanReference.cpp`. The native consumer returns
all actual axes/leaves, every branch summary and selected descendant IDs. The checker compares every
integer/Float64 row exactly and independently inventories descendants using
path prefixes, independently sorting each branch by path. All64 axes,571 leaves,
64 subtree summaries and all64 ordered descendant ranges agree. The
native probe also checks each nonempty leaf group's insufficient-budget case.
No expected rows, pins or oracles are generated or rewritten.

The small module case independently covers a branching sibling/grandchild
counterexample, immutable repeated detail, bare/unknown axes, duplicate IDs,
wrong ownership/order and invalid frames. Normal, ASan/UBSan and container
hardening pass. The suite compiles only the owned plant data/surface units, with no GPU/engine link.
Product and independent probe pass focused clang-tidy. Analysis of the case
still exposes the pre-existing shared Check.h findings owned by #603/PR#666;
no suppression or complete lint pass is claimed.

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PlantPlan
c++ -std=c++23 -fno-exceptions -O2 -Iinclude -Isrc/generators/flora \
  src/generators/flora/PlantPlan.cpp src/generators/flora/PlantAxisSurface.cpp \
  src/generators/flora/PlantSurface.cpp src/generators/flora/PlantBladeSurface.cpp \
  test/experiments/render/NativePlantPlanReference.cpp -o /tmp/outshine-plant-plan-reference
TMPDIR=/tmp python3 test/experiments/render/native_plant_plan_reference.py \
  --binary /tmp/outshine-plant-plan-reference --output /tmp/outshine-plant-plan-check
```

This proves native prepared-row consumption and bounded detail requests only.
C++ recipe/plan persistence, profiled/cutout surfaces, selected cached LOD products,
world selection, near/far coverage and Metal/Vulkan images/costs remain open.

## Native circular-axis surfaces

PlantAxisSurface now builds only the demanded axis from these immutable rows.
Segments/sides and maximum vertices/triangles are explicit. Unknown axes,
invalid subdivision, regularity/Float32-range failures and insufficient budgets
are rejected before mesh allocation. The generated Float32 indexed surface has
separate hard cap normals. Final corner orientation is checked; unresolved or
collapsed detail returns an error rather than publishing corrupt triangles.
This is the port of the existing circular/tapered axis_mesh, not SpatialAxis,
profiled axes, a welded branching solid or a selected screen-error LOD rule.

The same fixed64 axes produce25600 triangles. Every native triangle corner is
compared in order against existing Float64 Python axis_mesh under the original
frame/origin. Largest position error5.961e-8m, normal-component error2.981e-8;
limits2e-7m/1e-7 bound this local Float32 technical fixture, not world geometry.
No original pin or Python implementation changed. Six module arms pass with
normal, ASan/UBSan and container builds. Product source tidy is clean; shared
Check.h analysis remains tracked under #603. No world/device closure follows.

For the surface comparison use the NumPy reference environment and
PYTHONPATH=lib/src, then add --surfaces to native_plant_plan_reference.py.
The C++ probe emits actual indexed corners; Python geometry is only the
independent comparator. Private outputs remain under the supplied temp path.

## Eight shared native blade surfaces

PlantBladeSurface ports the existing Blade carrier, width warp, lobes,
bend/twist, linear/quadratic fold and petiole. Counts are derived from the
requested sampling/topology before surface allocation. Midrib and petiole
normal discontinuities keep separate indexed vertices; unused terminal
corners are never generated. Metric parameters and UVs remain attached.
PlantArc and PlantSurface share the actual axis/blade arc and orientation
operations. No general grammar, asset ABI or second world representation added.

All eight existing recipes produce206 vertices/260 triangles each. Actual
native corners compare in order against unchanged Python Blade.mesh:
maximum position3.726e-9m, normal-component2.980e-8, UV1.743e-9. Limits are
1e-8m/1e-7/3e-8 for this local Float32 fixture. All571 canonical placements
and eight prototype identities remain unchanged; no per-leaf mesh generation
or placement resampling occurs in the native builders. This says nothing yet
about the world consumer, prototype pooling policy, plant materials or wind.

Nine module arms cover rows, closed axes and folded blades in normal,
ASan/UBSan and container builds. Product and independent probe analysis is
focused; existing shared Check.h findings remain separate. Use --blades in
the NumPy reference environment to compare both wood and eight prototypes.

## Actual shared-mesh Metal portrait

The complete technical fixture was rendered at640×720 and opened on the native
integration branch. One combined wood mesh and eight actual shared leaf meshes
use571 instance matrices. Counters from the actual built arrays report174060
logical triangle references,719744B prototype arrays and73088B leaf matrices.
These are CPU logical payloads, not capacities, RSS, physical GPU bytes or
actual post-cull shader invocations. The picture shows a coherent young plant;
materials use the existing default thin-sheet response, with no new vein/wood
material or botanical/SpeedTree acceptance implied.

An initial declared autoExposure stage was rejected before device execution.
The successful comparison explicitly uses the existing RuntimeScene light-based
calibration:2.5/(1.2×20000)=0.0001041667 exposure for20000lux directional light.
This does not implement GPU image metering or establish a Metal-only regression.
Original rejected attempt, successful command, source/binary/PNG hashes,
computed counters and image remain in /tmp/outshine-plant-metal-2ab0d9ee7/.
One offscreen frame is component evidence, not a complete product build, Places
integration, timing/memory gate or world-quality proof.

## Allocation-free surface measurement

MeasurePlantAxisSurface and MeasurePlantBladeSurface expose the same parameter,
regularity, subdivision and count checks used by their builders. They allocate
no surface arrays. Product assembly can therefore total selected axes and used
blade prototypes before construction; counts remain separate from instanced
triangle references. The fixed reference still matches all original corners.

## Prepared product persistence

[Prepared native plant assets](prepared_plant_assets.md) describes the finished
prototype/instance boundary, generic cache integration and actual fresh-process
Metal comparison. That asset decodes without constructing PlantPlan or surfaces;
its separate plan identity permits later detail refinement. It does not close
recipe planning, world consumption or LOD/device acceptance.

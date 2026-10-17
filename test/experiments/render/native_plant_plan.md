# Native canonical plant demand — #1 / #48

`src/generators/flora/PlantPlan` consumes the already prepared canonical axis
and leaf rows from the checked Python PlantPlan contract. It does not yet
regenerate a recipe, decode an asset, build geometry or replace world trees.
Those are separate integration steps, not implied by the CPU comparison.

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
A summary inventories all descendant axes/leaves and blade variants. A leaf
demand returns only the named axis's own group and refuses an insufficient
budget atomically. Queries return borrowed const spans; their owner must live.
There is no resampling, row copy, geometry generation or allocation per query.

ID lookup has expected constant time; summaries use prepared counts. Initial
validation/indexing includes ordered path uniqueness(O(A log A)), hash sets
and A×V variant accumulation, plus linear leaf traversal. This is not a claim
of global optimality or a whole-world packed/mapped asset implementation.

## Independent fixed-row check

The existing #367/#374 `tree-entry-sapling-17.npz` and its checked SHA256 remain
unchanged. A test-only standard-library ZIP/NPY1 reader decodes that fixed
fixture; it is not a product NPZ importer or a new asset ABI. It sends the
canonical rows to `NativePlantPlanReference.cpp`. The native consumer returns
all actual axes/leaves and every branch summary. The checker compares every
integer/Float64 row exactly and independently inventories descendants using
path prefixes. All64 axes,571 leaves and64 subtree summaries agree. The
native probe also checks each nonempty leaf group's insufficient-budget case.
No expected rows, pins or oracles are generated or rewritten.

The small module case independently covers a branching sibling/grandchild
counterexample, immutable repeated detail, bare/unknown axes, duplicate IDs,
wrong ownership/order and invalid frames. Normal, ASan/UBSan and container
hardening pass. The suite compiles PlantPlan.cpp and PlantAxisSurface.cpp, with no GPU/engine link.
Product and independent probe pass focused clang-tidy. Analysis of the case
still exposes the pre-existing shared Check.h findings owned by #603/PR#666;
no suppression or complete lint pass is claimed.

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PlantPlan
c++ -std=c++23 -fno-exceptions -O2 -Iinclude -Isrc/generators/flora \
  src/generators/flora/PlantPlan.cpp src/generators/flora/PlantAxisSurface.cpp \
  test/experiments/render/NativePlantPlanReference.cpp -o /tmp/outshine-plant-plan-reference
TMPDIR=/tmp python3 test/experiments/render/native_plant_plan_reference.py \
  --binary /tmp/outshine-plant-plan-reference --output /tmp/outshine-plant-plan-check
```

This proves native prepared-row consumption and bounded detail requests only.
C++ recipe planning, serialization, leaf/profiled surfaces, cached LOD products,
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

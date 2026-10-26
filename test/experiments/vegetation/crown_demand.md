# Original crown demand before leaf poses

Owner [#716](https://github.com/Weichwolf/outshine/issues/716), following the
[connected original leaf carriers](leaf_patches.md) and their
[material comparison](connected_materials.md). This is an experimental Python
cache-miss/reference builder, not a per-frame generator or new native contract.
It reduces selected original host groups before pose allocation or organ meshes.

## Original input and analytic envelope

Use the six unchanged versioned files in `data/crown682/`,185651B plus manifest.
The source has5472 skeleton nodes,4301 foliage hosts and90000 original leaves;
source catalog/material identity is
`62a49f3c918384dba7633ec6e13a3f48560c828df682c0745cea7e0e8115015d`.
Continuous original random phases, variants, anchors, uint64 IDs and physical
area remain unchanged. Source poses are generated only for selected hosts.

`crown_demand.host_bounds` reads the skeleton and recipes only. The root control
uses the original shoot origin plus/minus shoot length and organ reach. The
candidate bounds each coordinate of the original circular shoot centreline.
With stem basis Y/Z, its derivative is proportional to
`Y*cos(bend*t)+Z*sin(bend*t)`. Include both endpoints and stationary values
`bend*t=atan2(-Y,Z)+k*pi`; the implemented k=-1,0,1 covers this original .4rad
shoot and the checked -1/0/+1rad controls. `CrownShoots` already restricts
absolute bend to1rad; arbitrary multi-turn shoot recipes are outside this
experiment. Zero bend uses the straight segment.

Expand those centreline bounds by the maximum original organ reach:
`crown.petiole_m + max(length + petiole + width/2*(4/3)*hypot(1,fold))`.
Triangle inequality covers the entire original Blade surface and the connected
carrier with its largest admitted domain padding(size16). Connected32/64 have
smaller X padding over the same longitudinal rows. A scale-dependent256-epsilon
Float64 guard and outward `nextafter` cover numerical roundoff in this study;
this is not a formally certified bound for every possible recipe or transform.

`CrownDemand.query` reuses existing `demand.Camera` frustum planes and projected
span semantics, vectorized over host AABBs. One to16 supplied perspective views
are unioned. Default detail threshold0 keeps every visible host; a nonzero
threshold requires the caller's own coarse fallback, which this study does not
invent. Capacity is explicit, at most100000 leaf poses. A refused query returns
no poses before calling any group; it never silently drops visible sources.
`prepare` preallocates final ID/origin/frame/variant arrays once and copies each
selected original group, retaining at most one temporary source group. No
whole-crown `tables()` call or concatenated second full pose table is required.

The complete immutable plant plan remains the source for later camera views.
Camera, bounds and source use the same metre frame. Rotation changes selection,
not the plant. These static bounds do not include wind deformation or unseen
shadow casters; supplying several cameras does not itself solve shadow demand.

## Exhaustive source control and measured work

Frozen producer0cdf79f75 checks3330000 transformed vertices from all90000
original poses: source8×1 Blade meshes and connected16 carriers. Every vertex
is inside both host envelopes. Maximum signed violation is -.0745626m(root)
and -.0453561m(arc); negative means inside. All selected output rows are bitwise
equal to the full original comparator's ID-indexed pose/variant rows. That full
pose table is retained solely for independent verification and rendering.

Both envelopes hold206448B of bounds(4301×6 Float64). Build wall times are
.02456s root/.02617s arc; the root run includes initial lazy host discovery,
so this is not an isolated envelope-build ranking. Query times below are
medians of five repetitions; preparation is one run including its query.

| View | Envelope | Hosts | Leaf poses | Query ms | Prepare s | Final pose bytes |
|---|---|---:|---:|---:|---:|---:|
| Whole | Root |4301|90000|1.686|3.615|9450000|
| Whole | Arc |4301|90000|1.361|3.663|9450000|
| Inside | Root |824|17264|1.450|.677|1812720|
| Inside | Arc |679|14231|2.181|.577|1494255|
| Edge | Root |1559|32649|1.313|1.250|3428145|
| Edge | Arc |1338|28026|1.868|1.088|2942730|
| Away | Root |0|0|1.299|.00168|0|
| Away | Arc |0|0|1.268|.00161|0|

The inside request falls17.57% relative to the root envelope and84.19% relative
to the full crown. At capacity16000 the root request refuses17264 and emits
nothing; the arc request accepts exactly14231. The whole-crown counterexample
still requests all90000. Selection alone does not remove the complete visible
crown's pose/matrix storage or far-forest triangle work. Separate compact
continuous original-group evaluation is reserved in
[#721](https://github.com/Weichwolf/outshine/issues/721).

Final arrays are105B/leaf(uint64 ID,Float64 origin/frame,uint8 variant), not the
native renderer's matrix layout. Bounds and final arrays are measured payloads;
Python containers, temporary arrays, cumulative allocation counts and total
read/write traffic are not measured. The exhaustive process peaks101339136B
Linux RSS including its full-reference comparator. Normal-priority timings use
NumPy2.5.4, one BLAS/OMP thread on a shared four-CPU WSL host. They are neither
an isolated hardware comparison nor a native/GPU/A18 frame-budget claim.

## Actual same-camera rendering

Final reader ca4d53a85 consumes finished original poses and shared connected32
templates; pose generation is forbidden by a patched source group method.
Full-reference/root/arc use the same inside camera: eye(0,0,12),target(2,0,12),
vertical FOV60°,clip .1/30000m. Query1280×720 and rendering2560×1440 share aspect
and FOV. The other numerical controls use whole eye(0,-35,12),target(0,0,10);
edge eye(5,-4,12),target(5,-1,12); away eye(0,-35,12),target(0,-50,12).

The reader considers90000/17264/14231 poses, then rejects individual leaves by
their conservatively transformed prototype AABB before diagnostic triangles.
Absolute rotation times local half-extents plus an8-epsilon Float32 guard
enclose the actual Blender position copy. Crossing triangles are retained even
when all vertices are outside the camera. Remaining leaves are ordered front
to back within each variant, with original IDs as ties; eight mesh batches
carry original leaf IDs as face attributes. This expanded diagnostic geometry
is not proposed runtime instance storage. No unprovided occlusion oracle is used.

All three actually opened2560×1440 images are RGB8 pixel-identical: zero mean,
maximum and changed-pixel error. Each draws11629 leaves/93032 triangles and
12001128B of expanded CPU mesh arrays. Dense projected fans and repeated shoot
patterns remain visibly artificial; the source is deliberately unchanged.
This is constant-colour alpha coverage, not a PBR/material, botanical form,
shadow, wind, forest or LOD quality admission.

EEVEE5.2.2,32 samples uses software Mesa llvmpipe, not the host RTX GPU.
Scene builds .662/.788/.527s; renders105.157/126.983/82.505s; process peak
2298605568B. Other agents' CPU/Blender work shares the host. Do not rank these
render times: final raster work is identical and load is uncontrolled.

Two earlier readers were deliberately stopped without PNGs:90000 geometry-node
point instances(86ecc0fd0), then eight unculled expanded batches(dec59e2ac).
Both terminate143 after SIGTERM, not success or a proved software hang. They
provide no completed before/after timing or device performance result. A short
thread CPU sample identifies work in the software renderer, not a full profile.
The final reader avoids creating invisible diagnostic triangles and finishes0;
no claim of isolated render acceleration follows from the interrupted controls.

## Reproduction and limits

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread and
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
Python3.13.5, NumPy2.5.4 and bpy5.2.2 were used. Run
`crown_demand_probe.py --inputs test/experiments/vegetation/data/crown682
--output /tmp/<fresh-proof>` from the repository root. Prepare templates with
the parent `connected_comparison.py` reproduction, then
`render_crown_demand.py --inputs test/experiments/vegetation/data/crown682
--proof /tmp/<fresh-proof> --templates /tmp/<connected-study>/connected32
--output /tmp/<fresh-render>`. Set LIBGL_ALWAYS_SOFTWARE=1 and explicitly place
XDG_CACHE_HOME/MESA_SHADER_CACHE_DIR under/tmp. All output directories are new;
the scripts refuse private outputs outside/tmp. No diagnostic PNG is versioned.

Eight new controls cover no-demand work, zero/negative bend, straight geometry,
near-plane projection, empty/opposite views, union/refusal, original rows,
diagnostic transforms/UV/indexing and Float32/crossing-triangle bounds. The
combined86 controls pass both source and installed modes. The installed run
reuses the verified PR707 wheel, with all212 package Python files equal to this
focused tree; no library file is changed by this experiment. The permanent
vegetation branch's independent Draft202 library work is outside that claim.

Local SIGGRAPH2015 *GPU-Driven Rendering Pipelines*, slide13, was optically
inspected: coarse instance rejection precedes cluster expansion and finer
culling. This study applies only the ordering principle, with NumPy and a
diagnostic Blender reader, not that GPU pipeline. Source:
`doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf`.
Existing Ghidra/SpeedTree ComputeLodSkin evidence remains in the parent study;
no new binary analysis or proprietary implementation is claimed here.

Native world integration, shared bindings, dynamic residency, camera-motion
transitions, full shadows/wind/material response, direct coarse forests,
species architecture and device budgets remain separate unfinished work.

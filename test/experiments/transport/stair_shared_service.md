# One physical service lane, two centered directions

Issue #1727 extends the existing bounded Transport seed producer with an explicit
`shared_service` profile. One complete previously unused 106.199 m MVT part now
adds one physical road asset to the verified 105-source context. Both compact-car
orientations fit its actual stored cap; the original source carries no direction
word. This is a procedural interpretation, not recovered traffic regulation.

## Recipe and consumers

The source retains its original service class, surface/access tags, geometry,
part ID and provenance. Missing direction is recorded as `procedural_default`;
an explicit integer zero is separately `supplied_two_way`. Known one-way,
conditional/ambiguous direction and non-ground cases reject this recipe.
The existing one-way recipe still requires its supplied +1/-1 direction.

`physical_lanes=1` and `lane_use=alternating_bidirectional` distinguish the new
profile from two physical lanes. Forward/reverse counts describe the two logical
directions. Both offsets are zero and require `exclusive_lane_occupancy`.
Malformed/doubled/separate-lane metadata rejects at the body consumer. This
declares an occupancy requirement; it implements no runtime vehicle arbiter and
admits no simultaneous opposing passage. Public routing remains unadmitted.

Reuse the existing 3.5 m service width, 4.5 m clearance, 5.56 m/s speed, 0.12
longitudinal and 0.03 crossfall limits. These are explicit construction assumptions.
No native/world profile or source tag is changed. Landings, material ownership,
seed audit and the vehicle consumer recognize the new role together. No library,
cache, graph scheduler, public SDK or native type is introduced.

The same G2 curve producer and bounded whole-body checks apply in each direction.
Reverse orientation swaps the asymmetric front/rear end insets. Both original
ends retain the required body extent plus 0.05 m; this does not prove a junction
traversal, 3D chassis, steering dynamics or connection to another road. Reusing
one physical cap per precision avoids constructing two independent route caps.

SIGGRAPH-first source:
[Chen et al., Interactive Procedural Street Modeling, 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
PDF page 3/Figure 3 visually inspected. The network/interpretation step remains
separate from geometric realization. No new tensor-field or curve solver is
implemented; the established [horizontal kernel](../../../lib/horizontal.md)
and [one-way service reference](stair_service_comparison.md) remain the basis.

## Complete real controls

The frozen original catalog contains 1,657 line parts and 71 area features.
Of 181 service parts, 153 omit `oneway` and 28 supply a one-way direction. None
in this input supplies an explicit integer zero. These are catalog facts, not
proof that all missing-direction services support this geometric recipe.

Three complete source trials keep the existing budgets:

| Fixture | Actual complete result |
| --- | --- |
| `stair-service-shared-straight-vienna.json` | Part `822553560:33`, 106.199 m: accepted as a local physical module; 105→106 sources |
| `stair-service-shared-short-bends-vienna.json` | Part `2318217460:5`: fixed 6 m curve radius lacks original straight length; input rejects before publication |
| `stair-service-shared-profile-refusal-vienna.json` | Part `9609826400:1`: actual longitudinal/crossfall frame fails; independently verified retained 105-source baseline |

All IDs have the original `openfreemap:14/8937/5678:transportation:` prefix.
Fixtures bind the unchanged original source/DEM fixture by hash. Refusals retain
original data; a failed input does not publish a partial road or invent a height.
No limit, contact guard or old source admission is loosened.

The table records the frozen #1727 comparison. [Height repair #1731](stair_seed_height_rounding.md)
now qualifies the same curved profile-refusal input after a bounded stored-height
correction; its original geometry/DEM/recipe and strict profile limits are unchanged.

The positive surface has 10 vertices/10 triangles; its closed 0.3 m body has
36 triangles and volume 111.578390/111.578106 m³ in Float64/Float32. Whole source
coverage, both complete outer profiles and their directions pass. Maximum
normalized longitudinal/crossfall grade is 0.995997/0.993688; no extra tolerance.
Four exact prismatic swept-body checks cover both orientations and precisions.
The separate-cap opponent returns identical complete check records. A synthetic
90° G2 seed also checks both orientations in both precisions; that does not turn
the actual curved refusal into a positive case.

Existing local field work selects 133 old terrain faces and produces 121 points/
209 faces with 31 boundary vertices and 71 finite pins. Actual ground contact
error is at most 1.145e-5 m in the Float32 control. All 124 old support contacts
remain valid. Nearest old ground is 237.193 m away: this is an independent seed,
not an invented road connection. Existing whole geometry/terrain/pier budgets
remain unchanged. The catalog keeps all 1,551 unused lines and all 71 area
features explicitly unresolved/unplanned; native admitted-source IDs stay empty.

The saved independent reader passes the positive module and retained refusal.
The study compares all 25 old assets: 172 arrays/949,416 bytes are unchanged.
A fresh original one-way generation on a1de82630 versus the extended producer
on5f4ad3c4b yields identical values in all 23 array fields/25,696 bytes plus the
three planning/catalog/key documents. This preserves an actual complete old
product, not only isolated profile metadata.

## Costs and images

Final held-body-check measurement on c9aacb876, Linux/Python3.13.5/NumPy2.5.4/
Memray1.20: nine rotating normal-priority CPU/wall rounds, BLAS/OMP1, host load
18.16/17.80/11.48. The shared-cap/separate-cap methods return the same four
complete checks. CPU medians are 2.233/2.840 ms; allocation events 5,445/8,144;
requested bytes 605,480/937,640. Scoped high watermark rises 29,568→30,480 B.
Keep both final raw traces. CPU timing under development load is secondary to
the explicit work/allocation reduction; no portable/native speedup is claimed.

Both methods perform four exact straight-body sweeps. Shared caps perform two
unions versus four independent per-route unions. Known Float32 roundtrip copies
are 360 versus 720 B for the ten stored vertices. Total copies and physical
traffic are unmeasured. Held inputs/expected results precede tracing; peaks are
incremental function scopes, not process/UMA peaks or the full generator budget.
Initial complete local production took 3.049 s alongside the two other trials;
independent reading took 16.681 s. Neither is a native loading/frame measurement.

Matched 1280×720/16-sample Eevee diagnostics use the actual complete held terrain,
road and support meshes, physical material values and identical cameras. Both
images were opened: the added complete narrow paving strip is visible. Coarse
faceted terrain and isolated endpoints remain visible deficits. Pixel comparison
changes 168,549 pixels, 41,114 by more than 1/255, with no black pixels; changes
include local terrain triangulation/shading and are not attributed solely to
the new cap. Blender5.2.2/llvmpipe is a Python diagnostic, not the native Places
gate or a device-performance result. Images stay exclusively in system temp.

## Reproduction and scope

Use `.venv`, one BLAS/OMP thread, and `PYTHONPATH` containing `lib/src` and the
transport/assets/render experiment directories; the allocation driver additionally
uses the existing buildings `allocation_measure` helper. No new packages needed.

Run `stair_service_checks`, `stair_shared_service_checks`, `stair_seed_checks`,
`stair_closed_seed_checks`, `stair_southwest_checks`, `stair_road_curve_checks` and
`stair_frontier_checks` with unittest: 54 checks pass. These preserve existing
two-lane/one-way choices and test malformed/conditional source facts, exclusive
space metadata, asymmetric bodies, source topology, curvature and oversized bodies.

Freeze sources before production/measurement/rendering. Existing
`stair_local_generate.py` and fresh-process `stair_local_verify.py` take
`--resident`, repeatable `--prior-module`, `--fixture` and `--output`.
Use `stair_shared_service_study.py --resident ... --prior-module ... --local ...
--fixture ... --output /tmp/outshine-.../outshine-study` for independent asset/body
comparison and scene export. Render its `before-scene.json` and `after-scene.json`
with `render_transport_research.py`; compare using `test/scripts/pixels.py`.
`stair_shared_service_cost.py --local ... --fixture ... --output
/tmp/outshine-.../outshine-cost` records costs and exact input hashes.

Current complete evidence is under
`/tmp/outshine-research-service-lane-0b9e1495f-evidence/`; protected source handoff
inputs are `/tmp/outshine-transport-resume-1f10d55eb/outshine-resident102` and
`outshine-follow105-stored/outshine-module-0`. No test depends on these private
outputs. Fixed source/DEM and all three recipe fixtures are versioned. The
existing saved reader does not regenerate geometry or relax old pins.

SourceOf/MissingTopology/EndContact, all geographic networks, building/plant
interaction, routing/dynamics, native assets/World and both devices' p99/8-GB
system budgets remain separate. This is a bounded functional Python addition,
not an acceptance of the full Outshine goal.

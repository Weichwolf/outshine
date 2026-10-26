# Rail hardware reference

Owner: [#250](https://github.com/Weichwolf/outshine/issues/250), transport #19/#50;
native integration remains #11/#48. No SDK or new package dependency.

```python
from outshine_ref.transport.rail_hardware import RailRecipe, build_rail_hardware, bake_rail_sleepers

hardware, report = build_rail_hardware([[0, 0], [40, 0]], 6.0,
                                     recipe=RailRecipe(gauge_m=1.435))
if hardware is not None:
    rails = hardware.rails
    shared_unit_sleeper = hardware.sleeper
    instances = hardware.sleeper_transforms
    inspection_mesh = bake_rail_sleepers(instances)
```

The input is an explicit horizontal-bed centreline in the caller's local XY
metres and a common Z datum. Frame, selected finite bed, sources, classification
and which optional hardware is admissible belong to the caller. There is no
automatic highest-surface choice, DEM sampling or network reconstruction.

`RailHardware` owns immutable rail buffers, a shared immutable unit sleeper mesh,
read-only Float64 `N×3×4` affine sleeper transforms and a tuple of two rail-foot
polygons followed by each sleeper footprint. Transform columns are transverse,
backward longitudinal and up, with translation in column four. This preserves
positive handedness. `bake_rail_sleepers` expands exactly that shared mesh for
inspection; consumers can retain instances and avoid CPU vertex expansion.

The gauge is the distance between inner head faces, not rail centres. The own
12-point section has head width 0.072 m, foot width 0.150 m and height 0.172 m;
centre separation is `gauge+0.072 m`. It is a simplified recipe inspired by
[voestalpine's 60E1 dimensions](https://www.voestalpine.com/highperformancemetals/argentina/app/uploads/sites/264/2024/09/voestalpine-60E1.pdf),
not that manufacturer's exact outline or structural rail model. The 1.435 m
default gauge is procedural when source tags are absent; it does not assert the
actual gauge of Tokyo or any other place. No country/place special geometry.

Default sleepers: 2.6×0.24×0.18 m, target spacing 0.6 m. Rail feet meet sleeper
tops at `datum+0.18 m`; the rail head reaches `datum+0.352 m`. Polyline joints
use bounded miters. Overlapping immediate-neighbour sleepers at a corner are
locally omitted, at most one consecutive sleeper, explicitly counted. Nonlocal
overlap, rail-foot intersection, offset fold, sharp corners, closed periodic joins
or short spans reject that optional product with a reason. This is neither a
smooth train trajectory nor a load/fastening/rail-inclination/turnout model.

`RailBudget` defaults: at most 4096 stations, 8192 candidate sleepers and 98304
rail vertices per call. Its integer limits are bounded by 262144. Sleepers are
generated only after preflight; per-product pair checks use 256-element batches
and at most 100000 reported candidates. Bake expansion accepts at most 262144
finite positive, nondegenerate affine transforms. Invalid values/shapes raise;
budget/geometric absence returns `(None, report)`.

The caller must check **whole** footprints against the actual selected bed and
other hardware. A matching datum alone does not prove support. Rail feet touching
sleeper tops have zero volume overlap; rail/rail or sleeper/sleeper footprint
overlap at the same height is a conflict. The four-place experiment keeps valid
beds, leaves conservative switch/crossing gaps, and rejects conflicting optional
pieces locally with source IDs. Preserved old C1 windows do not silently become
horizontal hardware inputs.

Experiments, analytic controls, a real clipped Tokyo overlap fixture, independent
finite-bed/volume/gauge replay and equivalent instance/baked cost measurements
live under `test/experiments/transport/rail_*.py`. All mathematical construction
is in this package. Public package ownership tests live in
`lib/tests/test_rail_hardware.py`; experiment/native/source/frame proofs remain
separate. Python/GEOS generation and baked Workbench inspection are offline
measurements, not a renderer FPS, native LOD or device acceptance.

## Explicit affine bed planes (#376)

The previously experimental metric bed adaptation from #340/#352 is installable:

```python
from outshine_ref.transport.rail_hardware import build_planar_rail_hardware

hardware, report = build_planar_rail_hardware(
    [[0, 0], [40, 0]],
    [[0, 0, 10], [100, 0, 20], [0, 100, 30]],
)
if hardware is not None:
    rails = hardware.rails
    instances = hardware.sleeper_transforms
    actual_lower_corners = hardware.sleeper_bottoms
```

Signature `build_planar_rail_hardware(xy, triangle, recipe=RailRecipe(),
budget=RailBudget()) -> (PlanarRailHardware | None, report)` keeps the original
horizontal function unchanged. `xy` is a finite metric `N×2` centreline;
`triangle` is one explicit finite nondegenerate height-plane triangle. It selects
the infinite supporting plane, not the triangle's finite contact domain. No
automatic ground/deck/highest-surface selection or source/level/access assignment.

`rail_bed_frame(triangle)` returns read-only origin `(3,)` and frame `(3,3)`;
columns are two orthonormal metric bed axes and its upward normal. Clockwise or
counterclockwise triangles are accepted. Vertical/degenerate/unrepresentable
planes and invalid coordinates raise `ValueError`; no inferred zero heights.
The function rejects an oversized centreline before its conversion/projection.
The same `RailBudget` station/rail-vertex/sleeper limits and local absence reasons
apply to the local construction. Invalid recipe/budget types raise `TypeError`.

The centreline is lifted to the selected plane, expressed in the metric frame,
constructed by the existing horizontal kernel and rigidly transformed back.
Gauge, rail section, sleeper dimensions and spacing remain metric; a vertical
shear of flat hardware would change gauge on a cross slope. Sleeper height and
rail section are measured along the bed normal, not world vertical.

`PlanarRailHardware` owns read-only world rail `Mesh`, `N×3×4` transforms,
`N×4×3` actual lower sleeper corners, plane origin/frame and immutable
`horizontal: RailHardware` retaining the local construction. The shared unit
sleeper is `horizontal.sleeper`; bake with the existing `bake_rail_sleepers`.
`footprints` contains two rail projections followed by each complete 3D sleeper
projection, including its upper corners. These differ from bottom-only
projections on a tilted bed. Result and local arrays have separate ownership;
inputs can be released or changed after construction. Shapes are immutable Shapely
polygons. Retaining the local construction costs additional arrays explicitly
measured in the experiment; this is an offline reference, not an optimized native layout.

The caller must verify all finite bed contacts and conflicts, bind origin/source
IDs/kind, handle local omissions and choose compatible joins. A valid infinite
plane is not finite supporting terrain; a rail ending at a C0 crease is not a
planned switch or train route. No new Plan/Reservation API, consumer admission,
LOD, native/device or structural claim.

Fixed real inputs are `lib/tests/fixtures/planar-rail-vienna.json`: all77 lower and
56 upper previously admitted Wiener pieces with explicit triangles, original
source IDs and archive pins. Tests never update the fixture. The experiment
checks fresh source and installed wheel against the actual original saved
profiles/instances, full finite contacts, dimensions/volume, packed hardware and
the latest whole scene. Public controls include ownership, projection, true
metric gauge, budgets before conversion and invalid planes.

[Source/wheel/real geometry, image and cost evidence](../test/experiments/transport/planar_rail_library_comparison.md)
is separate from native integration #48. No experiment imports in this package,
no copied external implementation or new package dependency.

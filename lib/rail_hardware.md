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

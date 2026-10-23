# Single-core region occlusion — #1141

Decision: retain as a negative control. Do not port this selection to the engine.
A single opaque inner body seldom covers a complete source over a useful camera
region. This result rejects the tested strategy, not regional visibility itself.

`convex_region_occlusion.covered` tests one target AABB, one observer AABB and one
explicitly opaque convex core. Core planes use Float64 coefficients. For fixed
endpoint x the convex body's shadow is convex: x + lambda*(b-x), lambda >= 1.
Checking all eight target corners for every observer corner, then applying the
same argument with endpoints reversed, covers both complete boxes. Different
occluders cannot supply different corner rays: a gap can remain between them.

Slabs propose segment parameters. Strict interior witnesses receive a conservative
forward-error guard; ambiguous boundary cases stay. Exact rational segment tests
independently check accepted corner segments. Tests include a mixed-occluder gap,
inside observer, parallel/touching rays, point-only visibility, rotated planes,
ECEF translation and invalid inputs. Caller owns actual source opacity/core
containment. This is no general native arithmetic, caster or world admission.

## Measured controls

Native inputs on master `482f72c9f`; private exporters/inputs/commands and detailed
reports are linked from #1141. No product hooks, shader changes or cache deletion.

| Input | Observer AABB half extent | Hidden sources | Saved full-hull triangles |
|---|---:|---:|---:|
| Actual capture group, 3,488 sources | 0 m | 29 | 475 / 101,363 |
| Same capture group | 16 m | 8 | 134 / 101,363 |
| Same capture group | 64 m | 0 | 0 / 101,363 |
| Broader cache geometry control, 28,716 records | 0 m | 4,013 | 119,449 / 1,247,519 |
| Same cache control | 16 m | 725 | 18,313 / 1,247,519 |
| Same cache control | 64 m | 14 | 210 / 1,247,519 |

Capture sources are 4–6 km from the observer. The broader control includes nearer
blockers and older cache variants. Exactly identical geometry was deduplicated
from 206,619 occurrences; 28,716 is not a unique-building or current-world count.
Cached plans precede view LOD selection: their control is explicitly all-Shell.
Unusable old index formats and unbuildable sources were recorded and excluded.
They do not establish a current product fault or completeness of this inventory.

Core proposals use convex hole-free non-glazing bodies inset 0.5 m; unsupported
sources remain. Actual complete Float mesh bounds enclose targets, including
submillimetre deviations from prepared double bounds. For the capture group,
9 sampled points in each of 3,145 cores have absolute generalized mesh winding
>= 0.996. Sampling supports containment; it does not prove every interior point.

Candidates: up to eight angular nearest cores per logarithmic distance band,
then centre-ray rejection and 64-segment tests. Candidate misses only reduce
pruning. Any core whose bounds touch the observer box is excluded. For the broad
64 m control: 2,583,872 corner-segment tests, 13,400 saved mesh bytes; all 896
accepted corner segments also pass independent exact rational intersection.
The Python prototype took 4.77 s including core construction/selection, excluding
JSON decoding/oracle generation. No native timing or complete allocation/peak
profile was measured. No native port is justified by this result.

A face-granularity countercheck used all 101,363 actual capture triangles as
oracle-only target boxes. Regional backface support left 59,541 supported
candidates at 64 m; single cores removed another 169 (169/59,541 = 0.284%).
This cost 3,603,392 corner-segment checks and 1.03 s Python selection. All
10,816 accepted corner segments pass the independent rational check. Removed
index bytes are only 169*3*4 = 2,028; no vertex-memory saving is claimed. Existing
backface rejection is not a new gain. Triangle generation before selection is
strictly an experiment oracle, never the proposed production path.

The local SIGGRAPH 2000 extended-projection reference concerns the broader
from-region family. The single-core criterion here is separately derived.
Next work stays under #844/#899: representations/work must survive motion
without retaining all complete hulls; primary, caster and simulation demand
remain separate. This experiment does not increase any LOD error allowance.

```sh
python3 -m unittest discover -s test/experiments/render -p test_convex_region_occlusion.py
```

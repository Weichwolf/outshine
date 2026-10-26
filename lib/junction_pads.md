# Whole-profile junction pads

`outshine_ref.transport.junction_pads.profile_junction_pad(profiles_xy_m,
method="profile_hull")` returns an immutable Shapely Polygon. Each of two to four
profiles supplies `[left, centre, right]` in one explicit local metric XY frame.
Centres must coincide exactly. The primitive constructs the convex hull of the
complete profiles; `method="profile_box"` supplies the enclosing axis-aligned box
for controlled comparisons. Input arrays remain caller-owned and unchanged.

```python
from outshine_ref.transport.junction_pads import profile_junction_pad

pad = profile_junction_pad([
    [[0, -1], [0, 0], [0, 1]],
    [[-2, 0], [0, 0], [2, 0]],
])
assert pad.area == 4
```

Finite coordinates have absolute value at most 1,000,000 metres. Profile width
must be positive and at most 30 metres; centredness/width checks account for
64 Double machine epsilons at the input coordinate scale. The nondegenerate pad
has area greater than 1e-6 m² and at most the squared largest actual profile
width. A collinear straight connection needs no area pad and is rejected here;
callers retain its valid original edge contact. Two to eight extremal profile
points bound construction work and output size; no adaptive search or solver.

The caller chooses actual source connectivity and owns frame, source identities,
access, height, level, finite support, terrain, clearance and budgets. A profile
hull alone proves no route, legal width, turning radius, bridge load capacity or
world admission. The Double planar output needs independent checks after actual
Float32 mesh export. This operation changes no Plan/Reservation contract.

The former explicit ground-only experiment uses this same core with its previous
semantics and eight-pad cap. A separate optional experiment selection permits up
to sixteen new pads at already chosen source landing nodes, including explicit
stair ends and existing bridge ports. It first checks the union of all neighbours
and changes only missing whole profile coverage. Every skipped proposal retains
source profiles and a reason. Neither source axes nor original attributes change.

The independent saved-body reader checks owner and neighbour surfaces separately
over each whole endpoint profile at the same actual flat height. This accepts
both a common area and a genuine shared edge; the complete body must additionally
pass closed topology and connected cyclic vertex links. Old explicitly coowned
pair checks remain separate regressions. Camera choice never changes the plan.

Source and installed controls:

```sh
PYTHONPATH=lib/src:lib/tests python -W error -m unittest test_junction_pads -q
PYTHONPATH=lib/src:test/experiments/transport python -W error -m unittest \
  stair_landing_checks stair_corner_checks -q
```

Actual 24-source input, complete source/installed proofs, same-quality hull/box
comparison, fresh whole terrain/piers and actual 720p before/after rendering are
documented in the [landing comparison](../test/experiments/transport/stair_landing_comparison.md)
for #511/#434; #50 owns the library migration. No experiment imports or SDK/runtime
dependencies enter the primitive. The existing NumPy/Shapely dependencies suffice.

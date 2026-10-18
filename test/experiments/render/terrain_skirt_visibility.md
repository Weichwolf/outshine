# Height-independent terrain skirt selection

Owner #12; native costs #15, integration #531. The current GroundLattice keeps
all four original skirts for every frustum-admitted tile. Hardware backface culling
happens after their vertex work. This experiment moves only that decision earlier;
it does not remove front faces, change terrain detail or prove native performance.

## Plane argument

An ordered boundary has endpoints a,b and positive parameter increments dt.
Its top vertices are p(t) = a + t(b-a) + h(t)u; bottom vertices are p(t)-Du,
where u is the tile extrusion direction and D is positive. Heights may include
arbitrary terrain, deformation and nonlinear curvature compensation.
Each source triangle normal is a positive multiple of `(b-a) cross (-u)`:
height differences vanish in the cross product. Both triangles per segment have
the same orientation and lie in the same plane. Thus one oriented plane per side
classifies every source skirt triangle without reading any height sample.
The statement survives affine transforms, including reflection, when the normal
is recomputed from the transformed edge and extrusion. It requires ordered
fractions and the actual winding; degenerate or unknown planes must remain.

`visible_sides` implements this plane comparison. The independent control realizes
all original source triangles, transforms them into ECEF, and checks their actual
cross products against the eye. Height pages are read for that control, not for
the candidate decision. Nonfinite/underflowed plane arithmetic fails open.
The numerical distance guard is a heuristic here, **not a native float32 proof**.

## Complete cached-input comparison

The existing frozen exports contain 16,255 / 3,757 / 3,643 native pages for
Koerbersee / Tokyo / Wien. In total, 6,055,680 original skirt triangles are
checked; zero discarded triangles face the camera. These are CPU source checks,
not actual GPU invocations. Each report fingerprints all pages and its scenario.
The table counts skirt references for conservative frustum-intersecting pages;
it does not count the terrain surface, buildings or total GPU geometry.

| Input and yaw | Original skirt references | Candidate references | Original / candidate batches |
|---|---:|---:|---:|
| Koerbersee 0 |987648|486656|1 / 7|
| Koerbersee 180 |890624|438016|1 / 6|
| Tokyo 0 |282624|136960|2 / 10|
| Tokyo 180 |337408|162944|2 / 10|
| Wien 0 |295424|144448|2 / 8|
| Wien 180 |203520|98432|2 / 8|

For example, Koerbersee saves (987648-486656)/987648 = 50.73% of those skirt
references, not 50.73% of the frame. All four orthogonal headings are retained.
Grouping by side mask and real/uniform-grid class needs at most 32 groups;
these inputs use 6–12, compared with 1–2 original batches. Extra CPU grouping,
index storage and encoding may outweigh the saved vertex work. No speedup follows
from the counts alone. No native product, shader, image or threshold is changed.

Six focused cases check camera interior/exterior, boundary uncertainty, missing
and degenerate planes, finite extreme arithmetic and invalid arguments. The
independent randomized control uses 1,000 heightfields with irregular spacing,
large height/curvature variation and general affine transforms/reflections.

## Native comparison still required

Keep source/candidate images, the 0.5px surface allowance and full shadow casting.
A native candidate must preserve patch-code/source mapping when grouping, support
perspective and orthographic ViewPosition, and retain all sides when grid order,
planes or float32 uncertainty are unknown. Compare whole-frame CPU/GPU and Metal
stage intervals on identical rotation/movement inputs; no acceptance by CPU counts.

[Geometry Clipmaps, SIGGRAPH 2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
illustrates terrain continuity and transition constraints. The side-plane argument
above is an elementary source-triangle derivation, not a result attributed to that
paper. Earlier alternatives and rejected native trials remain in
[terrain_work_selection.md](terrain_work_selection.md).

```sh
python3 -W error test/experiments/render/verify_terrain_skirt_visibility.py
python3 test/experiments/render/terrain_skirt_visibility.py \
  --pages "$PAGES" --scenario "$SCENARIO" \
  --output "${TMPDIR:-/tmp}/outshine-skirt-plane/report.json"
```

Actual commands, hashes and all four-view reports remain privately under
/tmp/outshine-terrain-skirt-facing-ea54cc945/.

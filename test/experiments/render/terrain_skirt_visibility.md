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


## Native comparison: rejected

A private native prototype on `5af59acb2` retains all height values, shaders,
attributes, the 0.5 px surface allowance and complete shadow casting. It groups
skirt instances by side mask and adds 24,576 static index bytes. No cache or
shared Device ABI changes. A projective cofactor observer supports perspective,
orthographic and mirrored views without assuming the eye is the clip origin.

Grouping initially reordered terrain patches: the final image changed 52,332 pixels.
An independent sorting-only control reproduced exactly the same `f7766d07` image.
Preserving source patch selection/primitive order while remapping metadata slots
restored `c80e3641` / `ddb9ef51` for Koerbersee/Tokyo. Backface removal was not the cause.
A native independent projected-area case passes 6 checks; the existing physical
skirt-gap case passes 36 checks with actual Metal validation. A first invocation
without the required validation environment was a setup failure, retained separately.

A second variant optimizes one union-mask batch plus a complete fallback per
real/uniform-grid class, limiting actual draws to four. Exhaustive independent
assignments of up to six active masks agree on 1,000 random histograms. This is a
restricted batching optimum, not an optimal world renderer or floating-point proof.

Ordinary fresh processes use unchanged 1280×720/60, exactly 60 rotation frames and
60 GPU queries. Every final ordinary before/after image is byte-identical. The
following means divide both complete run sums by 120; no first-frame removal:

| Variant / run order | Koerbersee original -> candidate ms | Tokyo original -> candidate ms |
|---|---:|---:|
| Full mask grouping, A/B/B/A |13.398636 ->13.457937|15.508812 ->15.399583|
| At most four draws, A/B/B/A |13.836817 ->12.984630|15.444281 ->14.461574|
| Same bounded variant, B/A/A/B |13.252711 ->13.672802|16.703026 ->14.649250|

The terrain workload reverses its apparent gain with the opposite order. Matched
hardware stage traces also do not establish a stable whole-frame improvement.
The extra CPU classification/grouping and native state are therefore rejected;
production sources and the executable return to the original 489-object build.
No whole-suite lint, all-place admission or device-budget pass is claimed here.

All four raw stage traces place their largest GPU submission in measured frame 0.
Tokyo includes a 144.101 ms first submission with 86.759 ms between encoder stages
before tonemap. This does not mean the tonemap shader costs 86 ms. Actual pipeline/
resource readiness, synchronization and external scheduling remain to distinguish.
The ordinary frame samples stay complete; prewarming is not permission to hide
startframes or move measurement windows. This larger readiness problem is #16.

Sources, complete 493-unit prototype database, binaries, commands, negative controls
and hashes remain privately in /tmp/outshine-terrain-skirt-native-5af59acb2/.
The saved `rejected-prototype.patch` re-applies cleanly to the declared base;
`rejected-prototype.json` binds all nine source files and executable hashes.

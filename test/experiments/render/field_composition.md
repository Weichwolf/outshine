# Coupled original-group composition

Owner: #1715; native consumer/World admission: #1570. Python research only.

Separate averages do not retain opaque subpixel overlap. Porter/Duff,
[SIGGRAPH 1984, section 4.1](https://keithp.com/~keithp/porterduff/p253-porter.pdf),
explicitly assumes independent coverage when that overlap is unknown. This is
not a guarantee for correlated geometry/backgrounds. The local
`doc/references/vegetation/siggraph/1998-layered-depth-images.pdf`, sections 2–3,
preserves ordering through layers; both papers' diagrams/equations were opened.
No new layered format is needed for the bounded step here.

`field_composition.py` keeps two independent operations:

- `forward_interval` encloses a complete-source AABB in the chosen camera-forward
  coordinate, using the existing outward-rounded arithmetic after every operation.
  Strictly disjoint intervals give sufficient group order. Overlapping bounds
  mean unknown; they do not prove actual interleaving. Bounds crossing the eye
  retain originals. First-hit min/max must never stand in for a complete hull.
- `compose_points` combines linear premultiplied HDR RGBA on matching rays before
  the existing summed-area reduction. It accepts only opaque point masks 0/1;
  fractional filtered coverage cannot be reinterpreted as a point mask. Shape
  equality does not establish matching camera, raster, subpixel positions or world
  frame. The caller must bind them and the complete original/source/light leases.

The point arithmetic is `C = C_A + T_A C_B`, `T = T_A T_B`. The existing alpha
channel stores `1-T`. Applying this after independent area reduction loses
`Cov(T_A,C_B)` and the corresponding coverage correlation. The four-sample unit
opponent gives 0.50 correct RGB versus 0.75 with separate filtering.

The depth helper does not manufacture complete-source provenance from a string;
the native producer/descriptor must supply it. The composition helper does not
grant runtime field admission. Existing `select_field` and its unknown/parent
conditions are unchanged. Exact composition of finite point samples also does
not prove continuous subtexel coverage or a new pose domain.

## Original opponents

`compare_field_composition.py` validates the fixed native source archive/cells,
retains source IDs and regroups complete sources without moving geometry.
The ordered control partitions whole source bounds around a declared 10m empty
control band; crossing sources remain explicit originals. Its image shows only
the selected pair. The interleave control partitions complete IDs by alternating
membership. A separate ray trace of their original union is the opponent.
Both use delivered vertex colours and a simple opaque Lambert diagnostic, not
native PBR, relit Raw fields or a runtime World image.

For cells 25/33 at 1280×720, focal900px and the horizontal control eye
`(-60,1.7,64000)`, 1084 sources/26496 triangles remain accounted for. The ordered
pair has524/531 sources and29 retained originals; complete intervals are
63845.56–64070.10m and64080.25–64350.74m. Its jointly composed point image is
bit-identical to the original-union opponent. Independent filtering reaches
0.109375 maximum RGBA difference. The interleave pair has542/542 sources:
100 common rays see A first,97 see B first. Neither global order is correct.
These are finite CPU controls, not an extension of #1663's pose calibration.

## Smallest native connection

Admit demand using complete metadata before fine IO/field work. Combine truly
registered, ordered point fields before the existing prefix, or resolve whole
interleaving source unions in the existing SourceView/capture job; unknown cases
keep originals. A union capture resolves internal ordering, not composition with
remaining external scenery. No new scheduler, Raw-depth authority or main/shadow
depth policy follows. Keep the existing parent until all replacement obligations
are met.

A joint prefix binds every participating background/source/view/material/light
revision. A changed background needs new joint work; it cannot silently reuse an
isolated foreground prefix. Rebuilding every full joint field each frame is not
the recommendation. The native matched-background export requested from Master
remains necessary before actual field-error/World admission decisions.

## Reproduction and costs

Use `.venv`, NumPy, trimesh/rtree, Pillow and the existing fixed source inputs;
Memray is needed only for the optional cost driver. Set `PYTHONPATH` to `lib/src`,
`test/experiments/buildings`, `test/experiments/assets`, `test/experiments/render`.
Set `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`.

Create a temp camera JSON with `eye`, `basis`, `focal_px`; the control above uses
basis `[[1,0,0],[0,1,0],[0,0,-1]]`. Run:

```sh
.venv/bin/python -B test/experiments/render/compare_field_composition.py \
  --camera /tmp/outshine-composition/camera.json \
  --output /tmp/outshine-composition/originals
.venv/bin/python -B test/experiments/render/field_composition_cost.py \
  --control /tmp/outshine-composition/originals \
  --output /tmp/outshine-composition/cost
```

Freeze producer code before costs. The cost driver compares equal arithmetic and
selection kernels, checked composition, joint-prefix rebuild and held lookups.
Nine rotating CPU/wall rounds and separate native-allocation traces retain actual
work/output/table bytes. Scoped peaks exclude held inputs and are not RSS/UMA or
device times. Source ray tracing/background acquisition are separate costs.

# Ordered floral phases and closed buds (#331)

The earlier Convallaria-majalis study used nine similarly open, uniformly spaced
bells. This adds a prepared floral age sequence, closed buds and arced pedicels
whose posture follows that age. It is still an authored plant interpretation,
not an accepted species, ecological calendar or continuous growth simulation.

## Sources and own method

SIGGRAPH1988 *Developmental Models of Herbaceous Plants for Computer Imagery
Purposes*, section4.1 and PDF-page5/Fig9, was read and the complete page opened.
The example has older flowers near the base and younger buds towards the tip.
Its figure is procedural CG. Independent botanical calibration remains #25.

Three actual new Ghidra12.1.2 decompilations and ELF-label checks are recorded
at [the source decision](https://github.com/Weichwolf/outshine/issues/331#issuecomment-6199963830).
Binary SHA256:9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c.
Their time/profile/transition separation supports the design. Full unknown
field bindings and floating-point ABI reconstruction are not claimed. No
vendor implementation or asset is copied into the repository.

`raceme_phase.prepare` saves the complete vegetative recipe plus strictly
ordered birth phases, side angles and fixed authored transition parameters.
The first birth is0; positive bounded gaps supply the remaining phases and
nonuniform parent-arc stations. All dimensions remain metres; phase values are
unitless study coordinates. `states` evaluates
`g=clip((phase-birth)/0.85,0,1)` and `s=g*g*(3-2*g)`.
Size, direction and pedicel displacement follow g/s. Opening occurs at g≥0.68;
active organs require phase>birth. These constants are our authored choices.

IDs remain leaf(0,i), stem(1,0), pedicel(2,i), flower(3,i). A bud becoming a bell
retains its flower identity and attachment, although the topology changes.
This is selection of static phenological products; there is no claim of a
continuous animation or a visually accepted transition across that threshold.
Vegetative leaves/stem remain fixed during this floral study.

`flower_bud` creates a closed ellipsoid with explicit pole fans, implicit-quadric
normals and seam-aware corner UVs. No coincident pole rings or zero-area pole
faces. The closed convex ellipsoid is an own placeholder bud, without measured
tepals, sepals or anatomy. Open flowers reuse public `Bellflower` from lib/.
Pedicels use the existing analytic circular axis: rigid frames and length map
its chord exactly onto each saved node/end pair. Organs share reference points,
not a welded plant manifold or an inter-organ collision proof.

`phase_mesh` checks active requested organ count before creating any mesh. State
planning remains bounded O(number of flowers), at most16. Partial geometry and
complete geometry sliced afterwards have identical arrays, including faces,
corner normals, UVs and material IDs. New phase meshes use Int32 faces; other
components remain Float64. Earlier LilyStudy recipe playback is unchanged.

## Actual products and demand comparison

Frozen builder59dcb2189, complete terminal0 process:1.152s wall/1.080CPU-s,
81.45MiB peak RSS, recorded host load in `cost.json`. This includes two near
meshes, own texture maps, saved parameters/state, compression and the demand
comparison. It does not include Blender or field meshes prepared separately.

| Phase | Open/bud | Near vertices/triangles | Near payload bytes | Near mesh s | Field triangles/bytes |
|---|---:|---:|---:|---:|---:|
| 0.95 | 4/5 | 3906/7440 | 1083264 | 0.02964 | 2008/293416 |
| 1.30 | 7/2 | 5439/10368 | 1509480 | 0.04306 | 2242/327922 |

Both stages retain the same21 active organ IDs. Near/field state tables and
flower anchors are exact. Whole near surface areas0.0164431/0.0166604m² and
field0.0162494/0.0164605m² are descriptive only, not general LOD/coverage bounds.
All four complete meshes were checked finite, valid-indexed and oriented
against all source corner normals; minimum doubled triangle area≈1.24708e−8m².
A fresh process loads the saved complete recipe with RNG construction forbidden
and reproduces both near meshes byte-exactly.

For the same requested terminal bud, six alternating normal-priority calls:
complete-build-then-crop median≈25.62ms versus demand-first≈1.66ms. Both emit the
same114 vertices/224 triangles/32528 payload bytes. The former builds21 organs
and7440 triangles before discarding the unused geometry; the latter builds one
organ and224 triangles. Complete payload1,083,264 bytes plus selected output
versus32,528 output bytes do not count all intermediate copies.
Separate untimed tracemalloc peaks2,504,022 versus114,323 bytes; retained38,256
versus33,969 bytes. These are traced Python/registered NumPy allocations, not all
allocator calls, copy traffic or process RSS. No global optimality, GPU speedup,
frame budget or device claim follows. Original per-call timings are retained.

## Actual optical checks

Current renderer e0a0cd9fc uses the same whole1920×1080 near camera, daylight,
explicit fill light and procedural leaf maps as the preceding study. An extra
authored ivory-green bud material extends the local preview palette; it changes
no native binding or shared material contract. Blender5.2.2 EEVEE/llvmpipe
software,16 samples. An initial post-render receipt-key typo was corrected;
its failed output was replaced by the completed current early run.

Current early image: closed small upper buds and mature hanging lower bells
are clearly distinct, with arced connections. The leaf pair remains overly
smooth and the main stalk very regular; material/botanical calibration is open.
The whole image was opened alongside the previously opened Fig9.

Late image: seven open bells replace three of the earlier buds while two
upper buds remain. Whole early/late and preceding whole-plant images were
opened at the same camera; foliage and stem are unchanged between the two
new phases. The connection/age gradient is recognizable, without solving the
remaining smooth/repetitive form.

| Image | Setup s | Render s | Complete process wall s | CPU-s | Peak RSS MiB |
|---|---:|---:|---:|---:|---:|
| early | 0.186 | 85.496 | 87.430 | 87.954 | 1982.68 |
| late | 0.201 | 86.271 | 88.453 | 89.080 | 1984.21 |

Both final commands terminate0; setup/render times are software-host numbers.
Blender's existing custom-normal packing limitation from #291 remains; these
pictures do not certify exact decoded instanced/bump normals or native shading.
There is no full species, habitat, wind, animation, LOD or SpeedTree admission.

Five independent new controls cover ordered activation/opening thresholds,
monotone maturation, invalid saved states, closed outward ellipsoid topology,
UV seam/poles, exact curved endpoints/rigid frames, no-RNG selected products,
pre-mesh budgets and exact near/field tables/anchors. Combined with the earlier
affected controls,30 source checks pass. The13 organ/phase controls also pass
against the previously installed public wheel from PR #329; no lib source or
package dependency changes here, so no fresh package build is claimed.

## Reproduce and retain

Use `.venv`, installed lib dependencies and bpy5.2.2;
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`,
`OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, render`LP_NUM_THREADS=1`.

```sh
.venv/bin/python test/experiments/vegetation/raceme_experiment.py \
  --output /tmp/outshine-vegetation-raceme-products
.venv/bin/python test/experiments/vegetation/render_raceme.py \
  --inputs /tmp/outshine-vegetation-raceme-products/early \
  --output /tmp/outshine-vegetation-raceme-shots/early
```

Repeat render with late input/output directories. Current products/receipts:
`/tmp/outshine-vegetation-raceme-products-59dcb2189/`; images/costs:
`/tmp/outshine-vegetation-raceme-shots-59dcb2189/{early,late}/`.
Geometry/state replay is in `replay-geometry.json`. The earlier whole-plant
before reference remains `/tmp/outshine-vegetation-understory-shots-d9f4df918/plant.png`.
These are local research artifacts, with no doc images or replacement gallery.

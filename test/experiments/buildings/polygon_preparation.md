# Reuse polygon preparation — #1703

Three actual terrace/facade controls produce 545 polygon planes, 1,527 outer/hole
rings and 6,109 ring points. The old `faces.plane` gathers and shifts ring vertices
for planarity, then repeats that work for projection. It also reconstructs a
validated `Face`, repeating ring, material, tag and group validation even though
the existing immutable values only undergo hole reversal.

The selected path reuses the outer shifted coordinates already needed for the
normal and tolerance, then retains each hole's shifted coordinates for projection.
It keeps the original per-ring scalar planarity operator and fails at the same
first invalid ring. Reversed immutable rings and unchanged metadata are copied;
all resulting UV fields still pass the existing validation. A `Face` subclass
retains its original constructor path and its additional invariants.
There is no persistent cache, new index, alternate public API or changed tolerance.

The module's old compressed formatting is expanded for readability. The emitted
mesh dictionary uses a literal with the same fields and order. Existing topology,
UV channels, material/group/tag fields and caller interfaces remain unchanged.
Object identity of immutable index tuples is not an output ownership requirement.

## Selection and quality

On the same actual inputs, separate ring-delta reuse and batched planarity checks
gave only small or inconsistent CPU changes. Reusing already validated metadata
with the original scalar predicate also reduced allocations and scoped peaks.
The batched predicate is not adopted: it changes reduction grouping unnecessarily.
The source arrays remain shared exactly as in the actual compiler. A first private
comparison accidentally duplicated one input buffer per face; its report is marked
invalid for locality/cost selection and its superseded traces were removed.

Nineteen focused controls cover the new path, existing Shape behaviour and facade
frames. Independent cases check physical area/corner UVs, hole orientation and
extra UV channels, immutable ownership, the exact 1e-9 m acceptance boundary,
explicit warp permission, early failure before reading unrelated holes and
subclass-specific constructor rejection. Cold untrusted inputs retain validation.

All 545 complete actual plane responses and 39 saved body/facade arrays match the
original. A fresh replay of the three complete hip/broken-hip functional buildings
also retains all 87 old fields byte-for-byte, including near bodies and stairs.
All eight renderer-relevant camera/source/material records and mesh hashes match
the previously qualified inputs. Actual local image inspection remains separate.
No image is published. Native device/frame/World/UMA acceptance is not inferred.

## Reproduce and cost scope

Use the existing NumPy/Shapely geometry environment; Memray is needed for allocation
traces. Extract the old own implementation explicitly, then compare complete
responses on shared actual buffers and whole body construction at normal priority:

```sh
git show 15d8116e1:lib/src/outshine_ref/buildings/modelling/faces.py \
  > /tmp/outshine-polygon-reference.py
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render \
  OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B \
  test/experiments/buildings/polygon_preparation_cost.py \
  --reference-module /tmp/outshine-polygon-reference.py \
  --output /tmp/outshine-polygon-preparation-costs
PYTHONPATH=lib/src:lib/tests OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  nice -n 10 .venv/bin/python -B -m unittest \
  test_polygon_planes test_building_shape test_facade_frames
```

The driver freezes producers, reports nine rotating query rounds, and measures
allocations with native stacks separately. Source loading and plan preparation
precede queries; physical roofs/near expansion are outside the body construction
measurement. Shared existing inputs are excluded from scoped peaks. Patch dispatch
and complete body joining are included; quality/signature checks are outside timing.
The body remains a procedural construction measurement, not an asset DB hit.

For a plane with total ring-point count P and outer count O, the old non-warped
path makes O+2P guaranteed XYZ fancy-index copies; the new path makes P. Multiply
by 3 components × 8 bytes for the captured Float64 inputs. These are a known copy
lower bound; arithmetic temporaries, metadata and all internal/native copies are
not completely measured. Repeated ring-index validation is also removed only for
already validated base-Face metadata. UV validation is retained. Python times,
allocation requests and scoped peaks do not determine native A18/Iris frame costs.
Final producer, terminal measurements, original reports and image evidence belong
in the delivery PR; the complete worldwide device budget remains unchanged.

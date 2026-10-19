# Complete capture-cohort reference frames

Refs #869/#844. `measure_capture_raster.py` compares the same original Fine,
complete selected surfaces and actual captured native geometry as #859/#860.
It uses the existing native input reader and unchanged archived eye/focal data.
No exporter, provider, source build or native execution is added.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error -m unittest test_raster_face_identity \
  test_actual_capture_movement test_building_capture_inputs
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error test/experiments/render/measure_capture_raster.py \
  --input test/outshine/data/render/building-capture-movement/near-cell.json.gz \
  --output /tmp/outshine-complete-near-capture
```

The original 1280×720 camera, native focal length, complete geometry and existing
still/right6m/right6m-turn5deg views are retained. Orientation follows the earlier
declared cohort-facing diagnostic, not an invented archived planar camera.
All four sample centres per pixel are evaluated by rasterizing at2560×1440 and
twice the focal length. This is the same2×2 sample grid, not a quality reduction.
No region is cropped to pass the earlier16384-ray or8388608-control-pair caps.

## Depth, ownership and input equivalence

The existing Float64 depth raster optionally returns the nearest original
triangle index. Equal computed inverse depths choose the smallest triangle
index; the depth-only default retains its original arithmetic and output.
Every stream/triangle retains original source-slot/index and Part/Face metadata.
Unknown FullMesh identities remain unknown.

Comparison keys classify ordered native vertex packets from the same source
slot and stream. Packets contain Float32 position/UV/colour/material metres and
the original packed normal words. Equal keys imply equal vertex inputs under
the same binding; they are not new source/Part/Face identities. Binding arrays
remain separate. Different keys do not by themselves prove different shaded
pixels. Neither equal keys nor the Default-material code review qualify missing
actual per-Piece bindings or GPU execution.

Whole-frame coverage, forward depth and input classes are compared separately
for Fine→Full, Full→Captured and Fine→Captured. Raw complete depth/triangle
arrays and binding/key tables are saved in system temp. Diagnostic depth images
average the four declared samples; they are not native PBR images.

Independent triangle/R-tree rays and direct all-triangle Moller-Trumbore checks
test covered/uncovered points, changed-input locations and every reported maximum
depth-error point. Coverage must match; depth uses the existing rtol1e-10,
atol1e-8 m control. Coplanar ray/raster input-ownership disagreements are reported
explicitly. These numerical control tolerances do not alter one-pixel quality
requirements or certify untested native arithmetic.

The original far cohort can also be compared with **every** stored ray from the
earlier independent reference:

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error \
  test/experiments/render/compare_actual_capture_movement.py \
  --input test/outshine/data/render/building-capture-movement/native-cell.json.gz \
  --output /tmp/outshine-far-ray-reference
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error test/experiments/render/measure_capture_raster.py \
  --input test/outshine/data/render/building-capture-movement/native-cell.json.gz \
  --ray-reference /tmp/outshine-far-ray-reference \
  --output /tmp/outshine-complete-far-capture
```

## Costs and admission

Reports distinguish retained input/frame arrays, owned packet payloads, triangle
counts, raster triangle/pixel candidates, covered samples and largest batches.
Logical extents are not complete allocation/copy counts. Process peak includes
JSON, R-tree, scratch and output compression. A raster batch targets262144
candidates; an individual large triangle can exceed this target, so the actual
maximum is reported rather than claimed as a hard bound. Queries remain finite
under the fixed full-frame profile and original mesh cap.

Ray controls are bounded samples; an optional previous-ray comparison covers
all those stored rays. CPU timings are reference-host measurements, never native
runtime/GPU cost or device admission. The two inputs' null opacity/culling fields
remain null. Positive poses do not establish a continuous translation radius,
full-world source completeness, source LOD, clipping, actual PBR/shadows or frame
budgets. A geometric far-patch difference is not automatically a violation of
the native one-pixel requirement.

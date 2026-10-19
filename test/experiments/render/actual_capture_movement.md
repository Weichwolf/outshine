# Actual native capture at follow-up poses

Refs #859, parent #844. The existing portable input and independent ownership
checker are described in [native_building_capture_inputs.md](native_building_capture_inputs.md).
No additional native exporter, cooker or private source package is needed.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error -m unittest \
  test_actual_capture_movement test_building_capture_inputs \
  test_capture_region_visibility
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error \
  test/experiments/render/compare_actual_capture_movement.py \
  --input test/outshine/data/render/building-capture-movement/native-cell.json.gz \
  --output /tmp/outshine-actual-capture-comparison
```

The output must be a fresh directory in system temp. No pictures are published
in the repository. Input bytes, native provenance, original members, actual
emission modes, exact poses and triangle bindings are recorded in the report.
The binding columns are slot, original source index, Part, FaceAt index, stream
and original chunk/stream triangle. Unknown identities remain -1 in arrays and
null in JSON change records. Full-mesh fallback and independent Fine references
do not acquire invented Part/Face identities.

Three arrays retain native Float32 positions, UV payloads, packed normals,
wall colour factors and material-metre attributes without remeshing:

| Path | Native geometry |
| --- | --- |
| Fine | Complete independent Fine meshes with recessed openings |
| Full | Complete selected native surface inputs before capture selection |
| Captured | Actual visible-face, patch or full-mesh emissions |

Tests compare every expanded attribute byte with the independent native input
reader. The common triangle/R-tree ray reader receives the same rays and original
winding for all paths; native attributes are interpolated at the actual hit.
Two declared Lambert lights use colour factors and interpolated unit normals.
These are geometry/field diagnostics, not native UV textures, PBR or shadows.
Raw UV/material payload deltas are preserved but do not measure texture error.

The native capture eye, focal length and 1280×720 projection are retained.
Orientation is explicitly aimed at this cohort, because the input capture is
cube-wide and does not supply an original planar view basis. Follow-up cameras
move six metres along that basis's right axis and then turn five degrees.
All paths use identical 2×2 subpixel rays over their shared, integer-aligned
screen region; the camera resolution and focal length are unchanged.
Raw arrays retain rays, depths, selected triangle indices and fields for each
pose. Panels enlarge these distant pixels with nearest-neighbour sampling;
their display size is not the measurement resolution.

Fine→Full separates existing level simplification from Full→Captured selection
or patch error. Coverage loss, extra coverage, nearest-source changes, forward
depth, raw fields and premultiplied colour differences are reported separately.
Every coverage/source disagreement and depth delta above 1e-6 m, plus regular
covered/uncovered controls, receives an independent all-triangle Moller-Trumbore
intersection check. Its coverage must match exactly; depths use rtol1e-10 and
atol1e-8 m. The 1e-6 m value only selects additional control rays; it is not an
acceptance tolerance. The independent reader does not use the R-tree or its hit
selection. A single analytic triangle tests interpolation, winding and misses.

The driver rejects more than131072 triangles per mesh,16384 subpixel rays per
view or8388608 independent triangle/ray pairs per mesh/view before allocating
those query arrays. Shared ray queries use32-ray batches; independent checks
use16. JSON parsing, R-tree construction and library allocations are outside
these explicit array bounds. Reports name retained NumPy bytes, query times,
control pairs, whole-run time and process-wide peak RSS with platform units
converted to bytes. They do not claim complete allocation/copy/cache traffic,
incremental memory or native device cost.

The first input is a distant cohort around5.1–5.3 km away, spanning only a few
pixel rows. The comparison explicitly assumes opaque, front-facing triangles;
native opacity and runtime culling remain unqualified. Other world occluders
are absent. Sampled differences do not by themselves prove a violation of the
native one-pixel geometric contract, nor does a positive comparison prove any
continuous reuse radius. Near, thin, masked, disoccluded and overlapping-world
cases, conservative source-relative error bounds, runtime readiness and the
full CPU/GPU/world/device gates remain separate requirements in #844.

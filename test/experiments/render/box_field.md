# Finished box fields: direct DDA and max-height hierarchy

Owner: [#1127](https://github.com/Weichwolf/outshine/issues/1127).
Model: one opaque axis-aligned prism per occupied XY cell; base0, uniform pitch/fill.
The input is a finished procedural product. No geometry or instance matrices are built
by either query. Results are nearest boundary parameters and geometric normals for
t>0.1; benchmarks use normalized rays. Inside origins return the positive exit
boundary; this is not an admission of inside-camera raster equivalence.

## Representation and queries

- Shared direct XY DDA (`column_field_query.trace`) supplies the control; an explicit
  cell-intersection callback retains existing column consumers unchanged.
- A ragged2×2 max hierarchy skips empty/height-excluded blocks front to back.
  Every node encloses descendant footprints, including their fixed edge inset.
- Sorted heights use lossless uint8 palette indices when there are at most256 values
  including the reserved empty0. Otherwise levels retain float32 values. No quantization.
  Zero borders at each level make ragged and long rectangular fields bounded.
- Rays run in packets (default16384, maximum65536). DFS scratch is bounded by
  packet_rays×[(3×tree_depth+1)×4+4] bytes; result arrays and temporary arithmetic
  arrays are separate. The hierarchy is retained once, not once per ray packet.
- Work records count visited nodes, leaf tests and **all** executed3D slab tests,
  including root/child tests. Direct DDA additionally records its XY field-slab tests.
  Different tests have different instruction/allocation costs; visits alone are no speedup.

## Reproduce

Dependencies already used by these experiments: NumPy, Pillow, Trimesh and Rtree.
Use an existing native-control text field (`columns rows pitch fill`, then heights)
or an exported urban patch `product.npz`. Output must be an empty system-temp directory.

```sh
python3 -m unittest discover -s test/experiments/render -p test_box_field_query.py
python3 -m unittest discover -s test/experiments/render -p test_column_field.py
python3 test/experiments/render/bench_box_field.py --input /tmp/product.npz \
  --output /tmp/box-field-high --altitude 1622.0736565468423
python3 test/experiments/render/bench_box_field.py --input /tmp/ready.field \
  --output /tmp/box-field-small --oracle-rays 256
```

The benchmark records source/input hashes, dimensions, occupancy, retained array
bytes, Python/NumPy versions, full parameters and repeated query times. `--supersample 2` traces four samples per output pixel; diagnostic shading
and an identical flat ground are separate from intersection/normal comparisons.
Images remain in system temp. The comparison is not native PBR or a Place rendering.

## Current evidence and decision

One stored complete research product:5962 chunks,624×616 cells,247731 prisms.
Its arrays total515086B:515066 hierarchy bytes+5×4 palette bytes, excluding Python
objects, construction scratch, outputs and traversal scratch. The native611×611 /
247741 control remains a different input. Do not substitute one for the other.

At160×90 rays, direct DDA uses roughly0.02s versus0.08s for the hierarchy in the
elevated full field; executed3D slab tests are100319 versus584722. Low view also
favours DDA, roughly0.02s versus0.09s. The earlier0.61s direct result only rejected
rays at the3D envelope, then started DDA at its2D boundary. It counted empty space
above the roofs. The corrected control clips the actual traversal interval too.
Exact commands/results belong to the linked issue.
Independent triangle controls, random/ragged fields, full-precision fallback,
periodic gaps, empty fields, inside exits and packet partitioning are tested.
The dense no-zero palette counterexample must keep padded nodes empty.

Direct DDA is the preferred current opponent; hierarchy storage alone proves no
traversal advantage. Native comparison does not select a production renderer.
Terrain variation, Earth curvature, filtered pixel footprints, moving-view/TAA
stability, material/light/shadow composition, warm asset IO and both GPU budgets
remain open. Filtered unresolved parents remain #1113; no smaller LOD error follows.

Basis: [GigaVoxels, I3D2009](../../../doc/references/geometry/i3d/2009-gigavoxels-ray-guided-streaming.pdf),
particularly §§2–4: sparse hierarchy; empty-space skipping; front-to-back traversal;
ray-guided residency; filtering. This2.5D box prototype is not the paper's full
voxel streaming/filtering implementation. Figures1/3 and the corresponding text
were inspected. No new runtime dependency or product GPU binding is introduced here.

## Native GPU query control

With an already prepared native archive/CDB and the pinned GLSL compiler:

```sh
python test/experiments/render/bench_box_field_gpu.py \
  --input product.npz --output "$(mktemp -d)" --glslang "$GLSLANG" \
  --width 1280 --height 720 --altitude 1622
```

The same RHI probe runs direct DDA and a bounded stack hierarchy. Both use the
same packed height palette/mips and Float32 camera. Camera rays are generated on
the GPU from64B of basis data; no per-pixel ray list is resident for this case.
The current codec accepts at most256 exact heights; full-precision fallback is
not implemented here. Explicit ray controls cover whole-circle views, ragged/dense
fields, empty fields, parallel/vertical rays, inside exits and street corridors.
Three controls also compare against independent full triangle meshes.

Normal A/B/B/A processes each execute three settling dispatches followed by60
measured dispatches. Upload/pipeline creation and final readback are outside those
GPU submission intervals. Separate instrumented shaders count node evaluations;
their timings are excluded from normal comparisons. `--validation` enables native
validation, including Metal shader validation; these runs are functional evidence.

The fixed std140 block is304B: shape at0, metric at16, view/mode at32, sixteen
level descriptors at48. Descriptors hold byte offset/row stride/columns/rows.
Each16B std430 result holds ray parameter, face code, diagnostic evaluations and
overflow flag. C++ asserts sizes/alignment/offsets and validates input byte ranges.
Ray parameter is distance for normalized rays. Budget checks bound buffers/loops;
overflow is an error, never an accepted approximate hit.

Reports retain commands, source/archive/binary/SPIR-V hashes, actual device,
payload sizes, independent control results and sampled full-field CPU comparisons.
All full-frame GPU results are additionally compared against the first GPU run.
The32-epsilon Float32 tolerance is diagnostic, not a proven LOD/rendering bound.
PNGs use CPU diagnostic shading of actual GPU hits; they are not native PBR Places.
No material/shadow/World/terrain/curvature/filtering or complete-frame budget
admission follows from query time. Vulkan execution remains a separate device check.

# Original material inputs at whole-source hits

Owner #1850, parent #1797. This extends the geometric reference in
`triangle_columns.md` with unchanged original attributes. The existing
`native_capture_meshes.fields_at` barycentric interpolation is reused, not a
second shader/BRDF. Its source derivation and the optically inspected SIGGRAPH
LDI/LDI Tree and Möller–Trumbore references remain the ones recorded there.

## Binding and lifetime

`prepare(field, source)` checks identical original positions/topology and complete
original part/vertex ranges. It owns immutable snapshots of all Float32 normals,
UV, UV1 and vertex colours in their original global vertex order. Geometry is
shared with the already immutable field, not copied again. Original part-end
offsets and material indices remain separate. The caller may release or change
the input arrays afterward; the prepared snapshot remains unchanged.

Part ordinals and original triangle ordinals are offsets in this exact input
group, not World handles or invented per-face SourceIDs. No semantic material
parameters are inferred from material indices 0/1/2. The whole verified 19-file,
1558-source/38476-triangle/78374-vertex provenance is still checked by the reader.

The prepared attribute payload is 3448480 B, including the 24 B of part/material
metadata. Shared geometric payload is 1695316 B. Numeric payload totals 5143796 B
before Python objects; the experiment also keeps original/reference buffers alive.
This is not reduced fine residency or a compact far asset. Known preparation
copies include attribute concatenation and immutable snapshots, at least 6896936 B;
other NumPy copy traffic is unmeasured. Preparation is outside repeated queries.

## Sampling contract

`read` receives the query result of **this field at these exact World/sample
operands**. This ordinary internal caller contract supplies no live revision or
independent admission. The native recomputed Lens values are used in binary64,
not replayed GPU uniforms. Raw depth lifts the actual selected CPU hit back to
the unchanged original geometry frame. The existing interpolator evaluates its
normal, UV, UV1 and RGBA vertex colour; normal interpolation precedes normalization.

Sparse selection occurs before ray and output expansion. The old interpolator
already gathers only hit corners; both methods therefore interpolate the same
609/606 corner rows. The saving is in 203/202 instead of 1600/4275 ray/output rows,
not a claim that all original vertices were previously shaded.

Outputs carry sample indices, triangle/part/material indices, original-frame
position and raw normal/UV/UV1/colour arrays. `uv1` is the literal second UV
attribute: the old helper's `metres` key is renamed, without assuming universal
metre semantics. Colour is a vertex input, not shaded native radiance. Normals
are before native normal packing/vertex transformation. Original/frame coordinate
and normal semantics must be bound by the native consumer before World use.

Input/result shapes, original index ranges and capacities are checked. Hit budget
exhaustion returns no attribute rows and an explicit unknown count before work.
Unevaluated geometry stays unknown; a cancelling interpolated normal also keeps
the original group. Neither raw input validity nor `evaluated` admits runtime
rendering. There is no IO, publication, cache policy or thread introduced here.

## Actual comparison

The existing whole #1838 native packet supplies common 2560×1440 samples resolving
to 1280×720. Geometry still has one native mask disagreement per pose, as in
#1845; this attribute step changes no geometry or image-error threshold. Against
the complete CPU triangle reference, all hit triangle ordinals/depths agree.
All sparse/dense raw outputs are bit-identical on both finite poses.

Hit part counts are 66/125/12 and 71/119/12. No observed hit triangle has differing
per-vertex facade program codes, but that finite observation is not a general
provoking-vertex/material contract. Four normal/vertex-colour diagnostic panels
were opened. They show raw inputs and original part ordinals; native RGB never
fills candidate pixels. They are not PBR images or native colour comparisons.

Seven held-input CPU rounds and separate Memray traces measure each attribute
stage including selection, lifting and rays. Complete geometry queries, loading,
source verification, imports, original buffers and images lie outside that scope.
Preparation is measured separately. Results/payload are identical; scoped peaks
are incremental allocations, not process/UMA/device memory or frame/p99 budgets.
Exact timings, platform/load and owner acceptance live in #1850 and its PR.

## Missing native consumer contract

The current `litVertex.glsl` does more than interpolate raw UV: it unpacks the
native normal word, transforms the normal, computes a smooth `facadeBay` from
each vertex and a **flat** `facadeCode`. `litFragment.glsl` obtains UV/position/
facade derivatives before `applyFacade` and lighting. Interpolated raw values
alone do not define those stages or replace their existing semantics.

`MaterialFieldInput` currently takes original geometry or same-ray stored atlas
samples. `MaterialFieldRasterInput`/`materialFieldRaster.frag` consume already
evaluated normal/base/roughness/metalness/transmission properties, not these raw
triangle/part inputs. A native consumer must bind the actual placement/normal
packing, part material, flat varying and derivative conventions to the existing
shared material evaluation and actual World PBR observer. It must preserve depth
ordering at common samples before filtering. No new bindings/API or shader
implementation is proposed by this Python package. Missing qualification keeps
the original path; runtime World/revision nulls remain null.

```sh
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_source_fragment_inputs test_triangle_columns
python -B test/experiments/render/compare_source_fragment_inputs.py --cost --output "$fresh_temp"
```

Existing .venv/NumPy/Trimesh; optional cost uses existing Memray. Outputs use the
shared fresh-hosttemp helper. No new dependency, pin regeneration or native code.

# Whole native World composition and original fallback

Owner #1797. Producer/reference packet #1831; this is its Python consumer.
The existing MaterialFieldPass supplies all original/terrain/opening points.
No normal World renderer, native asset format or GPU contract changes here.
The first-hit-cell model remains the approximation described in
`field_depth_reprojection.md`, based on SIGGRAPH1998 *Layered Depth Images*.
The previously opened equations/figures and its disocclusion limitation apply.

## Inputs and actual ownership

`native_world_inputs.py` reads the versioned
`test/outshine/data/render/building-world-opponents` packet. It checks all 36
logical arrays, 23 unique physical arrays, full native buffer reconstruction,
actual executed requests/effective cameras, separate World PBR observer and
the 19 unchanged original operands with 1558 sources/38476 triangles. Native
original unions and repeats are independently recomputed, not trusted because
their manifest says they passed. Aliases share read-only arrays.

Manifest and compressed archive are bounded before reading. Hashing, ZIP/NPY
header inspection and decoding consume the same pinned compressed bytes;
replacing a file after validation cannot change the decoded snapshot. Primitive
shape/type/row length and total expanded size precede NumPy allocation. This
offline proof reader intentionally rechecks originals; it is not a cache-hit
consumer and its IO/preparation is not timed as field sampling.

Effective native Lens matrices/pre-view translation are used as real geometric
operands. Their producer explicitly recomputed them rather than reading GPU
uniforms. A whole-original capture-volume check records all 78374 vertices and
its numerical margins. This does not supply a conservative GPU arithmetic bound.
Missing runtime WorldFromField/live revision stay null. They do not prevent
the declared frozen comparison and are never fabricated to enable runtime use.

## World sampling and local fallback

`compare_native_world_fields.py` reprojects the coupled 128² source C/coverage/
raw depth to actual World sample positions, then orders source and native
terrain/opening at those same samples. The joint result is filtered once.
The independent-filter/global-over image remains an explicit negative opponent.

The output profile is always 1280×720. `--sample-axis 1` uses the existing native
1280×720 point raster; it is a finite pixel-centre comparison, not a converged
pixel integral. `--sample-axis 2` requires an actual 2560×1440 native reference
and resolves its common 2×2 samples to 1280×720. Applying it to the old 1280×720
packet is refused; it must not silently qualify 640×360. Odd crop origins and
ends retain global pixel phase; all added border samples are proven clear by
the lossless full-buffer reconstruction.

The explicit working comparison limits are maximum 0.25 per existing native
linear HDR RGB component and 0.125 visible-source coverage per output pixel.
CLI values/report units make them reviewable; these are research limits, not a
new general product/camera-response budget. They allow nonzero error and do
not require bitwise proxy reconstruction of originals. The current unfiltered
point inputs fail them; their sparse point aliasing does not establish the
filtered720p result either way.

Final union alpha is insufficient: opaque terrain can make it exactly 1 even
when the source silhouette is wrong. Visible source coverage is therefore
measured separately after depth ordering, before/after the common sample mean.
The finite near-pose source has 11 mask disagreements despite zero union-alpha
error; the geometric 0.0763px bound alone cannot accept those samples.

On excessive error, unresolved equal depth or incomplete model evaluation, the selected whole source
group retains its original point product. Those actual original samples are
reordered with unchanged terrain/opening and filtered at the same locations;
the oracle union never fills failed pixels. Current chosen paths reproduce all
four native unions and their raw sample depths exactly. Depth stays unfiltered
at the actual sample locations; only coupled colour/coverage is averaged.
Stale PBR-view reuse is refused before reprojection;
identical-observer repeat names may reuse their semantically identical capture.
Per-frame comparison against an oracle is not proposed as runtime admission.
Native live selection needs a qualified pose/error/light contract separately.
A missing qualified envelope selects the original before field sampling.
Fine original IO/residency remains necessary on group fallback. Other groups,
shadow/simulation needs and their independently owned products are unchanged.

## Avoiding unnecessary World rays

Project the eight corners of the complete declared capture volume, not a
visible-hit or selected-source box. Select its World rectangle before expanding
sample coordinates. The finite fixture also checks that all original hits lie
inside it. A volume crossing the eye or leaving the bounded reference crop
retains the original group. The2px numerical/diagnostic border is not a newly
certified GPU margin.

On identical whole model outputs, near pose needs 484 instead of 3136 rays and
the side pose 1240 instead of 4880. Visited cells remain 584 and 8934 respectively:
the omitted rays would not visit a source cell. Colour/depth/model-evaluation
arrays remain exactly equal. This equality qualifies an optimization of the
same approximation, not its image quality against original geometry.

Seven held-input CPU rounds and separate native Memray traces compare full-crop
and metadata-region construction. Both include complete output arrays and the
selected region scatter. Reports name ray/cell counts, allocations, requested
and scoped peak bytes, result payload and known scatter bytes. These explicit
array copies exclude other NumPy traffic; no physical memory bandwidth, IO,
GPU residency, UMA peak or native/device/p99 result is inferred. Full original
loading, fixture checks, correctness/rendering and imports precede timing.

## Separating capture and novel-raster error

The existing #1716 driver also keeps complete original-Capture and field-Capture
C/coverage/depth pairs separate. No colour/mask/depth is mixed across them.
The original-capture depth-cell model still reaches 0.3125 native SS16 coverage
error versus the World original; the field/original reprojected captures differ
by at most 0.0625 coverage on that opponent. Their HDR maxima are 1.8153 and 1.8680,
not a monotonic improvement from using original capture. Fixing only the field
capture approximation cannot remove this observed novel-raster residual.
The residual also includes resampling, disocclusion, frozen radiance and finite
GPU/microcamera conventions; it is not claimed to be pure geometric error.

## Reproduce and remaining qualification

Existing .venv; no new dependency. Ordinary consumer/tests use only lib/src and
the render experiment directory on PYTHONPATH. The optional old capture-pair
cost comparison additionally uses the existing buildings allocation helper;
that import is lazy and cannot burden ordinary render consumers.

```sh
PYTHONPATH=lib/src:test/experiments/render \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python -B -m unittest \
  test_native_world_inputs test_field_depth_reprojection test_depth_reprojection \
  test_field_composition test_field_external_depth
python -B test/experiments/render/compare_native_world_fields.py \
  --cost --output "$fresh_system_temp_output"
```

Outputs are fresh host-temp paths checked by experiment_output. Actual native
point images, comparison and selected-path fallback panels must be opened.
Normal tests do not regenerate native arrays/pins or publish doc/README images.
Tests include amended hashes/incorrect unions, changed effective camera/crop,
sparse oversized payloads, forged huge NPY shape before NumPy, input replacement
after validation, PBR mismatch, global sample phase and the opaque-union trap.

Remaining P0 input is the common subpixel native World reference requested in
#1797/6528716212. It will decide filtered image acceptance at the same 720p output
profile; no smaller profile or silent higher error limit replaces it. Capture
pose/visibility/radiance/error qualification, actual native World consumer,
GPU depth/shadows/resource lifetime and both full device budgets remain open.

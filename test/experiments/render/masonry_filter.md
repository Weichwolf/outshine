# Metric masonry filter comparison

[#306](https://github.com/Weichwolf/outshine/issues/306) owns this native material
experiment. The kernel is not activated in the product. Metric placement/LOD,
actual production raster, lighting and device budgets remain integration work.
The proposed recipe consumes existing generated WallVariant6; explicit building
colour currently selects variant0 and must retain that precedence.

## Recipe and composition

A250×80mm module with10mm joints repeats over two staggered courses. Brick area
is0.84, mortar0.16. The signed field is `0.84-brickCoverage`, weighted by the same
opaque-wall/band contribution as the existing facade. Native wall colour and
variant6 tint yield linear RGB(0.4588,0.3053,0.208). The experimental mortar target
is(0.48,0.46,0.42); the complementary brick colour preserves the original mean.
Amplitude is limited analytically at0/1 rather than by a biased final clamp.
Roughness uses a bounded0.04 field amplitude around its original mean. This is
parameter filtering, not an exact mixture of BRDFs. No relief, stain, weather,
brick-by-brick variation or new source-material semantics is claimed yet.

Windows, frames and bands use the existing common coverage filter. Candidates
compare a point field, a separately filtered box and jointly weighted1/4/9/16
subcell boxes. The joint candidates retain the final nine-cell window coverage;
field bounds follow that coverage. The prototype still computes final coverage
and field masks separately. A future integrated kernel must account for this
work explicitly, rather than treating the prototype's times as a frame estimate.

The periodic integral is the existing `periodicBand`, with exact flat-interval
returns to avoid subtractive noise at tiny spans. Binary-exact repetition scales
(4,6.25) and FMA reduce phase before subcell offsets. The unreduced absolute
four-cell control misses the unchanged2e-4 parity threshold on RTX3060
(max0.000233345); the positive paths reach at most0.000130027. This arithmetic
fix does not recover precision already lost in an asset/position input.

## Independent reference and native execution

4434 cases use real native profiles0/1/8, random and explicit opening-corner
positions, rotation, shear, anisotropy, zero/rank-one footprints and three
61-step motion tracks. Inputs/profile values are rounded to actual FP32 before
comparison. Shapely clips the physical pixel parallelogram against individual
brick/opening/band rectangles. Degenerate maps instead clip their unit screen
pixel by half-planes, preserving their correct line density. Eight known-area
controls check brick/mortar points, a complete repeat and one-dimensional limits.
This reference shares recipe dimensions, not the candidate's integral algorithm.

Six ordinary native captures produce26604 actual outputs. The192 sign/swap
controls differ by at most1.20e-7;30 colour/roughness controls preserve boundaries,
full glazing and the complete-period mean. Long thin footprints retain visible
bed joints; no largest-derivative fade erases a still-resolved direction.

These errors bound the added masonry contribution only. The existing final
window-coverage approximation has its separate #285 error; this table does not
bound the complete facade image or lighting.

| Candidate | Masonry-only maximum linear RGB area error | Field RMS across three profiles |
|---|---:|---:|
| Point | 0.233752 | 0.245–0.368 |
| Separate box | 0.065881 | 0.0272–0.0422 |
| Joint box | 0.065881 | 0.0274–0.0422 |
| Joint4 | 0.026344 | 0.0117–0.0160 |
| Joint9 | 0.017436 | 0.00735–0.00982 |
| Joint16 | 0.013182 | 0.00523–0.00670 |

Joint16 is selected among these tested candidates for the next integration:
smaller errors than nine, with comparable observed GPU time in the repeated
matched batch. It is not a globally optimal filter or a final device decision.
Translation false-step RGB RMS falls from0.0309–0.0653 for point sampling to
0.000456–0.000958 for sixteen; rotation/growth also improve in this cohort.

Matched115200-probe batches contain32 dispatches. RTX3060 GPU debug-label times,
including dependency barriers but excluding copies, were: point2.544ms,
separate box2.359ms, joint box2.202ms, joint4 2.156ms, joint9 6.852ms,
joint16 4.034ms. Reverse-order repeat: joint16 3.793ms, joint9 3.782ms,
joint4 2.617ms. The first nine-cell result was not reproduced. CPU jobs and the
interactive desktop were active; no exclusive-host, kernel-only or A18 claim.
Profiling uses the existing private a9 hostQueryReset-enabled binary; every
output is byte-identical to the ordinary native capture tiled to the same batch.

The batch requests14748152 logical buffer bytes and7374076 copy bytes, including
upload/readback protocol overhead. Per subcell the source has four brick and six
facade periodic-integral calls; only opening/band affect the field, so the driver
may eliminate unused pane work. No hashes or texture fetches. Final window
coverage is additional. Physical driver allocations/peak and runtime resource
costs are not established. The independent CPU reference child peaks near49MiB
including imports/NumPy/Shapely; this is unrelated to product memory.

`masonry_image.py` captures73728 affine pixels per method through the actual
compute shader: flat baseline, point and joint16. Its local PDF was optically
opened: joints read in the near wall, the window remains clear, and sheared
stair-stepping is reduced. These are actual component colours, not a production
fragment raster, illuminated image, streaming proof or Place acceptance.
All six candidates translate to MSL2.3 with identical four bindings,32B
probe/result strides and16B uniform block. Translation is not Metal execution.

## Reproduction and source

Use NumPy, Shapely, Matplotlib, glslangValidator and the unchanged common
`NativeGroundLitterProbe.cpp` linked against a matching ordinary native archive
and headers. This run reused the frozen01518e678/b42fbadf4 archive (SHA256
32d03a1b6b5e3b30ebced3f13cf030e090c858922a862c274b5305acd4722ae1), not a new
backend copy. Probe/link source and commands remain in #268/#297; no C++ changes.

```sh
PYTHONPATH=lib/src python3 test/experiments/render/masonry_native.py --binary /path/to/native_material --output /tmp/outshine-material-masonry-native
PYTHONPATH=lib/src python3 test/experiments/render/masonry_image.py --binary /path/to/native_material --output /tmp/outshine-material-masonry-image
```

Only the reference child is nice10; GPU work runs at normal priority. Outputs
require system temp. Success is cleared before preparation, native captures
reject stale/incomplete output, and the source palette is never changed.
SIGGRAPH1980 Feibush/Levoy/Cook pages3/7 and SIGGRAPH2013 The Order page18/Fig16
were optically inspected; see `doc/references/README.md` and #306. GPU Gems24.2/3
is the additional primary integral-filter source. No foreign shader code,
bitmap material or presentation image is added under doc/.

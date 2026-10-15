# Facade coverage over oriented pixels

[#285](https://github.com/Weichwolf/outshine/issues/285) replaces the complete
UV bounding box with nine analytically filtered subcells for oblique facade
pixels. Opening, pane and storey-band coverage share those subcells before one
material composition. Axis-aligned and zero-derivative pixels retain the existing
box formulas. Profiles, source colours, geometry, glazing semantics and negative
roof/plinth codes are unchanged. No new resource, binding, pass or asset format.

Each subcell is still represented by its projected box. This is a bounded
approximation to a pixel parallelogram, not an exact integral or global error
bound. The filter has at most nine box evaluations (54 periodic-band calls before
compiler sharing), independent of footprint size. It retains the previous
minimum width of0.0001 per box axis. Phase reduction happens before subcell
offsets, preserving representable integer translations even at high storeys.
The band and opening are disjoint; pane is a subset of opening. Final roundoff
clamps preserve that material partition.

## Comparison and decision

`facade_footprint.py` compares the old box,4/9/16 subcells and screen-space polygon
clipping on identical real native profiles and explicit synthetic footprints.
The clipping candidate integrates near pixels and fades to the old box between
one and two repeats, limiting enumeration to nine rectangles per mask. That
cost bound does not make its far approximation exact. It remains experiment
code, outside the production shader.

The independent area reference intersects actual pixel polygons with periodic
opening/pane rectangles using GEOS through Shapely. Degenerate rank-one cases
instead intersect the unit screen square with geometric half-plane polygons;
there is no inversion of a singular UV Jacobian. Profiles and pane/frame constants come from the shipped headers; colour and
roughness are explicit component parameters from the pinned palette input.
They are not a claim about the current building wall colour. Inputs are converted to float32 before
reference evaluation, so high-phase tests use the values the GPU actually sees.

The cohort contains9514 cases: five facade profiles, rotation/shear/scale
counterexamples, eight91-step motion tracks, rank-one/negative/high phases,
full glazing and coarse integral-period controls. Maximum opening/pane area
error is0.34087 for the box and0.07859 for the production filter; RMS falls from
0.07218 to0.01101. The declared motion tracks reduce erroneous step changes by
roughly2.05–3.41 times. These are finite-case observations. Some far, strongly
correlated footprints remain approximate. Sixteen subcells improve area error
further at additional work; native GPU measurements accompany the choice.

Actual captures are checked against independent NumPy implementations of each
candidate, including full colour/roughness composition and band partition.
The parity threshold2e-4 concerns FP32 implementation agreement, not physical
pixel-area accuracy. Measured worst mask parity is2.05e-5.768 extra native
controls check sign/swap invariance, exact representable integer phases and the
axis/zero-derivative path. Separately optimized kernels may reassociate the
same formulas: fast-path agreement uses an explicit eight-FP32-rounding absolute
budget, not a byte-equality claim. Normal tests/pins are never regenerated.

## Reproduction

Requires Python3, NumPy, Shapely2 and glslangValidator; Pillow for the image
chart. Build the shared `NativeGroundLitterProbe.cpp` against the platform's
regular production archive and its matching native headers. Native link flags
are available through `sh test/run.sh --native-link-flags`. The same versioned
`test/outshine/data/materials/ground-litter.palette` is input on every device.

```sh
PYTHONPATH=lib/src python3 test/experiments/render/facade_footprint.py --binary /path/to/native_material --output /tmp/outshine-material-facade-filter
PYTHONPATH=lib/src python3 test/experiments/render/facade_footprint_image.py --binary /path/to/native_material --output /tmp/outshine-material-facade-filter-image
```

`GLSLANG` can select the installed compiler. Output must be explicit system temp.
The caller removes previous success markers and native captures before launch;
failed execution cannot reuse an archived output. The image chart contains three
160×96 affine wall images, enlarged2× for inspection: old/native/reference.
Pixel spacing matches the declared derivatives. It is native unlit material
colour, without a Place, lighting, TAA or frame-budget claim. Its RGB RMS falls
from0.01169 to0.00168 against independent polygon areas.

GPU comparisons use identical data and actual GPU intervals, excluding upload/
readback. Repeated-dispatch batches include their dependency barriers; amortized
values are component observations, not isolated kernel or frame latency. Logical
buffer/copy bytes exclude allocator granularity, uniforms, driver/process peaks
and reserve. Metal execution, identical lit Places, A18 and integrated frame/
memory counterchecks remain with Master after the independently verified merge.
#278 retains the later coherent coating integration.

Basis: SIGGRAPH2009 *Sparse Gabor Noise*, section5.2/figure6 (PDF page6 previously
optically inspected), motivates preserving the footprint's orientation; this
filter does not implement Gabor noise. [GPU Gems25](https://developer.nvidia.com/gpugems/gpugems/part-iv-image-processing/chapter-25-fast-filter-width-estimates-texture-maps)
explains the box estimate and its overblur tradeoff. Source/material composition
references remain in #196/#269. The clipping algorithm is a finite convex
half-plane clip; GEOS provides an independent reference implementation.

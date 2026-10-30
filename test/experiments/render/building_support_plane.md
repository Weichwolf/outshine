# Building capture coordinates and physical support

Owner: [#1759](https://github.com/Weichwolf/outshine/issues/1759), Python only.
The existing capture-facing prefix leaves5.89%RGB/3.31%coverage error against
originalSS64 after removing the repeated-filter comparison error. This packet
separates its geometric/sampling components and tests a different physical
support for the same first-hit field. No new source/view/renderer is introduced.

## Geometric change

Keep capture centre C, half extent H and orthonormal axes R/U/d unchanged.
Normalized capture coordinates u/v identify the same source cells. Fit all43049
occupied first-hit depths to `h/H = a*u+b*v+c`, ordinary unweighted least squares
with three parameters. Native depth decodes as `h/H = 4*depth−2`. No query image,
colour, selected interior points or original mesh determines the fit.

The physical support is `C+H*(u*R+v*U+(a*u+b*v+c)*d)`. Its normal is proportional
to `d−a*R−b*U`, through `C+c*H*d`. Intersect camera rays with this plane; measure
UV with the original C/R/U/H. Capture direction and physical normal are separate
values. The same field/prefix and two triangles suffice; no sample quad mesh.
The coefficients are approximately(-.025828,-1.016010,-.010386), rank3 and
condition4.495. Source depth RMS drops99.62→5.45m. This minimizes source depth
squared residual, not image error; residuals do not certify unseen surfaces.

## Same inputs, independent controls

The existing loaders bind all19 original source hashes,1558 sources/38476
triangles, the old795a1cf4 capture/d485bf1c stored field and all native buffer
hashes. The archived512-source sample queries are now consumed directly.
Full1280×720 query rays remain unchanged; the96×128 crop only stores them.
Fixed capture lighting is explicit; the earlier GGX-only control is separate.

|32px query against the same native originalSS64 |RGB relative L1 |Coverage L1 |
|---|---:|---:|
|Old Float64 prefix, capture-facing support |5.89177% |3.30513% |
|Same prefix, fitted support, rectangle approximation |4.05622% |1.68948% |
|Independent projected source-cell screen areas, fitted support |4.04778% |1.63701% |
|Native depth-preserving86098-triangle sample opponent |4.05839% |1.77662% |

A fixed fit also improves each of the four existing128/64/32px/yaw poses; their
other native originals use only4² samples and remain finite, less converged
comparisons. This does not define a general admitted pose or LOD range.

Existing GEOS triangle/pixel-area integration supplies continuous original and
flattened-capture silhouettes. Five signed maps telescope exactly to the native
prefix coverage error: pixel reconstruction(.03282%), capture predicates(.03237%),
source point discretization(1.51992%), flat projection(2.71976%), and GEOS-original
versus nativeSS64(.29925%). Parentheses are component L1/original coverage norms;
**these norms do not add**, since spatial errors cancel. The continuous fitted
capture differs from continuous original by.42349%, versus2.71976% before.
All14 source512 and3 source128 GEOS/native predicate mismatches are retained.
GEOS/native differences include finite sampling, camera jitter and numerical
backend predicates; this is no certified reference convergence or native bound.

A second independent control clips each constant RGBA source cell against screen
pixels. Contained cells use polygon area; boundary cells use existing GEOS.
Binary mask row unions agree in alpha within3.3e−12. Flat support reproduces
existing Float64 prefix values within2e−12 relative RGB. Tilted source rectangles
are approximate: fitted prefix versus projected-cell control differs.30929%RGB/
.19869%coverage; maximum bounding rectangle excess over the full crop is9.81%.
No claim of exact perspective filtering is made. Fractional premultiplied cells
stay fractional; opaque-mask union explicitly declines fractional coverage.

SIGGRAPH-first references, inspected visually: Feibush/Levoy/Cook,
[Synthetic Texturing Using Digital Filters1980, PDFp2/Fig1](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf)
separates texture/object/image coordinates and the mapped pixel footprint;
Shade et al., [Layered Depth Images1998, PDFp4/Eq5–9 andp5/Fig3](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
separates planar perspective warp from residual parallax. This packet retains
one existing first-hit field and tests its support, not the paper's LDI renderer.

## Reproduction and limits

Use the existing.venv, NumPy/Shapely; Matplotlib only for private figures.
Set `PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings` and
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`:

```sh
python -B -m unittest test_building_support_plane test_field_view_plane test_field_plane_integrals test_triangle_coverage_reference
python -B -m compare_building_support_plane --output /tmp/outshine-research-plane/run
python -B -m plot_building_support_plane --study /tmp/outshine-research-plane/run --output /tmp/outshine-research-plane/comparison.png
python -B -m plot_building_support_plane --study /tmp/outshine-research-plane/run --output /tmp/outshine-research-plane/components.png --components
```

Freeze actual sources first; fresh private system-temp outputs only.106 focused
controls pass including affected default samplers, inverse/forward coordinates,
known/curved/rank-deficient depths, frame transforms, actual readback bindings,
bounded expansion, independent area controls and signed error cancellation.
Both final scientific figures were opened and inspected. Numerical cell/GEOS
work is diagnostic, not runtime work. Native fit/storage/support integration,
changed lighting, other content/poses, World/Main-/Shadowdepth, movement/history,
reference convergence, p99 and full-system UMA admission remain separate.

## Measured work and memory

One Float64 prefix remains8421408B. At32px the projected sampling rectangle
changes1190→1260 pixels,19040→20160 logical texel fetches; each pixel still uses
16 fetches. More accurate projection increases this work5.88%, rather than
claiming a reduction. A fitted plane retains only three coefficients(24B as
Float64); native layout/persistence is not specified by this Python experiment.

Nine alternating normal-priority rounds at720cb9990, single BLAS/OMP thread,
concurrent host load29.85/28.47/28.55, give these medians and separate Memray1.20
measurements including Python allocators:

|Operation |CPU ms |Wall ms |Allocation events |Requested B |Tracked heap peak B |
|---|---:|---:|---:|---:|---:|
|Fit once, all43049 depths |6.504 |12.030 |395 |7329361 |3456704 |
|Old flat sampling, existing prefix |5.343 |10.365 |2233 |6604504 |682664 |
|Fitted sampling, same prefix |5.641 |12.535 |2734 |6996694 |717296 |

Different quality/work is explicit; this is no equal-quality speedup claim.
Known copied fit payload subset2238548B comprises172196B masked depth,
344392B depth cast,1033176B design stack and688784B coordinate indices; internal
linalg/NumPy/GEOS/IO/assignment traffic is unmeasured, not zero. Requested
allocation bytes are neither copied/retained bytes nor RSS.

The complete numerical diagnostic also integrates original silhouettes and43049
RGBA cells, and runs eight prefix samplings. A separate normal228d46c0f process
uses1.259sCPU/2.673swall verification/decode and8.983sCPU/13.808swall analysis,
144846848B OS peak and7241728B held output arrays. Separate full-analysis tracing
records19695493 allocation events,3922584839B requested and42554916B tracked
heap peak. These large diagnostic costs do not belong to the fitted-plane
runtime. Input decode is outside each tracker; prefixes also outside individual
fit/sampler trackers. Tracing/report reading process peak272441344B is separate
from normal-run RSS. Python/host trends replace no native A18/Iris/device budget.

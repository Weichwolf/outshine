# Directional grass contacts and ground gaps — #462

#451 needs actual grass/substrate composition. Root density and height alone do
not determine visible blade coverage. This bounded Python reference derives
unlit geometric contacts from the same saved roots and shared graminoid shapes;
it introduces no common library, native cache, shader or material API.

## Sources and semantics

First consulted: SIGGRAPH1998
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
PDF page4 optically opened: depth/parallax differs from a flat sprite.
Bruneton/Neyret
[Real-time Forests](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf),
PDF page4/Fig3 optically opened: seed and coverage products share a population;
terrain coverage loses vertical silhouette. These motivate the separation, not
a copied forest lighting model. No new numeric SpeedTree oracle is claimed.

A query owns a rectangular metric ground footprint at z=0 and a unit direction
d toward the eye/sky, d.z≥0.25. Pixel centres are uniformly spaced in ground XY.
The named authored frame is `grass-study-ground-1`: origin(0,0,0), orthonormal
X/Y tangent axes and Z up, all lengths in metres; no geodetic location is claimed.
The line through ground point q is q+t*d, t≥0. Project a vertex p to
p.xy−p.z*d.xy/d.z. Maximum nonnegative p.z at each covered sample is the
first contact from the sky, not the first surface from the ground. Geometric
face normals remain authored; opaque coverage is two-sided. This is binary
blade occlusion, **not** optical transmittance, leaf R/T or a grass BSDF.
Validity is `face>=0`/binary coverage, never height alone: height0 can be a valid
ground-plane contact. A gap has face−1 and zero premultiplied attributes.

Bounds of possible root positions expand the footprint by the maximum horizontal
form radius and its maximum height times d.xy/d.z. Missing neighbour support
fails before projection/expansion. This guard is necessary: an outside root can
project a blade into the queried ground. The selected stand uses Poisson seed113,
150 roots/m² in a saved4×4m support region; query footprint is1×1m. It contains
2368 canonical roots and eight shared12-blade forms, seeds17..24, three segments,
uniform scale0.85..1.15. These existing authored forms are unidentified graminoids,
not calibrated species or accepted pasture/steppe botany.

## Work before detail

The complete reference expands284160 triangles for independent proof/rendering.
The actual instance-query path receives only329952B of saved form/pose arrays;
the full expanded arrays are deliberately absent from its input. Transform eight
prototype-box corners per root, reject boxes outside the projected footprint,
then expand only selected roots. Linear projection of the enclosing boxes is
conservative. Canonical face keys preserve root/triangle sources and batch ties.
No growth, new root placement, shape evaluation or RNG occurs in that query.

Existing bounded triangle/pixel batches provide barycentric contacts. Two winner
reductions, sorting and scatter maxima, produce exactly equal face IDs, heights,
normals and coverage, as does the instance-selected path in all three trials.
Box filtering reduces transformed geometry, not the same true fragment work.

| Direction | Selected roots | Expanded faces | Full scatter / instance median wall ms |
|---|---:|---:|---:|
| Top |278|33360|415 /138|
| East,60° from zenith |417|50040|846 /639|
| West,60° from zenith |430|51600|958 /708|

Full geometry input13,639,680B becomes1,601,280/2,401,920/2,476,800B; associated
projected-vertex copies800,640/1,200,960/1,238,400B. Source-key arrays add
266,880/400,320/412,800B. All2368 root boxes/18944 corners are still inspected.
Bounding-box triangle/pixel pairs827394/5171694/5719565 yield only30590/160639/
170607 accepted fragments. The large remaining empty raster work is a measured
next target. Scatter clears4,194,304/20,971,520/23,068,672B across batches;
internal allocator traffic and total allocations are not measured.

## Independent checks, filtering and actual images

GEOS unions projected polygons after independent z=0 clipping. Explicit1e−12m
union precision handles nearly coincident blade edges. Unsnapped GEOS3.14.1 union
lost five sampled east-view contacts despite valid individual polygons and
positive independent triangle-ray hits; the failed run is retained. This is not
an exact-arithmetic continuous-area oracle or a global error certificate.

All196608 sampled memberships agree exactly with the precision-grid unions;
192 independent Float64 plane/Gram rays agree in hit state and within2e−11m in
height. Fixed area acceptance was1percentage point before the run:

| Direction | Continuous union % | Sampled256² % | Absolute error percentage points |
|---|---:|---:|---:|
| Top |31.4609|31.4072|0.0537|
| East |86.1205|85.9787|0.1418|
| West |88.1292|88.0371|0.0921|

Summed projected area differs again:0.4666/2.4560/2.6109m² on a1m² footprint.
An analytic duplicate/shift counterexample keeps summed area0.5m² while true
union changes0.25→0.375m². Poisson opacity is not an identity for these fixed roots.

Equal-area2×2 mip means preserve coverage and premultiplied height/normal moments
to1e−13; never renormalize the mean normal as if it were one resolved leaf.
Each256² contact product returns2,883,584B plus3,495,240B of mip arrays. Source
IDs select the original root table. A fresh installed-library reader hashes all
required inputs and reads complete products with planning, shape/vertex
expansion and projection forbidden. It is not a newly built-wheel claim.

17 source/installed controls include overlap, first-from-sky depth, clipping,
outside roots, empty/invalid demand, batch/order/source identity and mip moments.
The whole comparison, independent geometry and IO take45.17s wall/45.14CPU-s,
271.2MiB peak RSS, one numerical thread, source6987f16b2. Generation medians above
include instance bounds/expansion but exclude IO; they are not frame budgets.

Actually rendered/opened1920×1080 top/east pairs show physical blades on the left
and a generated cached mask on the right, identical footprint/camera/flat colour.
Ground gaps agree spatially, but the enlarged256² product visibly aliases thin
blades. It is a fixed-direction diagnostic, not accepted near detail or a natural
grass render. EEVEE/llvmpipe software, four samples:23.42s/1971.5MiB whole process.
Actual source7429bbf5d; renderer inputs are byte-identical to the final product.

## Reproduce and limits

Use `.venv`, existing lib dependencies and bpy5.2.2; source `PYTHONPATH` includes
`lib/src:lib/tests:test/experiments/vegetation`, numerical/llvmpipe threads1.
Commands: `python -m unittest grass_projection_checks test_ground_cover test_graminoid`;
`grass_projection_experiment.py --output <fresh-temp>`;
`grass_projection_reader.py --inputs <temp> --output <temp-json>`;
`grass_projection_preview.py --inputs <temp> --output <fresh-temp>`.
Original receipts/inputs/images are linked in the PR and stay in system temp.

This is cold finite-patch reference construction, not direct coarse world-scale
generation. No angular interpolation, silhouette/parallax, movement, wind,
correlated light/view masking, leaf transmission, shadow filtering, sloped ground,
whole grassland/species coverage, GPU/device or native/LOD acceptance follows.
Material/Light still own shading/composition. Separate view/light coverage queries
cannot be multiplied to infer correlated illumination of visible leaves.
Master confirms this research scope in #462; native contracts remain unagreed,
and #451 remains open. No paused Material/Light work is resumed by this delivery.

Follow-up #465 selects bounded row candidates before the same barycentric test.
The instance-query default now uses rows; explicit `sampler="boxes"` retains the
measured reference. Original256² contacts remain exact; the equal-quality512²
comparison and current diagnostic images are described in [raster_rows.md](raster_rows.md).

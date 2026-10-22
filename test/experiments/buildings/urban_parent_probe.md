# Direct urban parent probe (#1113)

Ongoing Python research under #1083 / Draft #1115. No admitted parent/world renderer,
native/device result or certified error bound. Runtime images remain local.

## Source product and controls

The source compiler clips coarse regions directly against urban/open-space evidence;
it generates zero child cells. Continuous circle/diagonal setbacks estimate allowable
centre area. Their midpoint is a candidate, not a place fit or certified discrete
bound. Parents retain an inferred height CDF and unlit areas for six orientations.
The resolved generator shares vacancy, pitch, footprint and height constants;
all seven actual owner arrays remain byte-identical (247,731 masses / 5,962 patches).
Missing original heights remain unknown, not certified maxima or safe occluders.

The controlled renderer uses common direct GGX/diffuse plus unoccluded ambient.
The independent resolved DDA has inset lots, bounded heights and a shadow minimum
separate from the camera minimum. It is solely an offline control, never a compiler
prerequisite. No native material/GI, detailed true city, DEM or curvature equivalence.

The raw/midpoint roof-sheet/wall-extinction models lose visibility correlations;
the swept-phase mixture improves coverage while worsening light response. The
categorical control treats the four heights of one cell as mutually exclusive,
skips the known hit cell in shadow queries and conditions other cells on previous
primary-ray misses. It still traverses recipe cells at query time and is expensive.
Exhaustive enumeration of all 81 independent four-cell scenes matches RGB/coverage
under vertical/day/back sun and point light against independent box intersections.

## Local phase filtering

`urban_parent_phase.py` separates macro pixel samples from subcell phase. Each macro
ray keeps its direction but shifts its origin within one recipe pitch, so its ground
hit samples the canonical cell phase. The existing categorical query evaluates the
actual per-event lighting and conditional shadows. No lit colour is stored or baked;
only the final pixel integrates the samples. Parent assets remain camera-independent.

A uniform four-point phase grid aliases the footprint edges and overcounts area.
Instead split each axis into the two gaps and the footprint. For fill fraction f,
gap e=(1-f)/2 and K samples, use one midpoint per gap and K-2 Gauss-Legendre points
inside the footprint. Their weights are e, f/2 times the Legendre weights, and e;
the tensor product integrates vertical footprint coverage exactly. This does not
make oblique visibility or pixel filtering exact. The CLI excludes ground pixels
whose sampled smallest edge-Jacobian singular value is below one recipe pitch;
that guard is not a stationarity/quality guarantee. Macro X/Y counts are independent.

## Shared shadow transfer

`urban_parent_transfer.py` collects two unlit moments per surface category: visible
mass M and visible/unshadowed mass J. Categories retain each recipe height and the
five box normals; the ground has its own category. Four heights give 21 categories,
42 F64 values per anchor. Interpolate M and J, then use J/M as conditional visibility;
BRDF, light energy and ambient are evaluated at each actual primary event. No mean
normal or shaded-colour field. Zero mass means unsupported category, not empty world:
that event falls back to the exact conditional shadow query.

The table is transient and view/light-direction dependent. Changing the view or light
direction/position requires rebuilding it. Intensity/ambient changes do not alter its
moments. This is not general SH radiance transfer or an independently relightable
lighting asset. Table collection streams phase packets; each macro packet prepares
one bounded lookup buffer rather than allocating a full-image coefficient array.

At matching quadrature nodes, directional-light GGX integration agrees with direct
phase integration. Point-light position and coarse interpolation remain approximations.
The categorical traversal is shared through `trace_transport`; the three actual
paired direct crops remain bit-identical to the earlier eb707864e products.

Frozen 19156f3eb completed ten runs / 13 actual 1280×720 images, including three same-process
paired controls; all terminal 0 and actually inspected. Inputs remain the true 240-km
fdfc50d1c controls: day 32/other cases 16 jittered samples per axis, seed 1113. Times below
include transfer construction and consumption; paired controls use the same prepared
rays, profile and process, sequentially at normal priority under development load.

| Case | Direct phase RGB RMSE | Shared RGB RMSE | Shared total, s | Paired direct, s |
| --- | ---: | ---: | ---: | ---: |
| Day | .01267 | .01367 | 5.31 | 17.17 |
| Back | .01151 | .01158 | 5.65 | — |
| Night, fine table | .07831 | .07681 | 14.71 | 36.37 |
| Lateral movement | .01409 | .01427 | 2.56 | — |
| Low altitude, anisotropic table | .00584 | .00618 | 5.82 | 17.29 |
| Night, too-coarse table | .07831 | .12192 | 6.17 | — |
| Low altitude, too-coarse table | .00584 | .00732 | 21.70 | — |

These are linear RGB region errors against inferred resolved boxes, not world error
bounds. The three paired cases retain bit-identical primary coverage and reduce total
shadow cell visits, including table build, by 98.59%/75.23%/93.50% for day/night/low.
Their measured Python time ratios are 3.23×/2.47×/2.97×; no native/device speedup follows.
Day/back use 4×4-pixel table spacing, night 1×1 and low 4×1. Primary phase counts are 6
for day/back/night and 4 for movement/low; table phase count 6. Low macro sampling is 1×8,
other cases 2×2. Three matched phase 4-table runs are retained as sensitivity controls.

Day/night/low moment arrays contain 34,272/646,464/45,696 B. The retained per-packet
visibility buffer peaks at 43,008 B. These are component sizes, not full temporary,
copy-traffic or process/device memory peaks; raw RSS uses Linux KiB. A coarse low-view
table misses all mass anchors and triggers 1,046,182 exact fallback rays, while its
ground visibility still errs. A coarse night table misses the narrow brightness peak.
No generic spacing or safe-occlusion claim follows from the supported cases.

## Adaptive anchor placement

`urban_parent_adaptive.py` samples potential support at the ground and recipe height
planes. It refines changes in that mask and changes in projected view/light separation
where their footprints overlap. The angular rule resolves half-footprint changes
across a base step. This follows projected-footprint/overlap geometry, not an intensity
or colour fit, and is a heuristic rather than a conservative visibility/error bound.

Nonuniform axes reuse bilinear M/J filtering through coordinate interpolation.
Refined pixels replace coarse nodes instead of duplicating them. If pixel-centre
support probes miss an evidenced subpixel parent, projected centre/height anchors
are inserted. Neither that probe nor q=0 proves empty world. Unsupported categories
still use exact shadows; those fallbacks can erase a node-count saving.

Frozen b4cdc97a7 adds five same-process comparisons / ten actual 1280×720 images,
all terminal 0 and inspected. Primary coverage is bit-identical across each pair;
all five uniform-control RGB crops are bit-identical to their earlier 19156f3eb
products. Adaptive totals include planning, table construction and consumption;
uniform totals include construction and consumption. Both run sequentially at
normal priority under development load, adaptive first, on the same prepared rays.

| Case | Uniform nodes | Adaptive nodes | RGB RMSE uniform→adaptive | Total seconds uniform→adaptive |
| --- | ---: | ---: | --- | --- |
| Day | 102 | 276 | .01367→.01265 | 5.15→5.60 |
| Night | 1924 | 792 | .07681→.07547 | 14.74→10.31 |
| Low | 136 | 105 | .0061769→.0061774 | 5.59→6.20 |
| Back | 102 | 276 | .01158→.01113 | 4.93→5.25 |
| Movement | 102 | 325 | .01427→.01408 | 2.38→3.23 |

Night uses 58.8% fewer moment-table nodes and 646,464→266,112 B of moment arrays,
plus 496 B of axis arrays; measured total time is 1.43× faster with slightly lower
RGB error. The other four cases are slower. Uniform remains the default; adaptive
sampling is an explicit experimental option, not a universal winner or a device
result. Day/night/low planning examines 7,920/9,620/2,640 ray-plane pairs. Low view
still triggers 5,601 unsupported-category fallbacks; fewer nodes alone are not enough.

58 focused tests pass: projection equivalence, affine nonuniform interpolation,
subpixel support, energy independence, no duplicate dense nodes and prior transport/
geometry contracts. Ruff/format/function-size checks pass. Conditional day 16→32
difference remains .00561; reference uncertainty and regularised local structure
remain material. No native material/GI, geographic/transition/world/device/p99 admission.
The recorded sizes exclude full scratch, copies, driver and process/device peaks.

The source statistics have no admitted native Draw/Depth/Caster consumer; geographical
XY coverage is not a World render bound. Readiness clarification is in #1102/6338364477.
Next: a colour/transmission reference against explicit opaque ray stops and parent/
child composition, instead of another sampling-tuning round. Native placement,
DEM/curvature bounds and caster-interest domains remain #1102 responsibilities.

## Generation growth and reproduction

Direct compilation of the versioned z5/28/12 parent needs one source record,
256 regions, zero children and 6,264 array bytes instead of 1,531,626,496 hypothetical
32-m rectangular child candidates. One cold source compilation took 0.101 s with
39,878,656 B raw process RSS. Coarser evidence lacks finer owner exclusions: bounded
generation only, not equal-quality/global coverage or a device performance result.
At the owner, raw roof area overcounts by 14.7%; continuous setback variants give
+2.9% / -1.7%. They are not certified lattice/hash/GEOS bounds.

With the existing `.venv`, fresh system-temp outputs and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-parent-control --samples 16 --jitter-seed 1113 \
  --lighting night
python -B -W error test/experiments/buildings/compare_urban_phase.py \
  --reference /tmp/outshine-urban-parent-control \
  --output /tmp/outshine-urban-parent-filtered --macro 2 --phase 6 \
  --shadow-stride 4 --adaptive-shadow --paired-uniform 1
```

The parent is saved before the offline control is built. `--parent` consumes a saved
parent; `--resolved` reuses an existing field
solely for the control. `--motion`, `--altitude`, `--lighting` vary view/light without
rebuilding the asset. `--macro-x 1 --macro-y 8` requests anisotropic pixel sampling.
The phase comparator reads the saved control images/parent, not resolved child data.

## Primary sources read and visually inspected

- [Feibush/Levoy/Cook 1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section 2.4 / Figure 2: filter polygon edges together with texture footprint.
- [Bruneton/Neyret 2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section 3 / Figure 2 / equations 1–6: correlated visible/lit proportions and their geometric parameters. Our phase quadrature does not adopt the Poisson ellipsoid model or precomputed forest implementation.
- [Han et al. 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures 1–2 / section 3: normal distributions for BRDF filtering; explicitly excludes occlusion, geometry simplification and cast shadows.
- [Horizon cloudscapes 2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF page 37 / slide 35: separate coverage and height density; no cloud implementation or cloud/building equivalence adopted.
- [Sloan/Kautz/Snyder 2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction / Figure 2, PDF page 2: separate transport coefficients from current lighting/BRDF evaluation. This probe uses neither the paper's SH representation nor its GI/low-frequency-light guarantees.

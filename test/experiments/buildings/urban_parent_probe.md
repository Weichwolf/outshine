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

Frozen eb707864e completed nine new 1280×720 runs, terminal 0; all images inspected
in comparison sheets. Controls are frozen fdfc50d1c, true 240-km target distance,
day 32 and other cases 16 jittered samples per axis, seed 1113. The table reports
linear RGB region RMSE against resolved boxes, not a physical world error bound.

| Case | Macro X×Y / phase K | Fraction of control rays | Conditional RMSE | Filtered RMSE |
| --- | --- | ---: | ---: | ---: |
| Day | 2×2 / 4 | 1/16 | 0.01272 | 0.01223 |
| Back | 2×2 / 6 | 9/16 | 0.00980 | 0.01151 |
| Night | 2×2 / 6 | 9/16 | 0.07198 | 0.07831 |
| Lateral movement | 2×2 / 4 | 1/4 | 0.01387 | 0.01409 |
| Low altitude, inadequate macro sampling | 2×2 / 4 | 1/4 | 0.00403 | 0.01402 |
| Low altitude, anisotropic macro sampling | 1×8 / 4 | 1/2 | 0.00403 | 0.00584 |

Day primary visits fall from 7.15M to 0.456M, shadow visits 34.24M to 1.528M and history
comparisons 160.69M to 7.216M. Query time 6.89 s versus historical 142.81 s is a shared-load
Python observation, not a current paired/device speedup. Day phase-packet arrays
retain 196,608 B and history 393,216 B; these exclude other temporaries/process peaks
and copied bytes. The raw RSS field uses Linux KiB, not an application budget.

**No universal cost/quality winner.** Conditional day 16→32 sampling difference is
0.00561; reference uncertainty remains material. Boundaries/local structure are
still too regular. Coarse phase sampling damages back/night; coarse macro sampling
damages the thin low-view silhouette. More phase points alone do not fix that case.
Next work must preserve pixel coverage and joint view/light response while sharing
unlit transfer work instead of repeating expensive quadrature for every pixel.
Transitions, other geographic inputs and complete native/world/device/p99 admission
remain open; Python observations admit none of those requirements.

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
  --output /tmp/outshine-urban-parent-control --samples 32 --jitter-seed 1113
python -B -W error test/experiments/buildings/compare_urban_phase.py \
  --reference /tmp/outshine-urban-parent-control \
  --output /tmp/outshine-urban-parent-filtered --macro 2 --phase 4
```

The parent is saved before the offline control is built. `--parent` consumes a saved parent; `--resolved` reuses an existing field
solely for the control. `--motion`, `--altitude`, `--lighting` vary view/light without
rebuilding the asset. `--macro-x 1 --macro-y 8` requests anisotropic pixel sampling.
The phase comparator reads the saved control images/parent, not resolved child data.
51 focused tests pass; Ruff/format and 120-line function checks pass. No native gates.

## Primary sources read and visually inspected

- [Feibush/Levoy/Cook 1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section 2.4 / Figure 2: filter polygon edges together with texture footprint.
- [Bruneton/Neyret 2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section 3 / Figure 2 / equations 1–6: correlated visible/lit proportions and their geometric parameters. Our phase quadrature does not adopt the Poisson ellipsoid model or precomputed forest implementation.
- [Han et al. 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures 1–2 / section 3: normal distributions for BRDF filtering; explicitly excludes occlusion, geometry simplification and cast shadows.
- [Horizon cloudscapes 2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF page 37 / slide 35: separate coverage and height density; no cloud implementation or cloud/building equivalence adopted.

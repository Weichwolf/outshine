# Angular asphalt sources: numerical bake accepted, appearance rejected

Research for open [#781](https://github.com/Weichwolf/outshine/issues/781),
following the Gaussian component in [asphalt.md](asphalt.md), merged by #787.
This Python comparison changes no native material, shader, surface role or asset.
The viewed plates still look patterned; **neither polygon dots nor the Voronoi
control is accepted as the new asphalt appearance**. The useful result is a
bounded procedural bake with independently checked coverage, means and mips.

## Sources and spatial model

[Worley, SIGGRAPH 1996](https://cedric.cnam.fr/~cubaud/PROCEDURAL/worley.pdf)
was consulted first, including an optical reading of page 2. Its discussion of
regularity in jittered lattices matters here. This experiment does not implement
Worley's F1 distance field or claim a Poisson distribution. The
[FHWA-HRT-14-065 pavement-texture report, February 2015](https://www.fhwa.dot.gov/publications/research/safety/14065/002.cfm),
figures [3](https://www.fhwa.dot.gov/publications/research/safety/14065/images/image008.png)
and [4](https://www.fhwa.dot.gov/publications/research/safety/14065/images/image009.png),
were downloaded and actually opened as close-up comparisons. Their varied fine
and coarse grains and irregular exposed surfaces differ from both candidates.
These photographs are comparison inputs only; the generator uses no image data.

The authored period is 16×16 cells at 0.024 m/cell: a 0.384×0.384 m tile.
The compared densities are one and four sources/cell. Existing grain hashes,
centre offsets 0.2..0.8 and weights 0.75..1.25 are reused; extra sites use fixed
integer salts. Each core is the convex hull of eight rotated radial vertices
at radius 0.14..0.22 cells. Nominal 6.72..10.56 mm diameter bounds are authored,
not a measured aggregate gradation. Density scales weight by 1/sqrt(density).

For primary cores P_i and weights w_i, the selected source is
`F(x) = 0.75 sum_i w_i I(P_i)(x) - C`, with
`C = 0.75 sum_i w_i area(P_i) / period²` over primary cells.
Translated neighbours complete the periodic boundary. The uniform binder gives
an analytic zero mean. An earlier `I(P)-0.25 I(2P)` alternative produced artificial
dark rings when viewed and remains only the rejected `binder="halo"` control.
Colour and roughness use the same F and unchanged amplitudes 0.035 / 0.02.
Both actual recipes are checked: route RGB [0.11,0.11,0.10], roughness 0.92;
world catalogue RGB [0.1256,0.1193,0.1104], roughness 0.65.

The periodic Gaussian control reuses existing sites and erf integration with a
bounded tail. It is a new periodic source, not byte parity with the original
nonperiodic world field. Existing component functions are unchanged.

## Independent coverage and complete mip checks

GEOS polygon/pixel-box intersections provide the reference, independently of
Pillow's local 1-bit supersampled masks. NumPy half-plane point membership is an
aliasing control. Triangles, a clipped diamond, full/disjoint coverage and
translation have separate analytic checks. `ss32-dc` uses 32×32 local samples,
then removes raster bias by projection to the known analytic zero mean. It
changes neither the source polygons nor the unchanged parameter-error limits.

All 512×512 pixels, both densities and both actual recipes were checked. Errors
below are maxima over recipes: sRGB colour in 8-bit LSB, roughness in 1/255 steps.
The limits are one unit each; no threshold was relaxed.

| Bake | Density 1 colour / roughness | Density 4 colour / roughness | Pass |
|---|---:|---:|---|
| Point | 7.614828 / 2.841541 | 6.979074 / 2.736812 | No |
| 8×8 | 2.794853 / 0.998956 | 2.368206 / 0.892740 | No |
| 16×16 | 1.579986 / 0.580677 | 1.144512 / 0.423554 | No |
| 32×32 | 0.795832 / 0.294263 | 0.657231 / 0.240096 | Yes |
| 32×32, zero-DC projection | 0.792346 / 0.292967 | 0.649944 / 0.237423 | Yes |

Uncorrected raster means are 0.000253962 / 0.000524192; projected means are
2.01e-18 / 2.75e-17. All ten scalar mip levels, 512→1, are separately stored as
float32 and checked against newly integrated GEOS references at their own pixel
sizes. Float64 recursive exact box means differ from direct integrals by at most
1.12411e-14 / 7.71606e-15. Every stored mip passes the parameter limits and the
far mean remains within numerical bounds. Each scalar chain is 1,398,100 bytes;
this is a research representation, not a selected production GPU format.
These results cover the declared axis-aligned tile pixels and box mips, not
arbitrary perspective/anisotropic footprints or an integrated nonlinear BRDF.
The helper's other permitted dimensions are not given these full error claims.

## Work, allocation and memory

The four-source 512² bake generates 1,296 source records including padding,
processes 1,024 visible polygons and 160,277 polygon/bounding-pixel pairs.
Peak local coverage rectangle: 210 pixels. Supersampling represents 164,123,648
boolean samples across rectangles; a peak expanded mask is 215,040 bytes, plus
the 26,880-byte bit mask. These are layout/work estimates, not measured traffic.

Linux x86_64, Python 3.13, main `.venv`: three uninstrumented repetitions/method,
normal priority; one separate Memray run with Python allocators at nice 10.
Concurrent native-agent work was possible; these are local bake trends.

| Full source generation + scalar bake | GEOS area | 32×32 + DC |
|---|---:|---:|
| Median ordinary seconds | 3.186977 | 0.851675 |
| Ordinary process peak RSS, KiB | 76,784 | 77,056 |
| Traced allocation count | 22,200,242 | 1,402,105 |
| Traced allocated bytes | 2,449,089,476 | 841,367,301 |
| Traced peak live bytes | 4,304,314 | 4,401,775 |

Output float64 field: 2,097,152 bytes. The explicit hash serialization copies
another 2,097,152 bytes inside allocation tracing, outside ordinary timing.
Native GEOS/Pillow total copying is unmeasured; allocated bytes are not copied
bytes. Scope excludes mips, NPZ IO, uploads, rendering and cache-hit time.
The complete correctness campaign peaks at 130,652 KiB process RSS.
The final drivers report bytes and distinguish Linux KiB from Darwin bytes;
the frozen Linux producer's `*_kib` values above remain valid.

## Viewed frames and rejected source alternative

Both whole plates were actually opened: 1280×720, 32 Eevee samples, bpy 5.2.2
LTS, llvmpipe LLVM 19.1.7. Each frame contains Gaussian / one-site polygon /
four-site polygon plates, each 0.4×0.65 m, same metric repeat, camera and research
daylight. Route/catalogue render times are 31.396 / 43.092 s; combined-process
peak RSS is 1,859,976 KiB. The recipes produce distinct images, as expected.
Each frame decodes 3,145,728 scalar bytes and explicitly writes 12,582,912 RGBA32F
bytes. Loading cached inputs forbids source generation and rasterization.
Only base levels are uploaded: Blender supplies sampling, not the exported
scalar mip chain. Principled/AgX is not native GLSL BRDF parity or a device gate.

The dot-like pattern remains visible. A further viewed control uses 4,096 fixed
uniform random seeds (seed 781), GEOS Voronoi cells and seed-centred shrink
factors 0.85 / 0.95. It resembles a crack/paving network. It is point sampled;
filter convergence, periodic partition and production costs are unverified.
`asphalt_voronoi_control.py` preserves this rejection for reproduction. Neither
source should be adopted merely because its code or numerical bake passes.
The next appearance hypothesis needs varied grain scales and plausible packing
within #781's dry colour/roughness scope; adding relief is not implicitly agreed.

Native surface roles, metric frame/phase, recipe/cache/atlas consumers, selected
format/filter and actual lit world/Places plus A18/Iris budgets remain open.
No appearance gain or product acceptance is claimed; #781 stays open.

## Reproduction and producer evidence

Full numerical/render/cost producer: `7d6da2cdc339597898e4bb368d2ec64553c76ed7`,
frozen at `/tmp/outshine-asphalt-angular-7d6da2cdc`. Later changes add the rejected
control and reusable cost CLI, shorten stdout and correct RSS units; numerical
functions and renderer geometry/material/light code remain identical.
Private receipts and actual inputs live in
`/tmp/outshine-research-asphalt-angular-7eb9b484d/`: `whole-study/report.json`
and three NPZ files, `whole-render/` with both PNGs and input hashes, `cost-*`
JSON/Memray traces and the original `bake_cost.py`, `final-unit-controls.log`,
`voronoi-control-final/`, initial rejected halo inputs and the FHWA/PDF references.
The original private cost loop and final reusable loop call the same frozen bake.
The reusable CLI was replayed with both methods: its field hashes match the
originals exactly. This later run measured 3.747774 / 0.851849 s respectively;
it is a separate load sample, not a replacement for the table's producer run.
Reports include source/input SHA256 values; no private tree is a product input.

Required packages in `.venv`: NumPy, SciPy, Shapely/GEOS, Pillow, Matplotlib;
bpy/outshine-ref for the plates and optional Memray for allocation tracing.
Use a fresh explicit temp output for every command, e.g. with an unused shell
variable `study_dir=$(mktemp -d /tmp/outshine-asphalt-replay.XXXXXX)`:

```sh
.venv/bin/python test/experiments/render/test_asphalt_angular.py
nice -n 10 .venv/bin/python test/experiments/render/compare_asphalt_angular.py \
  --output "$study_dir/study"
LP_NUM_THREADS=2 OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 LIBGL_ALWAYS_SOFTWARE=1 \
  nice -n 10 xvfb-run -a .venv/bin/python \
  test/experiments/render/render_asphalt_angular.py \
  --inputs "$study_dir/study" --output "$study_dir/plates"
nice -n 10 .venv/bin/python test/experiments/render/asphalt_voronoi_control.py \
  --output "$study_dir/voronoi"
.venv/bin/python test/experiments/render/measure_asphalt_bake.py \
  --method ss32-dc --output "$study_dir/cost"
nice -n 10 .venv/bin/python test/experiments/render/measure_asphalt_bake.py \
  --method ss32-dc --track --output "$study_dir/cost-traced"
.venv/bin/python -m memray stats --json -o "$study_dir/allocations.json" \
  "$study_dir/cost-traced/allocations.bin"
```

Repeat cost commands with `--method area` and separate output paths. Detached
trees use the absolute main `.venv/bin/python`. Unit controls: 9/9 pass. The
full campaign and both frame renders terminate with status 0. Final checks
add Python format/lint, bounded function sizes, CLI/path controls and doc lint;
native checks belong to later integration.

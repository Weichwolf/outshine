# Lit inspection of the existing mature spruce products

Owner: [#508](https://github.com/Weichwolf/outshine/issues/508), under
[#182](https://github.com/Weichwolf/outshine/issues/182). This is a diagnosis of
existing experimental forms, not a new plant algorithm or botanical admission.
The renderer is independent of the unmerged parent-profile implementation in
Draft #202: it consumes its complete saved product through existing master
helpers. No native, library, material, recipe or cache API changes.

## Matched comparison and decision

Both complete 2560×1440 images were opened. The old control looks like a thin
foliated column, with a blunt upper crown. The parent-profile candidate visibly
tapers towards a single leader; its upper branch groups become smaller and its
lower groups are distinct. Both have a ground contact and a separately selected
cast shadow. The candidate is an improvement in silhouette, **not an acceptable
finished mature spruce**: regularly repeated tiers, very narrow crown, soft
foliage and a translucent appearance remain obvious. Its shadow shows the same
regular layering. More copies of this form will not solve those faults.

The local European Atlas of Forest Tree Species, *Picea abies*, pp.114–115
(both PDF pages opened), shows dense identifiable branch masses, a pointed
leader, rough bark and individual twig/needle structure. Its isolated tree is
also much broader than this authored forest-form recipe. These are visual
references, not measured dimensions or a claim that every spruce must match
that photograph. Weber/Penn, SIGGRAPH 1995, §4.3/Fig.1, PDF p.3, was also opened:
child length and occupation depend on their parent and attachment position.
The comparison supports the existing parent-length correction, without proving
botanical correctness, stand allometry or any SpeedTree equivalence.

The next useful experiment is a closer representation from the existing finite
twig/window products, checked at the same physical scale against the whole-tree
far product. Do this before another complete eight-form bake. Inspect branch
spacing and crown width separately from filtering/material response; increasing
atlas resolution alone cannot repair a poor growth recipe. Neither mean normals
nor the statistical-window coverage errors are approved by this comparison.

## Inputs and controls

The frozen renderer is commit `5b50c5d4b`. Merge `fe6898d5d` brings current master
in without changing its vegetation helpers or library modules. The control was
rendered before reboot, the candidate after reboot; timing is not a controlled
speed comparison. Both reports identify OpenGL/llvmpipe LLVM19.1.7, software
rendering, not an RTX3060/A18/device-performance result.

| Property | Columnar control | Parent-profile candidate |
|---|---:|---:|
| Complete product identity prefix | `7b0ab06dee97` | `e0c6abe45a93` |
| Actual axes / represented needles | 50,401 / 11,407,570 | 52,811 / 11,628,260 |
| Finite windows | 61,760 | 70,990 |
| Expected nominal foliage area, m² | 200.001725 | 203.870941 |
| Nominal target area, m² | 204.140691 | 204.140691 |
| Physical upper bound, m | 33.230971 | 33.243369 |
| Whole-view charts | 64 × 256×256 | 64 × 256×256 |
| Files actually read, bytes | 7,934,667 | 7,349,015 |
| Hash/header time, s | 0.0375 | 0.0286 |
| Hashes + decode/binding/setup before environment, s | 0.2437 | 0.1918 |
| Render wall time, s | 160.5168 | 138.1838 |
| Whole measured function wall time, s | 160.7659 | 138.3811 |
| Process peak RSS, MiB | 2292.383 | 2293.367 |

The area convention is half the cached closed-needle prototype area with the
declared variation expectation; it is not an observed stand LAI. Candidate area
is about 1.93% above the control, not exactly equal. Both retain the same
physical needle recipe; the candidate's additional source axes/windows are real
differences. Old wood uses the prior procedural bark colour field, while the
candidate was baked with constant wood colour. Identical Blender material,
camera and lights therefore do not make every source channel identical; do not
attribute bark colour or every brightness difference to morphology.

Camera `(18,-70,24)`, target `(0,0,16.6)`, vertical FOV30°, flat100m ground,
32Eevee samples, fixed sun and fill. The source roots stay at `(0,0,0)` with an
identity frame; no tree scaling or reference geometry. Camera/sun/fill choose
indices23/12/7 independently from the same complete product:128triangles each,
384total tree draw triangles, plus ground. All8192 cached view triangles and
all three full2112×2112 maps remain available. A60-direction selection probe
chooses16distinct views for each source without rebuilding it.

The selected camera chart spans roughly34.9m in its long direction and9.1–9.3m
in its short direction: only7.3texels/m longitudinally. Rendering this far
product as an approximately1200-pixel-tall tree enlarges it several times.
That limits the portrait's detail independently of the source's needle count.
It is evidence for a missing appropriate close representation, not proof that
texture allocation is the sole cause of softness or transparency.

## Reproduction and checks

Use the project's `.venv` with NumPy, SciPy, Pillow and bpy5.2.2. Supply existing
complete `whole-plant-views-1` product directories; this command does not build
them. Each must include its recipe, all view meshes and three atlas maps.
Source cache preparation/provenance remains in Draft #202. These private inputs
are not substitutes for versioned source recipes or a fresh generator proof.

```sh
export PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 LP_NUM_THREADS=1
export PYTHONPATH=lib/src:test/experiments/vegetation
proof="${TMPDIR:-/tmp}/outshine-vegetation-mature-portrait-$(git rev-parse --short HEAD)"
.venv/bin/python test/experiments/vegetation/mature_tree_portrait.py --inputs "$CONTROL_PRODUCT" --output "$proof/control"
.venv/bin/python test/experiments/vegetation/mature_tree_portrait.py --inputs "$PARENT_PRODUCT" --output "$proof/candidate"
```

The renderer validates the version/recipe identity, hashes all consumed files
and forbids `NeedledPlant.foliage`, `NeedlePattern.detail`, `Needle.mesh` and
`PlantPlan.axis` during cache binding. Each invocation terminated0 and saved a
PNG/report. Independent inspection checked both recipes against their identity,
64×81 vertices/64×128 faces/UV dimensions, finite arrays, unit view directions,
map dimensions, identical colour/normal alpha, leaf weight≤alpha, 2560×1440 PNGs,
matching scene settings, camera/light indices and the complete orbit selection.
No new generic test or native gate was needed for this diagnostic renderer.

Raw PNGs, source-file hashes, reports, `verification.json`, `chart-metrics.json`
and logs are local under
`/var/tmp/outshine-vegetation-mature-portrait-proof-494a36177`.
Image SHA256 prefixes: control `7d1ffd06cc01`, candidate `5b318d182186`.
Both entire images were opened, along with the cited PDF pages. The images stay
private; master alone owns publication. This delivery closes only the matched
inspection, leaving #182/#131/#168/#1 and the full research goal open.

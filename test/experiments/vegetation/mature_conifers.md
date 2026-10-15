# Mature conifer forms over the conserved boreal stand

Owners: [#131](https://github.com/Weichwolf/outshine/issues/131) morphology,
[#172](https://github.com/Weichwolf/outshine/issues/172) actual branch radii,
[#179](https://github.com/Weichwolf/outshine/issues/179) requested needle area,
[#168](https://github.com/Weichwolf/outshine/issues/168) coarse products.

The existing boreal showcase contains 350 closed crown proxies over 160 m.
Its spruce/pine profiles request 35/30 m trees. The four new cached conifers from
PR #171 are young, about 3.5 m high; scaling them affinely would also enlarge
their needles. This experiment instead creates eight distinct mature wood plans
for the original two species and four size variants. Existing site IDs, counts,
positions, variants, ground, camera, sun, resolution and samples stay the control.

`mature_conifer_recipes.py` interprets the existing height/spread/DBH/bole profiles.
Spruce has whorled primary branches, lower droop and a persistent leader; pine
has a higher, rounded crown and a long clear stem. Structural lengths/radii and
deterministic seeds vary with the profile. Physical needle recipes, placement
spacing remain unchanged. This is a static procedural form interpretation,
not a calibrated age, growth or population model.

Mature branches expose a real previous limitation: a fixed radius per order does
not fit thin upper stem attachments. `AxisLevel.radius_parent_ratio` explicitly
opts into `branching-plant-4`. Actual child base radius is the smaller of its
nominal base and this fraction of the actual parent radius at attachment. The
nominal tip/base ratio remains. `PlantPlan.radii(path)` evaluates only the needed
ancestry; immutable `radius_limits` bounds all sibling/parent choices without
placing axes. Tables, physical needle offsets, mesh, collar and bark consumers
use those actual radii. Existing v2/v3 fields, IDs, recipes, default buffers and
geometric rejections remain; old versions cannot smuggle the new field.
Adaptive v4 levels explicitly permit up to 256 siblings, with
`AxisState.path_capacity=256`; old level/path defaults retain their 64 limit.
The spruce recipe uses 240 primary branches in 48 five-branch whorls. This avoids
the first rejected recipe's 21 isolated whorls over a 35 m trunk.

The parent-attachment maximum follows the principle in
[Weber and Penn, SIGGRAPH 1995, §4.4, PDF page 4](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf),
opened optically. The nominal-radius-plus-explicit-cap implementation is our
limited procedure, not the paper's complete length-power/pipe/growth model.
Botanical form references, also opened optically:
[Picea abies, JRC 2016](../../../doc/references/vegetation/botany/2016-european-atlas-picea-abies.pdf)
and [Pinus sylvestris, JRC 2016](../../../doc/references/vegetation/botany/2016-european-atlas-pinus-sylvestris.pdf).
The photographs guide morphology/colour; they are not generated texture inputs.

The first mature 350-site image was opened and rejected: sparse, pole-like
crowns did not improve the original closed proxies. Its complete cache and
attachment checks passed, illustrating why those checks do not replace the image.
The recipes had ignored the profiles' existing `lai` input. At half the closed
physical needle area they supplied nominal indices of about 3.15/1.10 rather
than the requested 8.0/3.5. Correcting the radius invariant alone was insufficient.

`NeedledPlant.area_summary(unit_areas_m2)` counts all deterministic variant IDs
and expected area before axis/organ work. Uniform scalar variation contributes
the exact expectation `1 + variation**2 / 3`. The experiment reads half the
triangle area of each cached closed needle prototype, without meshing new organs.
Its target is `profile.lai * pi * (profile.spread_m * variant_scale / 2)**2`.
This convention follows the needle-area definition discussed by
[Chen et al. 1997, PDF page 2](../../../doc/references/vegetation/botany/1997-leaf-area-index-of-boreal-forests.pdf),
also opened optically. The declared circular crown area is a recipe reference,
not a measured crown footprint or stand LAI; clumping/effective LAI differ.

`mature_density.py` enumerates terminal branch degrees 1..64 within the fixed
recipe family and selects minimum area error, then axis count, then degree.
It requires relative expected-area error at most 5%. Needles keep their physical
size/spacing, and alpha remains unchanged. No global plant optimum is claimed.

`mature_conifer_experiment.py` first counts every planned axis/window, with at
most 65,536 axes and 100,000 windows per new complete form. The larger explicit
offline axis profile is needed for the previously ignored area requirement;
the old low-count recipe fails that requirement. Existing 20,000-axis/window
experiment defaults and all native/frame gates remain unchanged.
It then invokes the shared whole-product
baker with all individual-needle placement/meshing disabled. New recipes must
match the catalog's physical needle recipes and pattern families. Demand is
checked before catalog arrays decode. All original axes/needles remain represented
by stable source ranges; cached recipe data supplies later requested near detail.
The finite mature catalog uses nominal 0.32 m windows, at most 256 needle groups
and 32×32 templates with 8×8 subpixel samples. Catalog generation and complete
products run in separate processes. Defaults remain 0.08 m, 64 groups and 4×4.
Larger windows approximate curvature and tails; matched physical shoot coverage
must be reported separately, not inferred from correct total needle area.

The optional far wood-colour field averages the existing procedural bark recipe
along stem height and radius. It gives pine a warmer upper trunk and preserves
young-twig colour. It is unlit and approximate; it does not replace the physical
bark shader, normal distribution or optical/material approval. The default wood
raster remains byte-identical on the previous independent compositor control.

```sh
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/mature_conifer_experiment.py --plans-only
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/mature_conifer_experiment.py --phase catalog
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/mature_conifer_experiment.py --phase products --size 256
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/mature_forest_checks.py --scenario build/experiments/vegetation/landscapes/boreal.json --products build/experiments/vegetation/mature-conifers/mature-products.json --output build/experiments/vegetation/mature-conifers/stand-checks.json
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/vegetation/mature_forest_preview.py -- --scenario build/experiments/vegetation/landscapes/boreal.json --products build/experiments/vegetation/mature-conifers/mature-products.json --output build/experiments/vegetation/mature-conifers/forest/render.png
```

Use the previously generated `conifer-crowns` organ assets as inputs. Only the
finite physical catalog is loaded during whole production, never a near tree.
`mature_crown_probe.py --output <probe> --catalogs <mature-output>/catalogs`
creates one unlit physical-aspect diagnostic per species before all 64-view bakes.
Whole stand images, frame costs and botanical/art quality remain separate from
the scalar radius invariant. High local coverage errors of statistical windows,
mean normals, shadows, wind and moving detail transitions remain open in #168.
The full worldwide goal and the other Swiss/European species remain open.

`mature_forest_plan.py` binds all original source sites to complete catalog forms.
It rejects gaps, duplicate ownership and recipe mismatches; no site is discarded.
Models are placed rigidly, because their recipes already own the size variation.
Camera and sun views are independent. Cache checks keep texture/mesh allocations,
disable all organ/window/axis generation and measure one 60-step camera orbit.
Only requested terminal subsets are expanded separately to check physical needle
offsets and exact cached ID ranges. CPU decode/query and resident arrays are not
native frame, upload, GPU allocation or A18 evidence. The 2560×1440 whole-stand
comparison must be optically accepted before replacing the old showcase image.

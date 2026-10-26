# Terminal broadleaf shoots at matched physical leaf area (#295)

The four flat research tiles exposed branch-dominated crowns. Increasing the
number of leaves along those few axes creates overlapping leaf rows. A short
additional shoot order distributes the same physical leaf budget through space.
This own experiment reuses the published `PlantPlan`, `ShootPlan` and `Blade`
implementations; it introduces no native API or accepted botanical species.

## Method and controls

`broadleaf_crowns.py` compares the existing 97-axis oak/beech forms against
1,105 axes, including 1,008 short terminal shoots. The entire original woody
skeleton keeps identical IDs, origins, frames and dimensions. Only the terminal
order carries leaves in the alternative. It uses a 0.35m maximum shoot length,
12 attachments per last-order branch, irregular azimuth/attachment/bend and the
existing distichous beech / spiral oak arrangements. These are authored study
values, not measured allometry or a light-competition simulation.

A bounded monotone bisection of leaf internode lengths matches the physical
leaf area using summary counts before building axes or organ poses. Discrete
leaf-count steps remain explicit: the actual A/B area differs by at most0.22%.
Targets18m² and55m² are experimental budgets. Neither is a botanical LAI claim.

Both alternatives use identical eight-segment physical blade meshes and wood
meshing. Two-sided projected leaf union is measured at pixel centres in six
fixed orthographic directions, independently at256×256 and512×512. Bounds are
shared per pair. The convex hull is only a measurement proxy. It is neither a
filled canopy nor an acceptance mask; no envelope geometry enters the render.

## Completed comparison

Frozen CPU experiment `2874e738510636f1bb094d96d407b43c95309469`, terminal0:
69.859s wall,69.827CPU-s,582.87MiB peak RSS; one thread for numerical libraries.
System load started1.46/2.74/2.57 and ended1.93/2.61/2.53. These are local offline
measurements, not an isolated machine or target-device benchmark.

Numbers are density-only / terminal shoots. Coverage ratios are terminal /
density at512×512 across all six directions.

| Form / target m² | Leaves | Actual leaf area m² | Mesh build s | Triangles | Projected union ratio |
|---|---:|---:|---:|---:|---:|
| oak / 18 | 5,532 / 5,544 | 17.741 / 17.779 | 0.255 / 1.555 | 193,212 / 242,004 | 1.268–1.544 |
| oak / 55 | 17,128 / 17,136 | 54.929 / 54.955 | 0.444 / 1.754 | 587,476 / 636,132 | 1.850–2.006 |
| beech / 18 | 7,560 / 7,560 | 17.817 / 17.817 | 0.283 / 1.980 | 262,164 / 310,548 | 1.365–1.707 |
| beech / 55 | 23,348 / 23,352 | 55.024 / 55.034 | 0.516 / 1.895 | 798,956 / 847,476 | 1.872–2.051 |

The maximum256→512 union-area difference is0.65%. These six views are samples,
not a global angular bound. Actual triangulated leaf area is also reported:
about4.9% above the analytical oak area and1.5% below the beech area at this
subdivision. Both methods share the same blade discretization. This discrepancy
is not hidden by calling analytical area the actual mesh area.

Extra terminal wood costs about48,500 triangles. At55m² that is8.3% more total
triangles for oak and6.1% for beech. Placement/wood construction is slower:
1.75–1.90s versus0.44–0.52s. The result improves spatial coverage; it is not a
claim of faster full-model generation.

A separate complete prepared-array cache decode returns five arrays per model,
without repeating plan construction. For55m² oak:26,409,002 compressed bytes,
104,449,668 returned bytes,0.304s decode; beech:35,478,279 compressed bytes,
139,271,700 returned bytes,0.486s decode. OS cache state is uncontrolled. These
are float64 offline reference products, not a compact runtime asset format.
Array payload/work counts are explicit; total allocator calls and internal
compression-buffer copy traffic were not instrumented.

## Actual optical check and use

Blender pair renderer `e0e815d3211c14f6eeb782d0cc36a90e5bc3bac5`: both whole
1920×1080/four-sample images opened. Left is density-only, right terminal shoots;
orthographic direction/width, light, materials and55m² leaf budget are shared.
Blender5.2.2 EEVEE uses OpenGL llvmpipe software. Both processes exit0: oak40.450s
wall/46.539CPU-s/3426.32MiB RSS; beech44.072s/51.677CPU-s/3452.20MiB. Expected EGL
context fallback diagnostics precede successful images, not a GPU claim.

The right crowns visibly form foliage groups; the control creates stacked leaf
rows. Remaining repeated branch tiers, sparse upper crown, generic trunk bark
and lack of botanical growth asymmetry remain visible. This accepts the local
architectural improvement, not SpeedTree quality or a completed species recipe.

`biome_models.woody` now uses terminal55m² oak/beech products, with eight blade
segments. Heather, conifer, grass and all tile populations remain unchanged.
`biome_recipes.py` separates the common original authored dimensions from mesh
assembly and experimental architecture. Original oak/beech/heather geometry
was checked byte-for-byte against previous cached products before applying the
new broadleaf recipe. Only previously collapsed leaf UVs change: blade(t,s)
parameters now survive as((s+1)/2,t), with a focused layout check.

The whole forest comparison changes both architecture and the authored leaf
budget (old oak18.35m²/beech13.49m² → approximately55m²) and blade subdivision
(three → eight). Only the matched-area plant pairs isolate the architecture
change. Whole-tile visual improvement must not be attributed to hierarchy alone.

Both entire before/after forest tiles were opened at1920×1080/four samples.
Camera, root population, non-broadleaf products, light and materials are unchanged.
The new forests show substantially more connected foliage and stronger canopy
shadows. Still visible: repetitive branch tiers, sparse ground vegetation,
generic species silhouettes and small provisional conifers.

Frozen tile source `fad9b4a61d2cde67a29367202b648cba1e0d7633`, Blender5.2.2/EEVEE
llvmpipe, both processes exit0:

| Tile | Cached composition s | Render s | Process wall s | Process CPU-s | Peak RSS MiB | Instanced triangles |
|---|---:|---:|---:|---:|---:|---:|
| Laubwald | 9.549 | 114.542 | 125.854 | 154.341 | 3288.10 | 22,476,000 |
| Mischwald | 12.608 | 103.824 | 118.212 | 142.494 | 3413.97 | 19,244,100 |

Laubwald system load1.75/1.77/2.09→2.53/2.05/2.15; Mischwald2.10/1.98/2.12→
3.20/2.32/2.22, with Buildings beginning a parallel render late in the mixed run.
The full-mesh tiles cost substantially more than the initial snapshots. No forest
frame/memory budget, native device performance or selected LOD is accepted.
Current images/receipts remain in
`/tmp/outshine-vegetation-terminal-tiles-fad9b4a61/`, model inputs in
`/tmp/outshine-vegetation-terminal-models-82bb53095/`. New broadleaf arrays were
checked exactly against the measured matched-area products; unchanged inputs
are linked to the original products. Full replay and partial requests retain
stable plan/organ IDs.
All research images stay local; no `doc/` images or replacement gallery are
published, following the current shared instruction in #47.

## Verification

22 focused tests pass: `broadleaf_crown_checks`, `biome_tile_checks`, plus the
existing `test_blades`, `test_plant_plans`, `test_graminoid` library controls.
They cover pre-placement budgets, unchanged original axes, reversed/partial
requests and exact full-table replay, rigid frames, organ anchors and leaf UVs.
The two actual prepared full meshes additionally have finite values, valid
indices/corner layouts, no zero-area triangles and no opposed corner normals;
maximum normal-length error6.67e-16. Minimum double triangle area1.247e-8m².
Ruff checks/format and the separate AGENTS-only `make lint-docs` pass. Native
engine/Place/CPU-GPU gates were not run for this Python-only recipe experiment.

## Local sources

First reference: Weber/Penn, *Creation and Rendering of Realistic Trees*,
SIGGRAPH1995 (`doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf`).
The branching hierarchy / leaf sections motivated the extra order. The local
PDF was inspected optically, including page5. Palubicki et al., *Self-organizing
Tree Models for Image Synthesis*, SIGGRAPH2009, is the existing local reference
for shoot/metamer architecture; this experiment does not implement its growth
or resource competition model.

Ghidra read-only analysis of the installed SpeedTree10.2 ELF was actually run:
`CGenerator::GetParentTemplateNode` at0x209a270 resolves an incoming generator
link to its parent's template; `CGenerator::ComputeTopologicalLeaf` at0x20b79b0
marks graph-terminal generators. Its name does not mean botanical leaf geometry.
Both decompilations returned TARGET_STATUS=true, exit0. This supports the local
hierarchical-generator interpretation; it does not establish SpeedTree's leaf
placement algorithm. ELF SHA256:
`9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c`.
Private vendor analysis remains in `~/Downloads/outshine-speedtree-analysis/`;
no vendor code, plant model or texture enters the repository or these renders.

## Reproduce

Use `.venv`, installed `lib/`, NumPy/SciPy/Pillow and bpy5.2.2. Set
`PYTHONPATH=lib/src:test/experiments/vegetation` and `OPENBLAS_NUM_THREADS=1`.

```sh
.venv/bin/python test/experiments/vegetation/broadleaf_crown_experiment.py \
  --output /tmp/outshine-vegetation-broadleaf-comparison
.venv/bin/python test/experiments/vegetation/broadleaf_crown_experiment.py \
  --output /tmp/outshine-vegetation-broadleaf-comparison --cache-only
.venv/bin/python test/experiments/vegetation/broadleaf_crown_preview.py \
  --inputs /tmp/outshine-vegetation-broadleaf-comparison \
  --output /tmp/outshine-vegetation-broadleaf-optical --form oak --area 55
.venv/bin/python -m unittest broadleaf_crown_checks biome_tile_checks
```

Repeat for `--form beech`. To run all22 controls, also add `lib/tests` to
`PYTHONPATH` and append `test_blades test_plant_plans test_graminoid` to the
unittest command. Current original comparison data/meshes are retained under
`/tmp/outshine-vegetation-broadleaf-comparison/`, whole-plant images/receipts under
`/tmp/outshine-vegetation-broadleaf-optical-e0e815d32/`.
The retained source/receipts identify CPU, pair-renderer and tile-renderer freezes
separately. Plant expansion, physical triangles, prepared-cache decode, offline
rendering and native frame budgets are separate costs; runtime LOD/cache and
worldwide species/biome acceptance remain open under #22/#25/#48.

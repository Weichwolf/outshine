# Stored building products under translation and direct relighting

Owner #1167. Extend the matched-original-remainder comparison from
`building_mattes.md` without rebuilding its source products, changing plane/grid
parameters, adding a native representation or relaxing image requirements.

The diagnostic starts at one existing root camera and moves physically in its
fixed right/up/forward basis. Defaults are300/100/200m/s for one second, sampled
at nine poses including the original midpoint. This is a finite image probe,
not a60-frame performance run or a worldwide motion certificate. Pixel format,
1280×720 output and16×16 per-pixel quadrature stay unchanged. The union of all
projected endpoint geometry over the declared poses defines one fixed screen
ROI. A smaller ROI is rejected by the common image comparison.

For each pose, compare the full original, original-cut prisms, prisms with the
same complete original remainder as the mattes, separate-profile mattes and
identical-normal-merged mattes. Products and fine-reference material fields are
loaded/prepared once per source cell. Fine geometry remains an offline oracle;
the image loop performs neither source-product construction nor normal merging.
Three fixed lighting controls repeat the same path: previous daylight, low
elevation direct light and low ambient-only illumination. Ambient-only is no
simulation of night, local lights, shadows, weather or a physical sky.

RGB/T error is evaluated against the complete original at each physical pose.
Also report the L1 change of the signed error field between consecutive poses,
after subtracting the original at its corresponding pose. The common ROI keeps
pixel coordinates aligned. This removes changes reproduced by the original;
it is a fixed-screen residual-change diagnostic, not motion reprojection,
perceptual flicker, an error bound or a temporal filter. A constant wrong image
offset can have zero residual change while retaining a nonzero spatial error.

The original dielectric GGX lighting model stays unchanged. Existing renderers
gain explicit optional light/intensity/ambient arguments with identical default
values. No baked illumination, geometry/normal/roughness change or new runtime
cache contract. Source response receipts stay applicable. Camera-/Caster-Ready
domains and full physical LOD transitions remain separate work.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/compare_building_matte_motion.py \
  --input /tmp/outshine-transition-products \
  --products /tmp/outshine-matte-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --native-cell 33 \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --frames 9 --duration 1 --local-velocity 300 100 200 \
  --output /tmp/outshine-matte-motion
```

Input generation is unchanged (`building_transition.md`, `building_mattes.md`).
Outputs are explicit Temp paths. First/midpoint/last images and C/T arrays are
retained, as are poses where merged mattes lose RGB or T to matched prisms.
Each lighting path retains a contact and complete per-pose metrics.
Actual work, array sizes and process costs belong in the PR; no native, GPU,
frame-rate, full allocator/copy/bandwidth or population-scaling admission.

SIGGRAPH-first: [Décoret et al.,2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
PDFp.6/Figs.11–12 discuss parallax/relighting and representation artifacts;
text and figures optically checked. The experiment uses the bounded source-mask
heuristic from the preceding delivery, without importing the paper's geometric
guarantees or shadow results.

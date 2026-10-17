# Native per-axis detail — #1

`PlantAxisDetail` implements the [parametric tube model and derivation](plant_axis_detail.md).
The shared PlantAxisSurfaceLimits keep its integer domain identical to the mesh
producer. Positive coefficient operations and bound evaluation round upward.
Bisection plus side-count enumeration chooses the smallest supported triangle
count under that evaluated model; equal triangles choose fewer vertices.
It does not construct/sample a mesh to discover which detail it needs.

PreparedPlantDemand now has two explicit alternatives: fixed AxisSegments/Sides,
or AxisParametricErrorM with both fixed counts zero. Mixed requests fail.
Existing fixed demands retain their exact behavior. Measurement and assembly
share the same per-axis choice. Geometry/instance/reference budgets still apply
before product allocation; insufficient/unreachable demand returns no product.
Every axis/leaf ID, ownership and leaf pose remains intact. Smaller geometry
is a new requested asset variant; callers must include detail mode/value in its key.

The model covers ideal local capped circular axes without twist. It is **not**
a bound for Float64/libm sampling, Float32 emission, frame transforms, shaders,
normals, projected pixels, radiance or motion. The world LOD/error allowance is
unchanged; no runtime producer is switched to this mode by this component work.

## Verification

```sh
JOBS=6 sh test/run.sh outshine/src/generators/flora/PlantAxisDetail \
  outshine/src/generators/flora/PreparedPlantBuild outshine/src/generators/flora/PlantPlan
```

The selector case independently enumerates the complete integer domain for
straight/tapered/signed-curvature/thin-axis examples and multiple budgets. It
checks minimal supported detail, infeasible/invalid requests, singular profiles
and an independent long-double circle bound. Assembly tests exercise unequal
axis details, measured/actual inventories, preserved owners and exact leaf
matrices. Adaptive preflight and budget refusal are allocation-free. Normal,
ASan/UBSan and container arms pass; existing fixed-plan/surface cases still pass.
Product units pass focused clang-tidy with full build flags. Case analysis retains
the known shared Check.h/libc++ parameter-name findings (#603), not suppressed.

## Actual controlled Metal portrait

The existing6.5m terminal18 oak study, seed718, has1105axes/5544leaves. Three
actual640×720Metal images use identical camera, recipe, materials and leaf detail:
fixed8×6wood; model-selected5mmwood; and a separate32×32wood control. All were
opened. Leaf IDs, owners, variants and all5544Float64 matrices are exact across
all three outputs. Source growth-form gaps remain the same; this is not a new
mature species, forest, world or frame-budget acceptance.

| Actual product | Fixed8×6 | Model-selected5mm |
|---|---:|---:|
| Wood triangles |119340|27022|
| Unique prototype vertices |75476|23031|
| Unique prototype triangles |119612|27294|
| Whole-instance triangle references |307836|215518|
| Prototype array bytes |3249456|882960|
| Encoded product bytes |4068694|1702198|

Thus wood work falls77.36%, complete triangle references29.99%, and encoded
bytes58.16%; leaf placement storage stays709632B. One local cold diagnostic
measures0.335ms preflight and1.528ms assembly versus the fixed path's separate
measure/build counters in the retained log. These are individual timings, not
p99 or device FPS; the extra selection work is explicit.

Against the separate dense control, pixels.py reports mean absolute RGB error
in8-bit channel units0.109751(fixed) versus0.100613(selected). Changed pixel counts
are1757versus1875; the maximum181/255is unchanged. This view's mean error improves,
not every pixel. All three whole images look similar at this scale; local bark
samples differ. No other-view or lighting/coverage error bound follows.

Original inputs, build commands, binary/source hashes, products, three images,
actual row traces and pixels.py results remain privately under
`/tmp/outshine-native-plant-recipe-39bf9e07d/axis-detail/`. Private cache requests
are by key; no spatial-index/world-registration claim is made by this portrait.
Next: compose numerical/frame/projection error before runtime screen-space use,
and continue the validated species/near-far world integration.

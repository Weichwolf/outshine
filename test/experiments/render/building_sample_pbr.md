# Capture-calibrated building GGX view control

Owner: [#1759](https://github.com/Weichwolf/outshine/issues/1759), Python only.
After removing the independently identified repeated-filter comparison error,
the512-source prefix still differs from originalSS64 by about5.89%RGB/3.30%
coverage. This control asks how much **view direction alone in the existing
GGX model** changes the image on the same stored source points/materials.

## Inputs, model and positive control

`building_sample_source.read_source` binds the exact old795a1cf4 capture,
d485bf1c stored field, native capture-lit RGBA and native prefix query. All43049
occupied source points remain; no radiance-based selection or fitting occurs.
Depth maps to world height `(4d−2)H`, and source cell centres retain the existing
orthographic grid. Coverage, materials, normals and positions stay fixed.

Use existing `material_filter.direct_ggx`: ordinary opaque isotropic dielectric,
F0=.04/F90=1, actual stored base colour/roughness/normal, normalized normal as in
that reference. Native Source's three materials have the ordinary IOR1.5/
specular-factor1 defaults, no metalness/transmission/coat/sheen/anisotropy.
Unsupported metalness, invalid material or light changes are rejected.
White directional100, azimuth0/elevation45 and zero environment match the capture.
The capture response uses its constant orthographic outgoing direction; the
counterfactual uses normalized `query_eye−stored_point` in the same evaluator.

The versioned energy-table NPZ checksum is verified. Its shader SHA866ea367…
matches the locally generated native `brdfTables.glsl` exactly, all arrays equal.
The generator is unchanged since pinned0eb2e739d; its energy header only changed
an include path. This source/table correspondence and capture calibration are
not an independent native perspective-BRDF readback or a backend error bound.

| Actual fixed query control | RGB relative L1 | Coverage change |
|---|---:|---:|
|Native source versus capture model, all43049 points |0.03578% |0 |
|Capture model versus perspective model, source |0.10032% |0 |
|Native-source versus capture-model pixel integrals |0.03559% |0 |
|Capture-model versus perspective-model pixel integrals |0.07243% |0 |
|OriginalSS64 versus capture-model pixel integral |5.89095% |3.30513% |
|OriginalSS64 versus perspective-model pixel integral |5.90046% |3.30513% |

For this unchanged32px query, existing Float64 prefix integration equals direct
16×16 source block means to6.20e−13 maximum RGBA. These are disjoint positive
means: in real arithmetic, `sum|A(f−g)| ≤ sum|f−g|/256`, by the triangle inequality
inside each block. Evaluated in Float64 and divided by originalSS64 RGB energy,
the model's view-change upper value is0.09953%; actual pixel change0.07185%.
This bounds only this model response through the aligned operator, not native
query-model error, arbitrary poses, original geometry or reference uncertainty.

The opaque coverage is exactly identical before/after the modeled view change.
Thus this view-only PBR operation cannot repair the3.305% coverage error at all.
Its RGB response is also far smaller than the remaining approximately5.9% image
error in this calibrated model. Prioritize the remaining geometric/capture/
sampling comparison; this is not a new runtime held-view/LOD admission and does
not justify skipping required view-dependent shading in other content/poses.

SIGGRAPH first: [Karis2013, pages2–3](../../../doc/references/materials/siggraph/2013-unreal-engine-4-shading.pdf),
visually inspected: blending results versus parameters, Cook–Torrance/GGX and
roughness-squared parameterization. Existing Outshine correlated visibility and
energy tables remain; no UE4 visibility remapping or Lambert-only substitute.

## Reproduce, evidence and costs

Use existing `.venv`/NumPy, Matplotlib only for the private scientific figure.
Set `PYTHONPATH=lib/src:test/experiments/render`, BLAS/OMP threads1:

```sh
python -B -m unittest test_building_sample_pbr test_material_filter test_field_view_integrals test_field_view_sampling
python -B -m compare_building_sample_pbr --output /tmp/outshine-research-pbr/run
python -B -m plot_building_sample_pbr --study /tmp/outshine-research-pbr/run --output /tmp/outshine-research-pbr/error.png
```

Freeze actual kernels first; fresh private system-temp outputs only.28 focused
controls pass, including independent cell/height coordinates, bounded expansion,
same-view identity, invalid table/light/material rejection, all-point retention
and actual capture/pixel opponents. The native source/model/error figure was
generated and opened; error scales are row-specific and explicitly labeled.
No README/doc images, new renderer, representation family or native change.

The diagnostic performs86098 GGX evaluations and three existing prefix builds.
One input row array6887840B, source positions1033176B; model fields16777216B,
native field4194304B, each transient prefix8421408B, held pixel arrays1376256B.
Explicit copied-payload subset35202596B: masked material channels, depth cast,
independent-block cast and immutable-prefix copies. Other assignments, IO and
temporaries are uncounted; this is not total memory traffic.

Separate Memray1.20 instrumentation including Python allocators of the complete
comparison (inputs decoded outside tracking) records11613 allocation events,
377371871B requested and39191694B tracked heap peak. Requested bytes are not
copies/retained bytes/RSS; instrumented timing is not the normal execution time.
Reports preserve frozen commit, load, CPU/wall, OS VmHWM and versions. One
noninstrumented1fd3731a0 run:0.404sCPU decode/verification,0.327sCPU model/integrals,
108576768B OS peak, normal priority with concurrent work/load25.61/24.37/19.28.
No equal-quality alternative speedup, native/device/p99/UMA or World admission.

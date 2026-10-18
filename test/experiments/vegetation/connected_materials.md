# Original material on connected leaf carriers

Owner [#704](https://github.com/Weichwolf/outshine/issues/704), continuation of
[the exact-pose/coverage study](leaf_patches.md). This completes its original
material comparison as an experiment. It does not admit a universal leaf LOD,
botanical BSDF, native shader, whole-tree fallback or forest product.

## Original inputs and mapping

Use the same six-file cold source and saved connected32 geometry from the parent
study. Original authored `fern-colour.png` is256×256 RGB8 linear data;
`fern-height.png` is256×256 unsigned16, height scale.000060m. The same original
`fern_material.leaf_material` adapter uses Principled roughness.55, specular IOR
level.2, bump strength.25 and a.12 Translucent mixture. Its content hash and all
three source-file hashes remain bound. The filename describes an existing
generic study material, not a validated Quercus or fern optical measurement.

Source UV is `u=(s+1)/2,v=t`; negative petiole coordinates use the original image
nodes' linear REPEAT mode. The connected surface uses an unrolled(x,y) domain.
`connected_material_fields.py` interpolates the original coarse source UV and
corner normals over that domain before sampling pigment/height. Averaging UV
and then sampling colour would not preserve the same filtered material.
Output sizes32/64 share **the identical saved connected32 geometry/domain**;
neither resolution changes poses, root attachment or analytic physical leaf area.

Three premultiplied RGBA fields hold colour/coverage, object-Z slope moments
E[x,y,xx], and E[xy,yy]/height. Alpha is duplicated for independent field checks.
The cross moment also validates positive-semidefinite covariance; the isotropic
shader consumes only its trace. Content identity binds the original material,
carrier recipe, size/order and actual field bytes. Finished readers check the
identity, equal alpha, zero-mass fields, ranges and covariance before use.
A positive object-Z slope chart refuses any captured source normal with z≤.25;
the eight actual sources have minimum coarse-corner z .802–.949. This chart does
not cover arbitrary folded leaves. No original source is changed to fit it.

Capture uses4×4 midpoint samples per texel. The earlier unit-cosine images use
2×2 capture: alpha can differ by.25 at a native texel, so their old numerical
tables must not be silently reused as this material's coverage result.

## Two explicitly approximate PBR consumers

`connected_material_shader.py` copies the actual original adapter and replaces
its two source image nodes with the finished unlit fields. Pigment and height
are divided by coverage. The conditional mean slope resolves a unit direction,
which is rotated by the exact original blade-to-host basis before world shading.
The original bump scale/strength, dielectric shader and mixture remain.
This unit direction is **not** the earlier raw normal moment renormalized while
discarding its length: the slope distribution remains separately represented.

Middle subject: mean slopes at original roughness. Right subject: the existing
`leaf_normals.resolve_moments` isotropic proxy,
`roughness = min((.55^4 + trace(slope covariance))^.25,1)`.
The shader applies that expression after linear image filtering. Same original
mean-height bump is used by both. Unresolved bump variance, normal/colour
correlations, anisotropic or multimodal GGX convolution are not preserved by
this proxy. Mean height on a reduced surface also changes its physical metric.
Neither consumer is an exact filtered version of the full original BSDF.

## Independent controls and actual images

The original-image EEVEE readback samples64 finite UV patches of half-width1e-5.
Raw PNG8 readback versus independent CPU linear REPEAT sampling has maximum
channel errors.001098 colour/.000784 height, declared limit.006. Both original
RGB8 and height16 values/orientation are checked. A zero-area UV control renders
black in this software EEVEE setup, even after copying its decoded pixels into
a float image; constant emission still renders correctly. It is retained as a
failed oracle fixture, not mistaken for a black source material.

Two flat-field actual-BSDF controls use identity and90° rigid normal frames.
Both consumers match the original adapter within.000226 displayed RGB-channel
mean, declared limit.01. The two original source responses differ by.131510;
the chosen asymmetric light therefore really tests rotation. An earlier light
had identical dot products and was rejected as a rotation oracle.

Frozen reader13e34a014 renders host17:714 original versus168 triangles for each
candidate. All eight2560×1440 PNGs were actually opened: two oblique/grazing
directions, two opposite lights,32/64 material maps. Left original / middle mean /
right variance. Sun shadows are disabled to isolate material response; ordinary
view occlusion remains. Pigment/vein structure and signed light regions are
present; the earlier internal grid seams remain absent.32² loses edge/vein detail;
64² recovers more, with remaining reduced-surface/highlight differences.
Variance is visually subtle and is not declared the global winner.
In every32/64 pair the original x=0..849/y=0..1439 RGB crop is pixel-identical;
camera, source, light, geometry and subject placement were unchanged.

## Finite GGX comparison and costs

Reuse Material's `relief_shading_reference.specular_from_dots` and the existing
two-sided `leaf_gloss` convention: correlated-Smith single-scatter GGX,
alpha=roughness², F0=.04/F90=1. This is distinct from the full Blender Principled
adapter. The normal-filtering reasoning follows local SIGGRAPH2007 Frequency
Domain Normal Filtering, page1/figures, visually inspected. Local SIGGRAPH2013
Real Shading in UE4 page3 was opened for GGX/roughness/Fresnel; that paper's
geometry approximation is not claimed as our implementation.

`connected_material_specular.py` compares the exact same finite geometric-normal
contacts at32²/4×4 capture against both proxies, all eight actual variants,
12 independent spherical views×12 lights:1152 cases per method. Error is summed
absolute response difference / sum coverage mass, before view geometry or bump.

| Method | Mean L1/mass | Maximum L1/mass | Maximum pixel error |
|---|---:|---:|---:|
| Mean slopes | .000030912 | .002548090 | .123249793 |
| Isotropic variance | .000030781 | .002547510 | .123262756 |

The average gain is about.42%; the worst pixel becomes slightly worse. This
does not justify generally preferring the extra variance work/data. Grazing
cases remain counterexamples; actual full-source PBR filtering is unresolved.
The array-roughness consumer is checked against unchanged scalar gloss cases,
including opposite directions and the analytic normal-incidence value, without
loosening the1e-15 comparison. The complete source/installed set also runs the
existing gloss energy/reciprocity controls. No original library/GGX code changes.

| Eight-template preparation |32²|64²|
|---|---:|---:|
| Coarse source faces |272|272|
| Capture image slots per variant |16384|65536|
| Retained Float64 field bytes |786432|3145728|
| Float32 image-buffer payload bytes |393216|1572864|
| Preparation wall seconds |.194|.835|
| Linux process RSS peak bytes |52322304|63660032|

Preparation creates no host poses or whole crown. These are actual CPU array
sizes, before native packing/mips, containers, cumulative allocation/traffic or
GPU storage. The copied Blender material graphs share24 field images; duplicate
per-leaf image copies are avoided. The diagnostic source control still loads
its original images; this renderer is not a production warm-cache benchmark.
32/64 rendering wall104.35/97.78s and process peaks2492923904/2486325248B use
software llvmpipe EEVEE5.2.2,32 samples, `nice -n10` on a shared4-CPU WSL host.
Those times are not a resolution speed ranking or RTX/A18 device evidence.
The finite GGX study takes7.859s/57462784B on the same host, separately scoped.

## Reproduction and remaining work

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread and
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
First `prepare_connected_materials.py --inputs <cold-source> --study <connected-study>
--output /tmp/<fields> --size 32` or64. Then `material_image_probe.py --original
<cold-source> --output /tmp/<image-probe>` and `calibrate_connected_materials.py
--original <cold-source> --output /tmp/<calibration>`. Render with
`render_connected_materials.py --study <connected-study> --fields /tmp/<fields>
--original <cold-source> --output /tmp/<images>`. The pure CPU comparison is
`connected_material_specular.py --original <cold-source> --carriers <connected-study>
--fields /tmp/<fields> --output /tmp/<specular>.json`.
Render environment: LIBGL_ALWAYS_SOFTWARE=1 and XDG_CACHE_HOME/MESA_SHADER_CACHE_DIR
under/tmp. All private products/controls/current images are in
`/tmp/outshine-vegetation-material-study-056dfd094`; original inputs remain at
the cold-source path documented in the parent study.

77 source and77 fresh installed-library controls pass. The isolated wheel matches
all212 focused-source library Python files after the current master merge;
its SHA256 is aeea662c0cc76187c8f393ec21436990d3cc03fee55b67494b80410d40f8f68a.
Run the parent study's13 `_checks` modules plus `connected_material_checks`,
`connected_specular_checks` and `leaf_gloss_checks`; add the render experiment
directory to PYTHONPATH. The count includes the existing seven gloss controls,
not77 new tests. Sixteen finished32/64 field products revalidate without source
generation. Ruff/format/function-size/diff and lint-docs pass. These Python
checks do not replace native engine/device gates. Permanent Draft202 library
work is outside the focused PR and installed-library equality statement.

The bounded original-material comparison is complete; source fidelity and
measured counterexamples are explicit. Whole-crown/forest demand and integration,
species quality, angular/mip/wind/shadow transitions and physical leaf transport
remain at #23/#643/#1 and the full active goal. No public/native/cache contract
or shared material-owner parameters are changed. This study promotes no PBR
normal-filter approximation to a generally admitted runtime leaf shader.

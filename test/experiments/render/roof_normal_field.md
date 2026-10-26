# Source-owned roof normal fields (#1089)

This bounded comparison keeps the #1085/#1088 profile geometry unchanged and
adds a relightable normal field generated directly from the original roof plans.
It improves visible roof lighting in an elevated control, but the dense atlas is
not a competitive admitted replacement. Raised profile walls still replace many
original roof hits. No native renderer, format, budget or source was changed.

## Representation and consumers

`bake_roof_normal_field.py` takes the verified original planar roof domains,
without triangulating a Shell/Fine mesh. Each needed nonflat source part gets
one fixed 16×16 RGB8 normal tile with a one-texel replicated gutter. A texel takes
the normal of the closest original domain at its centre. Extending that normal
outside a footprint does not create geometry/opacity or fill a hole. There is no
pitch/resolution sweep or cap on the number of original roof domains.

Normals are packed/unpacked through the existing normal recipe before RGB8
encoding. They remain in canonical parent axes, independent of camera/light.
The source ordinal/part registry retains original frames, XY bounds, tile
rectangles and owned roof material codes. Material codes reuse the existing
`prepared_material_fields.at_local` owner. Distinct anonymous source ordinals are
not collapsed. Original unsupported source 2893 remains completely original.

Flat roof domains already have the profile's original packed up normal. They
produce explicit no-field entries, no texture samples and no field UVs. An empty
all-flat product is valid. A missing contract for a generated roof is an error;
it is not silently treated as flat.

`roof_normal_field.py` is a ready NumPy consumer, with no form decoder or GEOS.
It maps only generated roof corners to the canonical atlas; original fallback,
walls, floors and no-field parts keep their existing normals. The prepare script
writes per-geometry display adapters, not camera-specific atlas products. All
three original far poses reuse identical atlas/UV contents. The near scene has
no atlas or UV reference at all, before its renderer can load either resource.

The optional `blender_roof_normal_field.py` adapter reads only ready assets.
An RGB8 Non-Color image and eligible roof UVs override the Principled normal,
transformed to the shared diagnostic frame. Texture alpha selects the normal
field; it does not change material alpha. Native UV/metres/categories, mesh
positions/indices/source IDs and geometric coverage are unchanged. Closest
sampling in Blender is recorded; it is not a qualified native filter/NDF/BRDF.
Blender reports an 8-bit image with 32 total bits/pixel, rather than a float image.
The float32 pixel-set API still receives 3,240,000 staging bytes for this atlas;
actual driver/GPU residency and all internal copies remain unmeasured.

## Actual images and controls

The existing complete native-cell fixture is the independent opponent. Original
static-020 day/backlight/night images from #1088 are reused. New field images use
identical verified 1280×720 cameras, source colours, roughness/metallic parameters,
light, ground and 16-sample Eevee settings. The unfielded renderer control is
pixel-identical to the earlier night profile. PNG containers differ because their
Date/RenderTime metadata differs; file-byte identity is not claimed.

An added roof-facing control raises the eye 1,500 m in the common-up direction
and looks at the same complete cell. All already-used profile sources still pass
the unchanged one-pixel metadata query (maximum 0.673542 px); it uses the exact
same mesh and ready field, without generating extra geometry or reducing content.
Original, plain-profile and field variants are actually rendered by day/night.

Final results below include three actual static field images, two elevated field
images, four elevated original/plain opponents, one unfielded control and one
near no-field control. Five earlier field renders were superseded by the exact
flat-roof no-field handling; their successful statuses and comparisons remain
available. All final relevant pixel/error sheets were opened, including day,
backlight and night. `test/scripts/pixels.py` independently confirms the final
roof-facing day pair: 273 changed pixels, 197 over one, maximum error 12.

| View/light | Max RGB8 error, plain → field | Mean absolute RGB8 error, plain → field |
| --- | ---: | ---: |
| Static day | 12 → 12 | 0.00021846 → 0.00022172 |
| Static backlight | 15 → 15 | 0.00023799 → 0.00023872 |
| Static night | 38 → 38 | 0.00026837 → 0.00027416 |
| Roof-facing day | 19 → 12 | 0.00104203 → 0.00096788 |
| Roof-facing night | 40 → 38 | 0.00087312 → 0.00084997 |

Whole-frame means include the common sky/ground and are not a quality gate.
Changed-region statistics and enlarged original-pixel crops accompany them.
The upper-view day improvement is real, while the main static case does not
improve. The near no-field image has zero changed pixels against the original
near opponent; its render receipt confirms no normal-field resource was loaded.
Native procedural PBR/emissive windows, weather/terrain, filtering/MSAA/TAA,
shadow/reflection and world/device admission remain unqualified.

## Independent normal/depth diagnosis

`probe_roof_normal_field.py` consumes saved original/profile depths, front faces
and the same four fixed samples per pixel. The existing depth query is reused;
the normal field does not rerun source recipes. A separate roof-facing depth
control runs the original triangle/Rtree reference at the actual new view.

The supplied native capture has no native part/face bindings for these surfaces.
The first diagnostic therefore had no same-part roof observations; that was
missing identity evidence, not absence of roofs. The final diagnosis uses the
actual vertex material program and requires a unique source/roof-code association
across both field and no-field parts. Ambiguous associations stay unknown; no
native face/part identity is invented. Sampled material categories are decoded
from original triangle corners, not interpolated integer values.

In the original far poses only 3/2/1 point-normal samples receive a field override.
At the same source, 27/28/23 original roof-category hits instead land on profile
nonroof surfaces. The worst same-source normal error stays 90.53°/92.57°/90.48°.
A roof normal texture cannot change which wall or other source is first hit.

The roof-facing control has 355 overrides. Among 155 unambiguous same-source
roof-code samples, mean normal error decreases 33.93°→3.04°; samples over one
degree decrease 155→9. Maximum error still reaches 78.85°. Sampling at the actual
original-surface hit instead of the proxy hit still has 13 errors over one degree
and an 88.71° maximum: the nearest sampled field is not a stable filtered normal
representation at domain discontinuities. It is not evidence for another tile
resolution sweep or for a universally adequate single normal.

The same elevated depth control records 947 original hits, 25 added/zero lost
samples, 243 changed source owners and 208 same-source roof-to-nonroof hits.
Geometry/first-surface errors remain even though the geometric envelope query
passes. Their cause is not repaired by replacing roof shading normals.

## Work and storage

The final preparation has 605 sampled tiles, six exact no-field parts, 154,880
texel centres and 371,712 original-domain distance evaluations. The 450×450 RGBA8
atlas has 810,000 bytes, including gutters/unused grid positions. All arrays,
including source frames, lineage, rectangles, bounds and code/no-field tables,
total 902,032 bytes. The far dense corner-UV adapter adds 185,472 bytes. Its near
counterpart adds zero. This is the implemented diagnostic layout, not a lower
bound on compact analytic coefficients, GPU descriptors or all future layouts.

Combined with the unchanged profile diagnostic arrays, the far result costs
2,288,176 bytes versus 2,226,396 for the complete original diagnostic opponent:
61,780 bytes more. Native packed assets, image staging, shader/driver resources
and residency are different quantities; no native format/memory winner follows.
The no-field near path avoids the atlas and the earlier 340,056-byte near adapter
entirely. No atlas dependency is kept in that scene.

One ordinary final preparation on `0892561e9` reports 0.426 s original roof
planning and 0.804 s field baking under concurrent host work. These are Python
observations, not a native timing claim. A separate trace of the data-equivalent
`c909dc66c` preparation, after imports, reports 82,208,190 peak traced bytes,
11,838 live traced blocks after the run and 284,852,224 raw process peak RSS
bytes. This includes the independent original-reference preparation, not just
field decoding. Tracing changes costs and omits some native GEOS allocations.
Cumulative allocation/copy traffic and GPU/device peaks remain unmeasured.

All final/traced consumed pixels, source geometry and UV arrays are checked
word-for-word against the actual rendered `c909dc66c` product. Later changes add
only the missing flat-code association metadata and reuse the code owner; they
do not change consumed render inputs. Final point diagnostics use the final
metadata. No oracle/tolerance was changed.

## Decision, sources and next hypothesis

Keep the dense normal atlas as a bounded comparison, not an admitted replacement.
It establishes a real upper-view lighting benefit but loses the implemented
storage comparison and leaves the dominant first-surface error. Flat roofs do
retain the useful no-field path. Do not tune tile resolution or replay the old
XY-column/projective pack experiments without a new hypothesis.

The next concrete candidate should keep the original plan-based roof height
surfaces as well as normal/material variation and intersect them before shading,
so raised proxy walls do not replace original roof hits. It must still beat the
complete opponent after field/query/storage work and resolve filtering/casters/
transitions; neither this result nor existing per-triangle projective fields
proves that new candidate succeeds. Master owns native contracts/integration.

SIGGRAPH/TOG 2007, Han/Sun/Ramamoorthi/Grinspun, *Frequency Domain Normal Map
Filtering*, was read including the first two PDF pages, V-groove/filtered-material
figures and the BRDF/NDF distinction. It motivates retaining a normal distribution
under minification; the sampled tile here does not implement its convolution or
SH/vMF models. Local source:
`doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf`.
The existing #1088 geometry comparison and `column_field.md`/`projective_field.md`
remain family controls, rather than rejected claims about all height/normal fields.

## Reproduction

Use the existing `.venv` and `PYTHONPATH=lib/src:test/experiments/render`, with
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. Inputs are the versioned full-view
fixture; prepare the #1088 baseline and saved ray controls with its documented
commands. Pass fresh system-temp output paths; no private input tree is needed.

```
python -B -W error -m unittest test_roof_normal_field test_far_height_profiles test_far_profile_fields
python -B -W error test/experiments/render/prepare_profile_normal_fields.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --profiles /tmp/outshine-profile-baseline --output /tmp/outshine-roof-field
python -B -W error test/experiments/render/probe_roof_normal_field.py \
  --profiles /tmp/outshine-profile-baseline --field /tmp/outshine-roof-field \
  --depths /tmp/outshine-profile-rays --output /tmp/outshine-roof-normal-errors.json
python -B test/experiments/render/render_far_height_profile.py \
  --scene /tmp/outshine-roof-field/roof-control/field/scene.json \
  --lighting day --output /tmp/outshine-roof-field-day
```

The final affected run passes 15 tests, including analytic gable directions,
frame changes, unchanged eligibility/geometry, source identity, empty/no-field
contracts and a fresh ready reader with forms/GEOS imports explicitly blocked.
Seven Python files pass focused Ruff check/format. The renderer uses the existing
bpy teardown workaround and omits `-W error` for its installed bpy deprecation.
For software-only headless verification set `LIBGL_ALWAYS_SOFTWARE=1` and
`EGL_PLATFORM=surfaceless`; it is not a native device measurement.
Evidence root: `/tmp/outshine-research-profile-normals-92ceffa94`.
No new dependencies, native code, source export or repository images.

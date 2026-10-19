# Source-group triangle / matte / relightable-card comparison (#12)

This follows [master's far-product priority](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6259935181).
This experiment tests one small source group against the existing Shell alternative;
it changes no native product, cache contract, runtime threshold or shader binding.

## Corrected common triangle reference

The previous automatic Trimesh backend was PyEmbree. Its iterative multi-hit
query can skip coincident opposite-facing triangles before front-face culling;
`intersects_location` additionally deduplicates locations. A back-first two-sided
quad returns two missing hits where analytic depth is 1. Merely requesting IDs
from the automatic backend does not fix that counterexample.

`source_far_reprojection` now explicitly uses Trimesh's triangle/R-tree backend,
retains all intersected triangle IDs, culls per ray, then selects minimum positive
camera-ray parameter with a deterministic face-index tie. Directions need not be
unit length. The 0.1 near threshold is in that parameter, as before. Broad-phase
queries are batched at 256 rays; a prepared immutable source index can be reused.
This is an offline geometry reference, not a software model of native depth ties,
raster edge rules, native materials, or a fast GPU implementation.

The coincident-side negative control fails on the old implementation and passes
on the corrected one. An independent exhaustive Moller–Trumbore test checks 64
non-unit rays against 100 random triangles without Trimesh/Embree broad phase.
Four actual source views (bbd9/60 and 6d41/1410, yaw 0/90, distance 8 extents,
32 m translation) compare 87,278 rays: old/new coverage, face IDs and depths
agree to the reported tolerance. A further exhaustive check of 512 of those rays
matches coverage, with maximum depth residual below 2.8e-12 m.

The old 24-view LDI and 432-view hull/preflight reports remain frozen historical
measurements, not rerun evidence for the corrected reference. Their byte counts
are unaffected. Four matching controls do not validate every old view or arbitrary
coincident geometry. New card results below use only the corrected reference.

## Common input, rays and shading

Original cohort bbd9aab7431104d6b1565886f434e239db5fb511a754a5c529c52b98665dc2ad,
ordinals 60 and 80, supplies two nearby complete original sources. Original
index/forms, generated parts and anonymous IDs are preserved. Their common ECEF
anchor is checked; different anchors or repeated original cohort/ordinal pairs
are rejected. This is not a complete regional/city membership set. Per-triangle
source membership exists in the joined diagnostic; a native Source/Part/Face
ledger for all emission paths remains missing.

Fine and verified native Shell come from the unchanged SHA-pinned exporter/GDB
controls described in [source_far_assets](source_far_assets.md). The loader checks
stream hashes, original input equality, actual detail type and source identity.

All four methods use identical 1280×720/original 1044.33155 px focal cameras,
four regular subpixel samples per output pixel, a shared enclosing cropped ROI,
and native colour-factor/packed-normal Lambert shading. The two sun directions
are shared between methods. No facade UV shader, native metallic-roughness PBR,
shadow, terrain/world occlusion, fog or exposure is evaluated. Panels enlarge the
same ROI with nearest filtering; saved arrays retain original pixel resolution.
Alpha L1, half-coverage losses/gains, common-surface depth, foreground-union RGB
RMSE and common fully opaque RGB RMSE separate different error contributions.
Union RGB still includes coverage differences; no empty image counts as success.

## Two actual triangles, two texture proposals

The capture camera is 8 source extents from the group. A cropped 64×19 texture
uses four original-triangle samples per texel. The actual Float32 world quad,
Float32 UVs and two UInt16 triangles occupy 92 B. Novel views intersect those
stored triangles and derive barycentric UVs; no source triangles are queried by
the card path. Filtering uses premultiplied data and zero exterior samples.

The matte stores RGBA8 colour already lit by sun A. The live card instead stores
RGBA8 unlit colour-factor/alpha and RGBA16F normal moments. Filtered moments are
not normalised: doing so would invent a new unit normal. Their Lambert response
still approximates unresolved normal/colour mixtures and the `max(N·L,0)` function;
this is not an exact general relighting model or a native material asset.
The source mesh remains the fallback for every unvalidated card domain. No card
receives opaque-occlusion permission or a guessed geometric error certificate.

| Representation for these two sources | Triangles | Raw payload |
| --- | ---: | ---: |
| Existing Fine | 2,304 | 141,072 B |
| Existing native Shell | 160 | 9,456 B |
| Static matte, RGBA8 + quad | 2 | 4,956 B |
| Live card, RGBA8 + RGBA16F + quad | 2 | 14,684 B |

These are specific layouts, not lower bounds on all possible representations.
Texture counts omit mipmaps, descriptors, memberships, depth, fallback and layout
overheads; native bytes are `Raised::UsedBytes`, not final GPU/cache residency.
Four bilinear taps read 4 B/texel for the matte or 12 B/texel for the live payload
before cache reuse. Bake uses 4,864 source rays. The diagnostic currently allocates
at least nine whole-texture floating arrays / 280 B per texel per combined card
query, plus per-ray arrays and library work. This explicit allocation lower bound
is not a complete profiler ledger. Python query/preparation/bake times and process
peak RSS are reported separately; they do not establish native GPU performance.

## Observed decision

Nine camera cases combine distances 8/32 extents, 0/32/64 m translation and
0/5/15/45/180 degree orbit angles, each under both lights. Original and shifted,
15/45-degree, distant and backside panels were inspected. At the capture pose,
the live card has alpha L1 235.57 px against 2,065 Fine covered pixel-equivalents;
64 m translation raises this to 295.31 px. At 15/45 degrees it reaches
671.45/1,175.43 px. The backside has no card coverage and must use fallback.
At distance 32, alpha L1 remains 14.51–14.91 px against about 128 covered pixels.

The live card responds to sun B, reducing capture-pose RGB union RMSE from the
matte's 0.1748 to 0.1100 (linear diagnostic range 0–1). That does not repair form,
coverage or normal-mixture errors. Its capture-pose light-A RMSE is 0.1243 versus
Shell's 0.0215. Shell matches Fine coverage in all nine cases, but still changes
colour/normal appearance; this isolated Lambert result is not native acceptance.
At the capture pose, common-surface camera-Z depth differs by up to 59.58 m for
the flat card, versus 0.4533 m for Shell. The card therefore also needs a separate
depth/occlusion solution; its colour silhouette alone does not provide one.

For this group, the live card is larger and visibly worse than existing Shell;
the cheaper static matte fails changing light. Neither is recommended for native
integration. This does not reject cards for all far scenery, larger groups or
smaller projected domains. No resolution sweep or new native pipeline follows
this failed control. A useful next candidate must beat the existing Shell in
both its intended projected/reuse domain and complete resident/work costs.
Continuous near/far, round-trip camera rotation, shadows, material/world and
target-device evidence remain open under #12.

## Reproduction

Use `.venv` with NumPy, Trimesh, Rtree and Pillow and the preserved original native
Fine/Shell controls. Outputs must be fresh explicit directories under system temp.
Pairs can only join when native anchors match; do not infer identities from `(0,0)`.

```sh
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p 'test_source_*.py'
.venv/bin/python -W error test/experiments/render/compare_source_billboard.py \
  --fine "$SOURCE_60/fine" "$SOURCE_80/fine" \
  --shell "$SOURCE_60/shell-4" "$SOURCE_80/shell-4" --output "$TEMP_VIEWS"
```

`report.json` records input/code hashes, memberships, cameras, suns, work, bytes,
RSS, timings and per-case errors. The local receipt linked from #12 records the
actual run, independent checks and inspected images. No diagnostic images are
published in README/doc. SIGGRAPH-first sources are
[Billboard Clouds (2003)](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf)
and [Layered Depth Images (1998)](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
whose first two pages including diagrams were visually inspected. A single
plane here is a bounded comparison, not an implementation of either full paper.

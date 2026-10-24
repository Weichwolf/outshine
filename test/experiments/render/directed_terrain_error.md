# Directed terrain displacement: point bound and depth counterexample

Issue: [#1243](https://github.com/Weichwolf/outshine/issues/1243), parent
[#515](https://github.com/Weichwolf/outshine/issues/515).
This is a Python candidate audit, with no native culler or metadata change.

## Directed bound

The existing [projective bound](terrain_projection_bound.md) encloses an
isotropic world displacement. Suppose instead that a qualified residual has
`deltaP = up*t + rest`, with `abs(t) <= H` and `length(rest) <= R`.
The transformed `up` need not be unit length. For fine homogeneous coordinate
`w > 0`, fine NDC coordinate `n`, spatial clip rows `rXY,rW` and pixel scaling
`D = diag(width/2,height/2)`, the exact displacement is

```text
deltaPixel = D*(rXY - n*rW)*deltaP / (w + rW*deltaP).
Tup       = max_rectangle_corners length(D*(rXY - n*rW)*up)
Sigma     = max_rectangle_corners spectral_norm(D*(rXY - n*rW))
bound     = (H*Tup + R*Sigma) / (wMin - H*abs(rW*up) - R*length(rW)).
```

Both numerator norms are convex in `n`, so the four rectangle corners bound
their values throughout the rectangle. The triangle inequality bounds the
numerator; the denominator follows from the absolute projected displacement.
A nonpositive denominator or unqualified residual cannot admit coarsening.
The implementation retains the existing `64*float_epsilon` row-sum padding
in both scales, plus the existing CPU bounds expansion and isotropic shader
displacement margin in the control. This derivation is local mathematics,
not a claimed result from a paper or a Float32 shader/compiler proof.

Uniform sample spacing alone does not justify a pure vertical residual.
`TerrainPatchError.cpp` also bounds bilinear XY nonaffinity and spherical sag
in a transformed basis. A later native contract must retain these terms,
their basis lengths and rounding; no metadata is silently reinterpreted here.
The control deliberately uses affine planar XY with no spherical sag, a
stronger constructed condition than merely uniform spacing.

## Complete depth/contact control

The source has 9×9 vertices at `(column,-row,height)` metres. Four vertices
`height[2:4,2:4] = -1/32` form a flat valley; other heights are zero. Existing
8-cell terrain patterns have 128/72/40/32 top triangles for strides 1/2/4/8;
all retain the fine outer edge. Both scenes include the same 64 skirt faces.
An unchanged opaque cuboid has a 0.5×0.5 m footprint wholly inside the valley,
bottom `-1/32 m`, top `-1/64 m`, and the same 12 source faces. Its bottom rests
on the actual fine terrain; no positive offset conceals a contact defect.

The perspective eye is `(4,-4,20) m`, looking down with vertical FOV 60°.
The independent existing CPU raster computes both complete scenes at
**1280×720**, with perspective depth and original source-face ownership.
All triangles stay in front of the near plane. The half-pixel allowance,
camera, skirts and object remain identical. The diagnostic's lower row is
only a shared display crop of the full computed image.

| Stride | Top triangles | Isotropic bound px | Directed bound px | Measured common-refinement point error px |
|---|---:|---:|---:|---:|
| 1 | 128 | 0 | 0 | 0 |
| 2 | 72 | 0.507443 | 0.138689 | 0.087752 |
| 4 | 40 | 0.507443 | 0.138689 | 0.103257 |
| 8 | 32 | 1.014890 | 0.276805 | 0.137569 |

At the same 0.5 px point allowance the isotropic selector keeps stride 1;
the directed selector permits stride 8. Yet the coarser depth covers **all
256 previously visible object pixels**; object coverage drops to zero.
There are exactly 256 changed material-ID pixels. Common-refinement samples
check corresponding terrain-point displacement, not a whole-image error
bound. The analytic bound can hold while scene visibility changes: motion
along a view ray can have little projected motion and still cross an object.

This rejects **point-only admission** for this candidate. It establishes
neither a current native rendering bug nor general depth safety of the old
isotropic selector. It does not reject every directed representation.
Contact/occlusion consumers need an explicit additional criterion before
claiming an admissible work reduction. No bigger pose sweep, new native
buffer, weaker allowance or shifted object is justified by this audit.

## Work and metadata limits

The inspected native bounds record is three vec4s, 48 bytes. Adding one
vec4 per patch would add 16 bytes: hypothetically 4,161,280 bytes for
16,255 pages with 16 patches each, before CPU copies, staging or frames in
flight. Reading the existing instance basis instead needs binding, access
and lifetime agreement. Neither option is implemented or selected.
The existing Early and spectral experiments remain isotropic; early-forced
fine work and their own baselines do not establish this candidate's benefit.
Submitted triangle references are not shader invocations. This counterexample
makes no CPU/GPU speedup or A18/Iris budget claim, and needs no timing contest.

SIGGRAPH 2004 [Geometry Clipmaps](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
and the existing projection derivation were the initial local references;
their terrain representations do not supply this scene-depth admission.
No new paper, provider input, cache or device claim is introduced.

## Reproduce

Use the repository `.venv` and an explicit system-temp output directory:

```sh
PYTHONPATH=lib/src:test/experiments/render .venv/bin/python -B -W error -m unittest test_directed_terrain_error verify_terrain_projection_bound
PYTHONPATH=lib/src:test/experiments/render .venv/bin/python -B -W error test/experiments/render/directed_terrain_error.py --render --output /tmp/outshine-directed-terrain-audit
```

Five new tests check contact geometry, source-face/depth-mask loss, directed
versus measured point errors, independent general-perspective samples with
nonunit `up` and isotropic rest, and invalid-residual/denominator rejection.
Four existing spectral tests run alongside them. The explicit driver writes
JSON, both full depth/object-mask arrays, and a local diagnostic PNG. Ordinary
tests write no oracles or images. Diagnostic colours are material IDs, not a
PBR/device image. Open the PNG when reproducing the image conclusion.

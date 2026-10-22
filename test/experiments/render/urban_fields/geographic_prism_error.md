# Geographic prism-patch placement

Owner: [#1102](https://github.com/Weichwolf/outshine/issues/1102).
Native consumer: [#1129](https://github.com/Weichwolf/outshine/issues/1129).

## Correspondence

- Input: ready urban patch NPZ, original global Mercator cell keys and height
  palette. No source reacquisition or child mesh expansion; the reference stays unchanged.
- Reference: upright WGS84 prisms at their geographic centres. Ellipsoidal east/
  north metric and basis follow native `UrbanArea::LotAt`/`WorldPlacement` formulas.
  Shared `places.camera.ecef` supplies positions; batched bases are checked against
  the shared camera convention. Heights follow the current engine coordinate
  convention; no orthometric/geoid conversion or actual DEM sampling is claimed.
- Candidate: intrinsic rectangular patches, local tangent basis, separate physical
  east/north pitch at a declared reference height. Every cell retains its height.
  Its lower coordinate is the exact centre projected onto the patch's up axis;
  upper=lower+recipe height. A raw ASL value is not that local lower coordinate.
- Patch frames are geographical, independent of camera direction. Reference
  arithmetic uses ENU; native X=east/Y=up/Z=south is a proper rotation. Longitudes
  wrap for ECEF while unwrapped Mercator cells retain their contiguous grid.
- Compare all eight corresponding vertices of every occupied prism. In ideal
  arithmetic the affine correspondence of two convex prisms bounds intermediate
  points by its vertex maximum. For a fixed reference frame it is affine in
  declared ground height too, so interval endpoints bound intermediate heights.
  Numerical Double results are a study, not an interval-certified culling/SSE bound.

## Complete retained field

247731 prisms,624 rows×616 columns; 1981848 corner pairs per case,20 cases.
| Patch side | Occupied patches | Height0, ref0: max m | Heights0..2000, ref1000: max m |
|---:|---:|---:|---:|
| 8 | 5962 | 0.001954 | 0.023842 |
| 32 | 400 | 0.022356 | 0.108699 |
| 128 | 25 | 0.316794 | 0.630516 |
| 1024 | 1 | 7.118199 | 8.443974 |

The whole-area frame also reaches21.46m error at height9000 with reference0.
Correcting the reference height helps but does not remove horizontal projection
variation. Refine the frame only when projected need requires the smaller error.
These are synthetic declared-height
stress cases on the retained distribution, not observed terrain/world scenarios.
No frame size is a universal implementation constant or a runtime admission.

## Reproduction and next integration

```sh
study=$(mktemp -d "${TMPDIR:-/tmp}/outshine-prism-placement-XXXXXX")
python test/experiments/render/urban_fields/geographic_prism_error.py \
  --input product.npz --output "$study/geometry.json"
```

Only NumPy and the existing reference library/reader are used. The script validates
its ready input, retains its SHA256 and writes to a fresh system-temp file. Tests
cover the shared basis, a single exact cell, wrapped longitudes and height intervals.

World preparation must supply native lower/upper samples and frames from published
terrain, retain provenance and carry the selected approximation error/bounds.
Batch patches and packed samples; avoid a draw or texture per patch. Known-frame
samples belong to ready assets; a camera turn does not rebuild them. Frame size
follows actual geometry/height bounds and projected need. Float32 packing, datum/
terrain correctness, native parity, PBR/Depth/Caster, temporal transitions, loading
and complete-device p99 remain open. This does not authorize a smaller LOD error.

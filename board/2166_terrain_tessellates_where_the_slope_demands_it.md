Type: feature
State: active
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: world, render
Tags: terrain, geometry, measured

# Terrain refines the final surface, including cliff faces

## Defect and boundary

The 33×33 GPU lattice draws a heightfield selected by RefineTerrain against
resampled source DEM, before earthworks and final stitching. A slope material
changes the BRDF, not the side silhouette. Malcesine still has a regular cliff
curtain; Koerbersee has inflated, smooth rock; Feldkirch has an abrupt near wall.
The shader work in 2255 cannot close this geometric defect. A heightfield also
cannot represent overhangs or multiple heights at one map coordinate.

Malcesine's camera ray at bearing 290° rises from 64.26 m at 4.1 km to 385.54 m
at 4.2 km in the resident z13 DEM; the final page agrees within 1 m at both ends.
Thus the large wall is sourced, not created by earthworks or stitching. The
global 183.594 m LOD-seam maximum lies outside that camera view. Suppressing
skirts changed only 99/921600 pixels and did not remove the curtain. Preserve
the real rise; diagnose regular folds and missing plausible relief separately.

Koerbersee at 81aa5fe78 has 5272 resident/drawn pages, all virtual. At 33×33
nodes and 32×32×2 interior triangles per page, that is 5,741,208 lattice
vertices and 10,797,056 interior triangles before culling, plus skirts.
Direct CPU product peak is 962,996,324 bytes. Its largest virtual seam was
169.552 m before the coarse-edge stitch and zero after it; the location must be
projected into the shot before attributing any visible defect to that seam.
The measured shot had sim p99 12.67 ms and worst 318.54 ms. Global lattice
density is therefore the wrong first knob; spend geometry where projected
surface error and visible contribution justify it.

## Decision

1. Add a read-only probe for selected visible patches: source zoom/posting,
   final page heights after stamps/stitch, normals, spacing, projected bounds,
   patch/skirt identity and source-versus-final error. Compare the same
   geodetic profile and rendered pixels in Malcesine, Koerbersee and Feldkirch.
   Do not equate global seam maxima or triangle counts with a visible defect.
2. Build or locally invalidate the error hierarchy against the final surface
   after earthworks, including virtual and distant patches. Preserve the
   unpressed source reference separately. Report error against resident DEM,
   final stamped field and source resolution as different quantities.
3. Select by conservative projected geometric error, not distance or slope
   alone. Use full view/clip bounds for near and oblique walls; e_m*focal_px/d
   is only a far-field approximation. A steep plane has zero interpolation
   error. A ridge, offset peak, narrow gully or later stamp must refine when
   their final geometry exceeds the declared 1-pixel target. If source or
   resource limits prevent this, report the unmet error.
4. Refine along and across side faces. Keep neighboring edges compatible;
   avoid cracks, T-junctions and geometry popping during LOD transitions.
   Where a heightfield cannot express necessary local relief, generate bounded
   native 3D rock patches with closed contacts and world-space continuous
   displacement. Geology is plausible synthesis from available data, not a
   claim to reconstruct actual cracks. Materials stay in the shared PBR path.
5. Bound CPU jobs, GPU uploads, visible triangles and resident bytes. Do not
   double the 33×33 lattice globally. Instrument cold load, warm camera motion,
   p50/p95/p99/worst frames, memory peak, and stable image transitions.

## DEM-conditioned procedural formation experiment

DEM remains the large-scale height authority; its source samples, drainage,
water levels and OSM construction corridors are constraints. A deterministic
formation field supplies only missing meso-/micro-scale geometry: choose a
small reusable family by slope, curvature, climate/landcover and available
geology; orient ridges with terrain structure, blend in world coordinates and
cache by source revision plus seed. Never invent a hill across a road, water
surface or building pad. Publish render and contact from the same final field.
Compare pure DEM, bounded formation blend and multi-scale erosion against
independent fine DEM where available; hold coarse samples and cross-tile seams
fixed. Reject repeated motifs, drainage reversals, loss of road clearance and
unbounded job cost. Frontier describes Odyssey's hierarchical selection,
orientation and blending of reusable forms; Starfield publicly documents
rule-based world composition, not an equivalent terrain algorithm. Adobe's
SIGGRAPH 2024 erosion amplification is a quality oracle, not a presumed
real-time implementation. References:
https://store.steampowered.com/news/posts/?appids=359320&enddate=1617185871&feed=steam_community_announcements ;
https://research.adobe.com/publication/terrain-amplification-using-multi-scale-erosion/ ;
https://news.xbox.com/en-us/2024/02/28/how-starfield-filled-its-galaxy-with-alien-life/ .

## Acceptance

- Analytic steep plane needs no extra tessellation; off-grid peak, narrow ridge,
  oblique relief wall and post-refinement stamp do. A deliberately infinite
  tolerance must fail the oracle. Final errors remain bounded at seams.
- Malcesine retains its real steep rise but loses repetitive curtain folds and
  teeth. Koerbersee has readable rock ridges/gullies; Feldkirch has no invented
  near wall. Open PNGs at two nearby cameras and one distant view; compare
  normals, silhouettes and LOD motion against saved references.
- Report the 1-pixel target separately against final resident geometry and
  source resolution, along with triangle/byte/frame budgets. WI 2092 tracks
  full-frame cost; 2144 covers shared edges, 2124 upload pacing, 2171 material
  plausibility. Use portable SDL_GPU geometry, no assumed hardware tessellation.

# Buildings — the OSM prism pass

**Pass:** `BuildingsStage` (`sim/src/render/stages/BuildingsStage.{h,cpp}`).
Neighbours: [`../classification.md`](../classification.md) (the building branch — geo-coordinate, base
albedo, epoch, decay — that must decide what is drawn here), [`shadow.md`](shadow.md) (whose casters
**are** this mesh), [`terrain.md`](terrain.md) (the ground the prisms stand on),
[`../lod.md`](../lod.md) (the ladder that must replace the single mesh), and
[`../renderer.md`](../renderer.md) (the pass topology). The `architect` agent holds the acceptance rules
for a generated building.

## Spec

| Contract | Why |
|---|---|
| **ONE vertex buffer, one draw, one pipeline** — the mesh is static once built, so the only per-frame traffic is the camera-relative anchor in the uniform | [`../visual-target.md`](../visual-target.md) §1: bandwidth is the shortage, upload is traffic |
| the vertex uv is **(metres along the wall, metres above the base)**, not 0…1 | the storey lines drawn today and the window grid a later pass draws are then the *same two numbers at two levels of detail*, and neither needs the geometry rebuilt |
| the shadow pass **borrows** this vertex buffer | never a second copy of the geometry ([`shadow.md`](shadow.md)) |
| what is drawn comes from a **building type**, not from a footprint alone | [`../classification.md`](../classification.md)'s building branch: geo-coordinate → tradition, base albedo → use, epoch → construction, decay → condition |
| the silhouette is the first thing to get right | the `architect` names a band of equal-height flat roofs as a defect; roof landscape *is* the silhouette of a German Altstadt |
| the LOD ladder is `sse_px` on facade band → storey → footprint | [`../lod.md`](../lod.md), which judges the cluster DAG to be the right mechanism for exactly this pass |

## State

**Built as prisms, and that is all.** `BuildingsStage` exists in the working tree and is **uncommitted**
(`git status` reports `BuildingsStage.{h,cpp}` untracked at the time of this split; the round that builds
it is running concurrently). One vertex buffer, one draw, one albedo for every building, storey lines in
the shader from the metre-based uv.

No building type, no region, no epoch, no decay, no roof forms, no LOD ladder. The height that feeds the
extrusion is a default on 95.8 % of the reference town — measurement in
[`../classification.md`](../classification.md) `## State`.

**The commit anchor and the frame measurement are owed by the concurrent round** and are deliberately not
guessed here.

## Gaps

- **OSM buildings ARE available for every theatre — the work is extrusion, not acquisition.** Measured
  2026-08-06 against a running `fb-tiles`, `/t/vector/z/x/y`:

  | | z13 | **z14** |
  |---|---|---|
  | Payerne (control) | 14 314 B — `streets` `land` `water` | **15 410 B — `buildings` `streets` `land` `water` `street_polygons`** |
  | Sindh | 37 172 B, no buildings | **32 711 B — `buildings` `streets`** |
  | z15 / z16 anywhere | — | `no such route` (`FB_TILE_VECTOR.maxz = 14`) |

  z13 carries no `buildings` layer; **z14 does, everywhere**, and `tiles/src/raster.c` and `lights.c`
  already read it. Nothing is missing but the extrusion itself.

  **Two cold-cache traps, and the wrong conclusion was recorded off each of them before the measurement
  was finished:** a first request returns 9 B and a later one the real tile — the same behaviour the DEM
  has. An earlier note read „northern Thailand has no OSM objects" off that artefact, and a revision
  called z14 a blocker without having tested z14. **Fetch twice, then conclude.**
- **A prism is not a building.** No roof form, no facade articulation, no material distinction — the
  `architect`'s defect list (uniform albedo, flat-roof band, uniform decay) describes today's output
  line by line.
- **Nothing reads a building type**, because none is produced ([`../classification.md`](../classification.md)).
- **No LOD ladder.** One static mesh at one detail; [`../lod.md`](../lod.md) names the cluster DAG as the
  mechanism for this pass and nothing implements it.
- **Interiors are unaddressed here.** The engine owns them by
  [`../../../CLAUDE.md`](../../../CLAUDE.md)'s boundary; no pass draws one and no format declares one.

## Knowledge

Nothing is derived here yet. The height measurement the extrusion rests on is in
[`../classification.md`](../classification.md) `## State` with its source; the vector-tile availability
measurement is in `## Gaps` above with the endpoint it was taken against.

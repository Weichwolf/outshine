# Existing-cell ground-floor entrances

`buildings.interiors.entrances.entrance(polygon, rooms, context)` is the former
`building_plan.entrance` selector, now shared with the actual city envelope path.
The old owner/backing/aperture/landing dimensions and default candidate/ranking
arithmetic remain unchanged. `building_layout.rectangle` and
`building_plan.owner/aperture_fits/entrance` delegate to the library. The legacy
default selector was replayed exactly against its previous implementation on
analytic/real footprints and with/without frontage evidence.

Optional `fractions_by_edge` supplies immutable tuples of existing exterior-edge
positions. Missing keys have no candidates. No new facade grid is invented.
`accept(door)` may reject a candidate for caller-owned constraints. Edge identity,
fraction and candidate limits are checked before candidate geometry: at most256
exterior edges,32 fractions per edge, configurable total capacity1..4096,
default1024. No global optimum or arbitrary room-programme solution is claimed.
Room polygons and source/frame ownership remain the caller's.

`buildings.opening_panels.OpeningPanel(width_m, height_m, opening, depth_m=.24)`
defines one finite wall slab with a rectangular through opening. Local axes are
right-handed `(facade tangent U, up V, outward W)`, front at0, back at−depth.
The immutable opening `(u0,v0,u1,v1)` leaves positive margins. `panel_surface`
emits32 triangles: four trapezoid panels on each face and four outer/inner side
quads. Corner-to-corner seams avoid T-junctions. Normals, metric planar UVs,
front/back/outer/reveal roles and caller-owned arrays accompany the boundary.
Capacity is checked before expansion. This primitive chooses no door, room,
material, collision policy or visibility.

The city experiment uses the existing facade cells and the same entrance
selector on a provisional connected, wall-inset ground-floor arrival space.
Existing complete room controls retain their earlier plans. Elevated, clipped,
special, too-low, obstructed or unsupported sources retain their previous
exterior and report why. Candidates require a contained cell slab, casing
margin, inward arrival pocket and known-neighbor-free landing. These are
procedural assumptions; no measured OSM entrance or complete room programme.

Exactly one ground-row cell changes kind. All other opening axes stay in place.
Where real recesses are demanded, bounded repeat subranges exclude that cell;
otherwise at most three planar rectangles preserve the existing repeat UVs.
The near cell uses the through slab, existing U-shaped casing and
`DoorLeaf`/local leaf template, plus simple arrival floor/ceiling and landing.
The leaf hinge uses the actual0.24m inner wall face. Angle is separate render
state; source plan IDs/centres/cells do not change when opening. The source
height interval and footprint are retained; the outside landing is an explicit
procedural extension. The plinth on the entrance facade is omitted rather than
blocking the aperture; fine partial-edge trim interruption remains future work.

Demand is determined from metadata before mesh-buffer reads. The experiment
uses a compact opaque entrance cell below6px projected door width, actual
through geometry above that, and the prior facade below0.5px projected height.
These are experimental screen-size choices, not radiance/shadow/transition
guarantees or native frame limits. Semantic entrance records remain present.
Near/compact/far selection within this simple plan preserves IDs and anchors.
Transition into the older complete-room control planner, upper routes, external
transport/terrain contacts, native player interaction and persistent native asset
integration remain unproved. The new path is not a second complete room planner.

Independent checks cover closed oriented panel volume, real through rays and
sealed counterparts, planar metric UVs, owned buffers and capacity; old selector
and actual shared first-storey controls remain regression checks. Actual source
Paris/Rome cases preserve state-independent IDs/cells/source properties, local
neighbor exclusion and the free entry plinth. A continuous0.30m torso tube and
0.10m support tube are tested against actual compiled prismatic assembly
sections on every critical height interval; round buffers conservatively
circumscribe the requested radii. Closed leaf and missing landing fail. This
avoids ambiguous vertical rays exactly grazing an entrance wall, without
changing the old structural checker. Only the straight arrival is proved.

`entrance_probe.py --output <temp>` compares the direct panel with the existing
prism-union/difference backend at identical solid geometry. It writes real
plan/section/2.5D plus before/closed/open/side/overview/compact geometry reports
for the shared Blender renderer. The2.5D diagnostic explicitly hides arrival
floor/ceiling; full meshes receive the independent clearance check. Main research
images remain local. Older recess/band probes suppress this later feature to
retain their isolated comparisons.

Local primary references: SIGGRAPH2006 *Procedural Modeling of Buildings*,
page3/Figure4; SIGGRAPH2003 *Instant Architecture*, page2/Figures2–3; Candler
`GroundSubTile/GroundfloorOpening` uses negative extrusion and separate door/
shop-front treatments. Existing pinned Ghidra extrusion dispatcher evidence
supports that primitive boundary, not this full selector or an Esri door oracle.
No foreign code, geometry, textures or packaged assets are copied.

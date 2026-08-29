Type: feature
State: open
Parent: 1953
Area: generators, render
Tags: benchmark, measured

# A building carries the detail that COVERS A PIXEL, and every threshold is derived from the lens

**Benchmark** — Unreal: 4 to 8 static-mesh LODs with screen-SIZE transitions, HLOD clusters merged over them, and with Nanite no discrete levels at all -- a cluster cut at a constant screen error. RAGE: High/Med/Low/Vlow inside a drawable, then `_lod` and SLOD1..4 of MERGED sectors. **Taking both**: thresholds from projected size like Unreal, and an aggregation tier above the per-building levels like RAGE's SLOD -- because they agree that a distance in metres is not the criterion, and they agree that the far field stops being individual buildings.

## The optics, which is where every number here comes from

A feature of `h` metres at `d` metres covers

    px = focalPx * h / d,    focalPx = H / (2 tan(fov/2))

720 rows over 55 deg gives focalPx = 691.5. Below TWO pixels a feature cannot be
resolved and does not merely waste vertices, it ALIASES -- so two pixels is where a level stops
paying. One pixel is where the building itself stops being worth drawing at all.

| what a level adds | size | holds 2 px out to |
|---|---|---|
| roof SHAPE, the rise | 3.0 m | 1037 m |
| footprint corners | 2.0 m | 692 m |
| cornice, plinth, bays | 0.3 m | 104 m |
| the building at all (1 px) | its height h | 691.5 h |

## TWO AXES, NOT ONE -- and this is why three levels are not enough

Footprint fidelity dies at 692 m and roof shape not until 1037 m. So a FLAT roof over a TRUE
footprint -- which is exactly what `Prism` builds -- has no band in which it is the right trade, and
the level that belongs in the gap is its MIRROR: a shaped roof over a hull footprint. Not built.

**A LEVEL IS BOUNDED BY THE SILHOUETTE IT ADDS, not by the smallest ornament riding on it.** Getting
that wrong cost Rothenburg every roof in the town for one measurement: gating architecture on the
0.30 m cornice put the whole level's reach at 104 m and `raised with full architecture` read 0.

## What stands now, measured

`kArchitectureReachM = 1200.0` -- one constant in metres, answering the same for a 6 m shed and a
100 m tower, which cover 1 and 17 pixels at the same distance -- is replaced by the derivation above.
Rothenburg 114 MB of vertex soup before, 23.7 MB after. Central Park went from 1810 MB and no
picture at all to a frame the owner has looked at.

The box is the oriented MINIMUM-AREA rectangle rather than an axis-aligned one, because a building
at 40 deg to the grid would otherwise gain a silhouette half again its own width, and the silhouette
is the only thing that level still carries.

## What is NOT built, and what it is worth

- **The middle level**: shaped roof over a hull footprint, for 692..1037 m
- **The ornament level**: architecture without cornice or bays, for 104..692 m
- **AGGREGATION**, which is RAGE's SLOD2+ and Unreal's HLOD: beyond some distance the far field stops
  being individual buildings and becomes one merged mesh per cell. Both engines BAKE these offline.
  Here the vertex saving is smaller than it looks -- `BuildingField::Verts_` is already ONE soup, so
  there are no per-building draw calls to save -- and what aggregation actually buys is CULL
  GRANULARITY and the removal of interior faces. Cull granularity is the cluster DAG, so this lands
  with board:1992/1993 rather than beside them

Neighbour, not duplicate: board:2026 owns that `Verts_` is never trimmed, which is who OWNS the
geometry. This item owns how much detail is in it.

## What would show this wrong

A place whose picture is visibly poorer at the same vertex count, or a level whose threshold is
crossed with no visible change -- which would say the feature size it was derived from is not the
feature the eye is reading.

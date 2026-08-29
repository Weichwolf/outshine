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

## PARKED, and the measurement that parks it

Central Park draws in 47.6 ms against a 16 ms frame. Where it goes, measured:

    of 2 frames, advance 4 ms and render 91 ms          -> 2 ms CPU, 45.5 ms render
    5401580 triangles, of which 4906106 are buildings   -> 91 per cent
    5401580 / 921600 pixels                             -> 5.86 triangles per PIXEL
    cull: the whole index list they cut from   653400 indices
    cull: against the list the CPU selected    653400 indices

The cut selects everything, and those 100 clusters are the TERRAIN -- the buildings
are a separate soup drawn whole and never enter the cluster route at all. That is board:1995
verbatim, and no level of detail inside this item can answer it: the box level works as designed at
12 triangles a part, and the cost is that there are 272295 of them.

**THE AGGREGATION TIER IS WITHDRAWN FROM THIS ITEM.** Worked through: a carpet of 2 px cells over a
z14 tile at the aggregation distance costs about 55000 triangles against about 60000 for that tile's
own buildings -- no win. What RAGE's SLOD actually is, is a BAKED decimated merge, and that is the
thing Unreal replaced with Nanite because a DAG cut does it better at runtime and per cluster.
Building it now would be building what the DAG deletes. The remaining levels here -- the middle one
and the ornament one -- are cheap and stand; the far field belongs to board:1992/1993/1995.

## MEASURED WITH THE PIXEL BOUND IN, and five of six hold the owner's frame

    place          triangles    ms/frame   varies by
    Rothenburg       592 804        0.90       1.951
    Heidelberg     1 360 726        1.01       2.286
    Venice         1 541 982        1.39       1.789
    Jura             759 099        1.61       1.866
    Central Park   4 393 020       15.69       1.161
    Shibuya        9 428 074       41.42       1.949

Central Park went 47.6 -> 15.69 ms on the triangle-fits-the-pixels bound alone, and its picture is
Manhattan's skyline over the park. Shibuya is the one place over 16 ms, at 9.4 M triangles.

## Two weaknesses in the instruments, stated rather than left to be found

**The blank-frame guard's negative control has NOT been re-measured.** The 0.000 readings that
justify the bar of 1.0 were taken over the LOWER HALF of the frame; the measure now runs over the
whole frame, because Central Park read 0.719 across a real skyline -- the camera looks over a park
and every building stands ABOVE the horizon. The bar has not been re-derived against a bare frame
under the new measure, and Central Park clears it by 16 per cent. Thin, and unproven at the bottom.

**A frame time from two frames is not a frame time.** The cases average over exactly the frames they
draw and the first carries every warm-up cost: two consecutive runs of the same tree gave Central
Park 15.69 and 37.79 ms and Shibuya 41.42 and 33.49. Any claim about 16 ms rests on a number with
that much spread in it, so the settle count has to become a real frame count before this item can
say it holds.

## What would show this wrong

A place whose picture is visibly poorer at the same vertex count, or a level whose threshold is
crossed with no visible change -- which would say the feature size it was derived from is not the
feature the eye is reading.

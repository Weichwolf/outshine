Type: feature
State: active
Area: world, render
Tags: look, measured
Depends: 2123

# Terrain tessellates where the SLOPE demands it, not where the distance says

**Benchmark** -- Unreal: the Landscape is a quadtree whose LOD is chosen by SCREEN-SPACE ERROR,
and 5.x carries displaced landscape through Nanite, where the cluster is refined until its
projected error falls under a pixel. RAGE: an authored heightfield in LOD rings, with the artist
placing extra density where a cliff needs it. **Both agree on the criterion and differ only in who
applies it**: the error a cell makes ON SCREEN decides, never its distance alone. outshine has no
author, so the criterion has to be computed -- and a distance ring cannot see that a 40-degree
slope makes several times the error of a flat field at the same range.

## Where it stands, measured 2026-09-07 against the webcam corpus

| place | what the photograph has | what the frame has |
|---|---|---|
| Koerbersee | jagged rock ridges with real relief | rounded lumps; the silhouette family is right and every edge is smoothed away |
| Malcesine | a cliff wall standing out of the lake | a smooth ramp, and a vertical saw-tooth comb where the water meets it |
| Feldkirch | a wooded hill flank | a smooth brown mass carrying a hole |

The lattice refines by distance, so a flat field near the camera gets the density a cliff at the
same range needs, and the cliff gets a flat field's.

## The solution

The refinement criterion takes the cell's own geometry: refine while the projected geometric error
-- the sagitta of the cell against its children, in pixels -- exceeds one. Slope and curvature fall
out of that rather than being special cases, and a flat cell stops refining early on its own.

## The goal

### Implementation experiment, 2026-09-07

The current path draws 32 x 32 cells per DEM tile, then adds four fixed virtual rings.
The first repair uses the detail already held by each source field: a nested, uniformly
sampled reference surface and a quadtree of 32-cell patches. Each patch compares its
triangles against ALL reference vertices underneath it, not just its centre; parent error
also includes every child error. Project with the actual camera focal length and distance
to the patch bounds, and split above 1 px. Near rings remain the minimum sampling needed
by the existing ground stamps. Source-data LOD and moving-world scheduling are separate
from this tessellation error against the resident DEM.

Reference choice: Cesium's projected geometric error, with a shared instanced grid as in
Landscape/CDLOD. RAGE's authored density cannot choose the detail of fetched data here.
The existing height pages, source fields and lattice renderer are reused. Proof: a plane
(including a steep plane) has zero error; an off-centre peak missed by the coarse grid has
nonzero error; finer triangles reach zero at the reference resolution; a narrower field
increases the projected error. Infinite tolerance is the negative control.

Visual expectation: Koerbersee's crests and gullies gain geometry where the source field
already holds it. The shader's normals are unchanged in this experiment, so a surviving
soft bank does not count as a tessellation repair. Preserve the nine input PNGs under
`build/terrain-before/`, open the changed renders, and check p99 against 16.7 ms.
The unchanged Koerbersee baseline reproduced digest 12552f22, p99 2.56 ms, 0/120 over.

### What is actually being asked for, and the correction the title needs

**A slope alone demands nothing.** A plane at forty degrees is exactly represented by two triangles,
and refining it adds vertices that carry no information. What demands tessellation is where the
slope CHANGES -- a crest, a cliff top, a gully, a break line, a terrace edge -- because that is
where a flat interpolation departs from the ground. The quantity is CURVATURE, and the title's
"slope" is shorthand for it. Everything below follows from that one substitution.

### The criterion: screen-space error, and it is not a preference

A cell approximates the ground with some vertical deviation d_e metres -- the SAGITTA between the
cell's own surface and the finest data under it. Seen from distance r that deviation subtends

    theta  =  2 * atan(d_e / 2r)  ~=  d_e / r          [radians]

and the frame maps a vertical field phi onto H pixels, so

    pixels per radian  =  H / (2 * tan(phi/2))

which gives the SCREEN-SPACE ERROR in pixels

    rho  =  (d_e / r) * H / (2 * tan(phi/2))

**Refine while rho > tau.** With tau = 1 px, a cell is finished when

    d_e  <=  r * 2 * tan(phi/2) / H

This is Hoppe's metric, Cesium's `geometricError` -> screen-space conversion, and the rule Unreal's
Landscape and Nanite both reduce to. It is the same formula written three ways in three readable
bodies, so it is not a decision this tree gets to make -- only one it has to apply.

### What that is, in numbers, for this tree's own frames

Derived from the nine surveyed cameras at 1280 x 720, tau = 1 px:

| place | vertical field | px / rad | d_e budget at 100 m | at 1 km | at 10 km |
|---|---|---|---|---|---|
| Olympiaturm | 13.06 deg | 3145 | 0.032 m | 0.32 m | 3.2 m |
| Graz | 26.23 deg | 1545 | 0.065 m | 0.65 m | 6.5 m |
| Rosenheim | 30.68 deg | 1312 | 0.076 m | 0.76 m | 7.6 m |
| Koerbersee | 33.97 deg | 1179 | 0.085 m | 0.85 m | 8.5 m |
| Wien | 38.04 deg | 1044 | 0.096 m | 0.96 m | 9.6 m |
| Feldkirch | 80.72 deg | 424 | 0.236 m | 2.36 m | 23.6 m |

The field of view is in the criterion, which is why a distance ring cannot carry it: Olympiaturm's
telephoto demands **seven times** the detail of Feldkirch's wide angle at the same range, from the
same ground.

### Where the curvature enters, in closed form

For a cell of width w over ground whose worst-direction second derivative is kappa = 1/R, the
linear interpolation error is the sagitta

    d_e  =  kappa * w^2 / 8

Setting that equal to the budget above gives the width a cell may have:

    w  =  sqrt( 8 * R * r * 2 * tan(phi/2) / H )

**Cell width scales as sqrt(R * r).** A flat field has R -> infinity, w -> infinity, and stops
refining on its own -- no special case, no slope test, no rule about cliffs. Worked at Koerbersee,
phi = 33.97 deg, a crest 2 km away:

| crest radius R | d_e budget | cell width w |
|---|---|---|
| 50 m (a sharp arete) | 1.70 m | 26.1 m |
| 100 m | 1.70 m | 36.8 m |
| 200 m (a rounded shoulder) | 1.70 m | 52.1 m |

**And that number is the item's sharpest claim.** 26 m at the sharp crest is about the posting of a
30 m DEM. The criterion is therefore asking for exactly what the data already holds and no more --
so the rounded lumps in today's Koerbersee frame are the lattice refining TOO LITTLE, not the DEM
being too coarse. If the measurement comes back the other way, the item is wrong and the source's
ground sample distance is the real item.

### Two conditions the scheme has to satisfy

1. **The error must be NESTED and monotonic** -- a parent's d_e is at least the maximum of its
   children's. Without it a cell can be accepted while a descendant is still wrong, and the
   refinement is no longer a bound on anything. This is Lindstrom and Pascucci's condition and it
   is what makes an out-of-core terrain hierarchy correct rather than merely fast
2. **The transition must be crack-free WITHOUT a skirt.** A skirt is a vertical curtain hiding a
   T-junction, and it shows the moment the camera looks ALONG a slope -- which is what a webcam on
   a valley flank does all day. The answer is a vertex morph across the level boundary (CDLOD), so
   a cell's fine vertices slide onto their coarse positions as the error approaches the bound and
   the two levels meet at identical vertices

### The bodies this is taken from

| source | what it contributes | readable |
|---|---|---|
| **Hoppe 1998**, *Smooth view-dependent LOD control ... terrain* | the screen-space error metric itself, and the incremental refinement around it | paper |
| **Lindstrom & Pascucci 2002**, *Terrain simplification simplified* | the nested, monotonic error and out-of-core indexing | paper |
| **Duchaineau et al. 1997**, ROAM | the split/merge priority queue driven by that error, and the silhouette boost | paper |
| **Ulrich 2002**, chunked LOD · **Losasso & Hoppe 2004**, geometry clipmaps | the two shapes a terrain hierarchy can take: irregular chunks against a regular clipmap | papers |
| **Strugar 2009**, CDLOD | the morph that removes the crack without a skirt | paper |
| **Cesium** | `geometricError` per tile and its conversion to screen-space error, on a real planet with real DEM tiles -- CLAUDE.md cites Cesium for anything that streams a planet, and this is that | Apache 2.0, read |
| **Unreal** | Landscape LOD by screen size, and Nanite refining a cluster until its projected error is sub-pixel | source |

**The choice:** Cesium's per-tile geometric error converted by the formula above, made monotonic
the way Lindstrom and Pascucci require, and joined across levels by CDLOD's morph. RAGE has no
answer here at all -- its terrain is authored and an artist puts the density where the cliff is --
so this is a place where only one of the two references faces the question, and CLAUDE.md says the
item has to say so. It says so.

### What the first look actually found, 2026-09-07 -- and it is NOT the triangle count

Husum and Malcesine read worst of the nine, and at 4x both show ONE signature: a near-vertical drop
is shaded as a soft ramp. Husum's quay is a stack of broad terraces with gradients down their
faces; Malcesine's shoreline is a row of triangular teeth, each shaded dark at its point and light
at its base, all ending on a clean straight waterline. **Soft gradients over a step mean the
NORMAL, not the density.** More triangles would give more teeth, smaller.

`src/render/shaders/groundLatticeCore.msl:57-61` says why:

```
  dhE = (h[i+1,j] - h[i-1,j]) / (2 * stepE)
  dhN = (h[i,j-1] - h[i,j+1]) / (2 * stepN)
  p.normal = normalize(float3(-dhE, -dhN, 1.0))
```

A central difference is a LOW-PASS FILTER with a two-cell footprint. Over a break line it averages
ACROSS the break, so a fifteen-metre quay wall is smeared over two cells and every vertex near it
tilts toward the drop; on adjacent columns the difference flips sign, which is the comb.

**So this item has a cheap first step that is falsifiable in one render**: at a break, take a
ONE-SIDED difference -- the side with the smaller |dh| -- instead of averaging over it. If the wall
then stands, the smoothing was the defect and the error bound above is a second, separate gain. If
it still reads as a ramp, the resolution is the defect and the ladder is the whole answer.

**And a bound both references share, worth stating before the work**: a heightfield holds ONE z per
(x, y) and cannot carry a vertical or overhanging face at any density. Unreal answers cliffs with
MESHES rather than with the Landscape; Cesium's quantized-mesh has the same soft edge. For outshine
that means a break line -- `man_made=quay`, `natural=cliff`, `barrier=retaining_wall` -- is
GENERATED GEOMETRY along what OSM already holds, and no amount of refinement substitutes for it.

### What I will accept as GOOD, by looking

The numbers above can all be right while the picture is still wrong, so the acceptance is visual
and it is mine. Against the nine photographs in `build/shots/webcam/`, at 4x:

- **Koerbersee** -- the crest between the two peaks left of centre reads as an EDGE against the sky.
  Today it is a curve. The gullies on the right flank are present as form, not as colour
- **Malcesine** -- the cliff top is a BREAK LINE: the wall stands, the ground above it lies flat,
  and the two meet in a corner rather than a fillet
- **Feldkirch** -- the hill flank right of centre carries the notch the photograph has in its
  skyline, and no hole
- **Rosenheim** -- the Alpine ridge at 25 km has the same count of layered notches as the
  photograph. At that range the silhouette is the entire content, so it is the whole test
- **All nine** -- no LOD seam like the hard horizontal band standing in today's Olympiaturm frame,
  no crack at a level boundary, and nudging the camera ten metres pops nothing

### And it stays inside the budget

The nine surveyed places stand today at p50 1.96 to 4.30 ms and p99 2.48 to 5.08 ms against
16.7 ms. The triangles are to be REDISTRIBUTED -- off the flat ground near the camera and onto the
crests -- so p99 may rise but no place crosses the budget, and `0 of 120 over` stays true for all
nine.

## What will be true

- [ ] A cell's refinement follows its projected error and the triangle budget at Koerbersee falls
      on the meadow and rises on the ridge
- [ ] Koerbersee's ridge silhouette carries edges a photograph can be laid against
- [ ] Negative control: hold the error bound at infinity and the ridges go back to lumps
- [ ] No place loses its frame budget: the nine surveyed places stay under 16.7 ms at p99

## What will show I was wrong

The error bound falls to one pixel and the ridges are still smooth. Then the DEM, not the lattice,
is what is missing the relief, and the source's ground sample distance is the item instead.

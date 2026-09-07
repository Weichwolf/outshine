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

### Source-data audit, 2026-09-07

The current working-state nine-place baseline is preserved separately in `build/goal-before/`;
`build/terrain-before/` still holds the original baseline. No image has been accepted as repaired.
A Paeth decoder suspicion was disproved: the existing upper-left indexing is correct, a 2x2
fixture preserves its known RGB bytes, and a deliberately wrong corner fails that fixture.
`PngPaethPreservesElevationBytes` keeps the byte-preservation proof.

Across 1153 cached tiles around Husum and Malcesine, the independent SDL_image comparison agrees
with this tree's decoder. One sampled tile carries off-earth values:
`https://s3.amazonaws.com/elevation-tiles-prod/terrarium/15/17203/10446.png` has 69 samples below
the admitted Earth range, minimum -20483.879 m. A fresh download matches the cached bytes.
Thus at least these invalid heights originate in the delivered data, not the decoder or cache.
This does not establish the cause of all visible teeth or justify the tile-wide median filter.
The sample inventory and independent comparison are under `build/dem-proof/`.

### Falsification

The error bound falls to one pixel and the ridges are still smooth. Then the DEM, not the lattice,
is what is missing the relief, and the source's ground sample distance is the item instead.


## Coastal DEM preservation and steep-face material, 2026-09-07

Previous turn progress: Spot squared falloff actual GPU negative/positive. Interrupted continuation made further progress and was resumed by user; no live gates now. User additionally requires Malcesine side-face tessellation and steep slopes to look like rock, not only refinement of the horizontal heightfield.

TerrainGrid tile median/MAD filter was proven destructive. Cached source Malcesine11/1084/731 (build/dem-proof/median-candidates.json): median61.08203125m, MAD floored1m,29,903/65,536 samples replaced (45.6%), valid connected relief up to1554.58203125m,0 off-Earth samples. Scientific source/filter/difference plot build/dem-proof/median-relief-loss.png opened; shows contiguous entire mountains erased because lake occupies majority. Raw encoding provider reference https://github.com/tilezen/joerd/blob/master/docs/formats.md . Removed FlattenOutliers and its45lines, retained separate FillOffEarth and physical Earth bounds. This does NOT validate all plausible-range source values: earlier real bad Husum samples and in-range corruption remain possible.

NEW TerrariumPreservesCoastalRelief.cpp independent16x4 PNG:11columns61m then5columns161..561m. Negative20/65assertfail (everyhillnode), after65pass; fullconventions9/9. Logs build/coastal-relief-before.log, coastal-relief-negative-assertions.log, coastal-relief-after.log. Terminal28575negativeexit2,86620positiveexit0. No original oracle changed.

Coastal-only twoPlaces session49044 was interrupted by user but resumed exacthandle and confirmedexit0: H58816690p993.44ms0/120, M5d793b86p995.72ms1/120over (draw worst20.45ms). Both fullPNGseen. Subsequent all9session34121exit0: build/coastal-relief-all-places.log, all9fullPNGs+all9 enlargedcrops build/coastal-relief-review seen. All p99<16.67 but Rosenheim1/120over, so NOT performanceacceptance. Graz hillreappears rather thanfloatingfragments; Husumdeeperharbor trenches appear; Malbluecurtains and Feldwallremain. Medianfilter notsolewallcause.

Coastal-only rates:
SHOT    DarmstadtWest              457e60fe  p50   2.64  p95   2.80  p99   2.92 ms  0 of 120 over 16.67, worst at 29  [sim p99 1.19 worst 1.20 | draw p99 2.65 worst 3.22]
SHOT    Wien                       7a60073d  p50   5.46  p95   5.72  p99   6.58 ms  0 of 120 over 16.67, worst at 108  [sim p99 2.11 worst 2.21 | draw p99 5.90 worst 6.09]
SHOT    Rosenheim                  6ae11c88  p50   3.44  p95   6.12  p99   7.37 ms  1 of 120 over 16.67, worst at 2  [sim p99 2.15 worst 2.16 | draw p99 6.96 worst 19.36]
SHOT    Husum                      58816690  p50   2.73  p95   2.96  p99   3.49 ms  0 of 120 over 16.67, worst at 50  [sim p99 1.19 worst 1.19 | draw p99 3.10 worst 3.19]
SHOT    Olympiaturm                a95d6751  p50   3.47  p95   3.90  p99   4.24 ms  0 of 120 over 16.67, worst at 7  [sim p99 1.64 worst 1.66 | draw p99 3.85 worst 4.07]
SHOT    Graz                       315b9483  p50   4.22  p95   4.84  p99   5.05 ms  0 of 120 over 16.67, worst at 60  [sim p99 1.44 worst 1.45 | draw p99 4.60 worst 4.74]
SHOT    Koerbersee                 b27e0222  p50   6.53  p95   7.06  p99   7.59 ms  0 of 120 over 16.67, worst at 96  [sim p99 3.24 worst 3.28 | draw p99 6.89 worst 7.04]
SHOT    Malcesine                  5d793b86  p50   4.10  p95   4.53  p99   4.70 ms  0 of 120 over 16.67, worst at 6  [sim p99 1.20 worst 1.22 | draw p99 4.43 worst 4.45]
SHOT    Feldkirch                  d4b49971  p50   4.67  p95   6.61  p99   7.77 ms  0 of 120 over 16.67, worst at 3  [sim p99 1.23 worst 1.49 | draw p99 7.45 worst 8.00]

Slope material wiring now implemented using EXISTING GroundMaterials slope.plausibleDeg max and AlpineLimit slopeBandDeg/RockTemplate. Engine::PaletteOver retains header4+RGBArows, packs rockindex/header1,band/header2 and appends one slope limit per row including fallback90deg. Classify obtains rowcount from header instead of palette byte length. groundClass.glsl groundWearsSlope blends each selected/runnerup material toward configured rock using smoothstep(limit,limit+band,slope). groundLit computes slope from interpolated world normal versus lights.up. Water2deg, forest35deg are existing config, no per-place colour hacks. Packed fourthcomponent is SPECULAR SCALE, NOT roughness; do not misinterpret. Actual material roughness/detail still unconsumed.

Slope twoPlaces terminal26975exit0: H d60b18a7 p994.17, M46e4db5c p997.71, both0/120. FullPNGsopened. Blue Malcurtains turnrockgrey, proving wrongslope material; horizontalwaterstaysblue. Husum bankturnslightrock, geometryteeth/rampnowevenmoreobvious. Noclaimrockappearancefinished: uniform smooth pleats, no sidewallrelief, cliffnormals/geometryremain.

Slope all9 terminal30107exit0 build/slope-material-all-places.log; all9fullPNGandall9detailPNGs build/slope-material-review OPENED. Against previouslyopened webcam references: Malfaces have plausiblebaseclassbutpleatedwallsnotrock; Feldblackwallnowgreyblue and stillgeomwrong; Grazhillnowrocksbut spatialcompositionstillmismatch; Koerbexcessuniformrockpatches; shorebandsunnatural; citiesuniform andskymismatch. This turn did not reopen all webcamfiles (previouscomparisonimagesavailable), no finalvisualacceptance. Malall9digesteda034e3 differs from2place46e4db5c (known nondeterminism remains toattribute). No source edit between runs.

Slope rates:
SHOT    DarmstadtWest              e72d1925  p50   2.63  p95   2.82  p99   3.25 ms  0 of 120 over 16.67, worst at 42  [sim p99 1.56 worst 2.17 | draw p99 2.88 worst 2.89]
SHOT    Wien                       8ff2d96d  p50   5.18  p95   5.78  p99   5.84 ms  0 of 120 over 16.67, worst at 25  [sim p99 1.65 worst 2.09 | draw p99 5.50 worst 5.52]
SHOT    Rosenheim                  7da2e093  p50   3.79  p95   5.84  p99   6.49 ms  0 of 120 over 16.67, worst at 2  [sim p99 1.44 worst 1.50 | draw p99 5.97 worst 11.06]
SHOT    Husum                      d60b18a7  p50   2.73  p95   2.98  p99   3.14 ms  0 of 120 over 16.67, worst at 21  [sim p99 1.14 worst 1.43 | draw p99 2.87 worst 3.08]
SHOT    Olympiaturm                07985050  p50   3.43  p95   3.96  p99   4.19 ms  0 of 120 over 16.67, worst at 63  [sim p99 1.44 worst 1.62 | draw p99 3.86 worst 3.89]
SHOT    Graz                       f93ff5b9  p50   4.42  p95   4.69  p99   5.33 ms  0 of 120 over 16.67, worst at 73  [sim p99 1.26 worst 1.28 | draw p99 4.75 worst 4.99]
SHOT    Koerbersee                 c99cdbe7  p50   6.52  p95   7.17  p99   7.73 ms  0 of 120 over 16.67, worst at 92  [sim p99 2.23 worst 2.36 | draw p99 7.43 worst 7.45]
SHOT    Malcesine                  eda034e3  p50   4.16  p95   4.67  p99   4.84 ms  0 of 120 over 16.67, worst at 35  [sim p99 1.84 worst 2.09 | draw p99 4.50 worst 4.56]
SHOT    Feldkirch                  5fa234c1  p50   4.46  p95   5.07  p99   5.65 ms  0 of 120 over 16.67, worst at 119  [sim p99 1.44 worst 1.46 | draw p99 5.22 worst 5.38]

NEXT terrain geometry: RefineByError currently runs Laying~484 BEFORE Press(yielding)~707, so generated basin/pad/corridor breaklines notinerrorref. It also SKIPS virtualsheets; fixedRefine4virtuallevels runs BEFORE errorrefine. RawDEM reference downsample/resample fromfield; patchmaxheightdeviation monotone butfullprojectionproofmissing. Planarsteepslopehaszeroheightapproxerror and should not require moretriangles withoutsidewallrelief; userrequiresactualrelief andside-facequality, notjusttrianglecount. 33x33grid/noextrudedsides,skirts16stepsremain. Investigate adaptive refinement of finalsurface, notonlyrawDEM; sidewallreliefsource/proceduralmaterial and normaldetail required.

Water geometry DOESexist: Laying~783 builds fans from WaterField Surfaces, notgeneratorWater::Occupy (thatonlydiagnoses). WaterField::Tessellate earclip method has no showncaller; actualLayingfans mayoverfillconcavepolygons/holes. Basin stamps fromwatersurface level-2m andfixedapron. PressPoints limitsterrainchanges: latestaudit Hdeepest6.933m, M28.161m,55Mstampsrefusedpastbound. Thus don'tattributeentirehundredmetrecliffsto28mstamps withoutproof. WaterlidsH220surfaces2998triangles/M58surfaces1874triangles. Shader class is2D horizontal projected onterrain, slopefallbacknowguardsimplausiblecovers.

No transient controls, allgates terminal. No commit. Fullgoal active/unachieved; AO/localbounce, projectedDEM<=1px/crackfree/noSkirts/motion, native material/alpha/scale/datums, strictvendorreds, finalall9acceptance remain. This continuation made evidence+sourceprogress, no blocker. Current slope onlyverified withactual9placerenders, no new analytic/GPUfixture yet; decoder9/9suite preceded slopeedit.

Type: feature
State: open
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

## What will be true

- [ ] A cell's refinement follows its projected error and the triangle budget at Koerbersee falls
      on the meadow and rises on the ridge
- [ ] Koerbersee's ridge silhouette carries edges a photograph can be laid against
- [ ] Negative control: hold the error bound at infinity and the ridges go back to lumps
- [ ] No place loses its frame budget: the nine surveyed places stay under 16.7 ms at p99

## What will show I was wrong

The error bound falls to one pixel and the ridges are still smooth. Then the DEM, not the lattice,
is what is missing the relief, and the source's ground sample distance is the item instead.

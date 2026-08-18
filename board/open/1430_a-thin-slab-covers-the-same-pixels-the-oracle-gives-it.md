Type: bug
Area: render
Tags: oracle, khronos

**A thin slab covers the same pixels the oracle gives it**

`SpecularTest`'s label plates are covered one pixel short of the oracle along their slanted edges, and
nothing else in the picture is.

[MEASURED] 228 pixels, **every one of them the oracle's and none of them ours** -- there is no pixel
anywhere in the frame that we cover and it does not. The bounding boxes of the two coverage masks are
identical to the pixel.

| population | oracle | ours | difference |
|---|---|---|---|
| the 23 spheres | 14 212 px | 14 212 px | **0** |
| the label plates | 26 734 px | 26 506 px | **228** |
| whole frame | 40 946 px | 40 718 px | 228 |

**The spheres are exact over a curved silhouette of comparable length**, which is what rules out the
camera, the projection, the fill rule and the sample position: any of those would move a sphere's edge
too. 227 of the 228 fall on ONE object, `Labels`, mesh 0 -- seven thin slabs in one mesh, `POSITION`
in float, a node carrying a translation and nothing else, no patch declared.

**IT IS AN EROSION AND NOT A SHIFT.** Our coverage lies down-and-left of 225 and 223 of the missing
pixels respectively and up-or-right of 3 -- but a translation would hand back on one side what it takes
on the other, and there is nothing to hand back. Area over perimeter gives the size: `228 / 3052` boundary
pixels is a uniform inward offset of **0.075 px**.

**And it is not the instrument's floor.** `worst_disagreement_px` reports 0.1353173 px against a floor of
0.005; the healthy population of that metric across the whole tree sits at or below **0.0019775 px**, two
orders under the floor, so the threshold is not what makes this red.

## Ruled out, with why

| | |
|---|---|
| **temporal jitter** | the compiled plan for this case is 2 stages with no temporal arm, so the jitter is zero |
| **partial alpha at the edge** | both renders' alpha is strictly binary -- 2 distinct values in each image |
| **a rim face we cull** | the oracle's normal at all 227 is `(0,-1,0)`, the plate's own front-face normal, identical to the interior's |
| **a projection scale error** | the flip rate is flat against image radius: 0.087, 0.104, 0.041, 0.074, 0.052 from centre outward |
| **the oracle's filter** | box at 0.01 px, so its sample sits within 0.005 px of the centre |

## What must be true

- [ ] the two masks agree on the label plates as exactly as they already agree on the spheres, or the
      term that separates them is named with its magnitude

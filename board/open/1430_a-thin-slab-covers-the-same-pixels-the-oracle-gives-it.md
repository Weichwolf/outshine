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

## A third instrument settled which side is wrong, and it is OURS

**Rasterised on the CPU in f64 from the file's own POSITION accessor, the node's own translation and the
manifest's own camera, mesh 0 covers 225 of the 228 pixels.** So the oracle is right and this engine
drops them: our own declared geometry reaches those pixels and our renderer does not draw them.

*The same instrument that settled `board:1432` and `board:1433`.* It is worth naming as a habit: **a
disagreement between two renderers is decided by a third thing that is neither of them**, and for a
silhouette that third thing is a projection anyone can write in twenty lines.

## What is ruled out by that, and what is left

The analytic mask is built with `>= 0` barycentrics, so it INCLUDES a pixel centre lying exactly on an
edge where a top-left fill rule would drop it on two of four sides -- but a pixel centre landing exactly
on an edge is measure-zero at an irrational slope, and 228 of 3052 boundary pixels is not measure-zero.
**So the fill rule is not the term.**

What is left is a sub-pixel offset of about 0.075 px between our rasterised edge and our own geometry's
projected edge, on this mesh and not on the spheres in the same frame. **A viewport scale off by 0.06 %
would have exactly this size** -- 0.075 px over the plate's 123 px extent -- and that is the first thing
to measure, against the fact that the spheres in the same picture are pixel-exact.

## What must be true

- [ ] the two masks agree on the label plates as exactly as they already agree on the spheres, or the
      term that separates them is named with its magnitude

## It is an edge offset and not a dropped triangle

[MEASURED] of the 228, **225 fall inside a mesh-0 triangle** of our own f64 projection, and their distance
to that triangle's nearest edge is:

| min | p25 | median | p75 | max |
|---|---|---|---|---|
| 0.0012 px | 0.0678 px | **0.1448 px** | 0.2217 px | 0.3493 px |

**None is deep inside a face**, and every one of the 228 is adjacent to a pixel we do cover. So no
triangle is missing and no primitive is culled: the rasterised edge sits inside the projected one by a
fraction of a pixel. *The distance is to the nearest edge of the containing triangle, which may be an
interior diagonal rather than a silhouette, so read the figures as a boundary-layer thickness and not as
the offset itself.*

## Where the next round starts

**Not in the analysis -- in a GPU-side experiment.** The remaining candidates all predict the same
picture and are separated by rendering, not by reading: a viewport transform off by a fraction of a
texel, a sub-pixel offset in the vertex path taken only by the textured fragment entry, and the driver's
own fixed-point edge quantisation. Mesh 0 is **the only textured primitive in this file**, which is the
one structural difference between it and the 23 spheres that are pixel-exact in the same frame.

## The textured-path candidate is dead, and so is the rim

[MEASURED] over the fresh run, every other case that samples a texture sits at the same scale as the
untextured ones -- `multi-uv-test` 0.0016, `texture-transform-test` 0.0012, `texture-linear-interpolation`
0.00085, `a-beautiful-game` 0.0010, `scifi-helmet` 0.00028 px. **`SpecularTest` at 0.1353173 is alone by
two orders**, so being the only textured primitive in its own file is a coincidence and not the term.

**The slab's rim is not it either.** A 2 mm slab's true silhouette is the outer edge of its rim rather
than of its front face, which would explain a small outward difference -- but [MEASURED] the oracle's
shading normal at 227 of the 228 is `(0, -1, 0)`, the FRONT face's own normal and identical to the
interior's. A rim face of a plate lying in the XY plane carries a normal perpendicular to that.

**And the healthy population's scale is itself informative**: 0.001 to 0.002 px is about what a
rasteriser's fixed-point subpixel grid displaces an edge by, and f32 projection error at this distance is
1.5e-4 px. Neither reaches 0.075.

## What would actually decide it

**Re-render the same case with the camera displaced by half a pixel.** If the 228 stay on the same
object-space edges, the cause is geometric; if they move with the sampling grid, it is the edge rule.
That needs a knob the scenario suite will have and the render suite does not, which is where this waits.

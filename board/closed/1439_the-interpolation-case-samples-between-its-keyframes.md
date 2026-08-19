Type: bug
Area: corpus
Tags: khronos, instrument

**The interpolation case samples between its keyframes**

`InterpolationTest` is the only case in this corpus carrying a **CUBICSPLINE** sampler and one of two
carrying **STEP**, and its declared grid landed on nothing but keyframes.

[MEASURED] its keys sit at **0, 0.5, 1, 1.5 and 2 s**; at 1 fps the derived grid ran **0, 1 and 2 s** --
*every sample exactly on a key*. **STEP, LINEAR and CUBICSPLINE all return the key's own value there**,
so the one case named for interpolation compared three keyframes and decided nothing about any of the
three curves. Green, and hollow.

At 4 fps the grid is **0, 0.25, 0.5, 0.75 and 1 s**, so three of five samples fall strictly between keys
and the three curves are three different answers there. The rate is a declaration; no threshold moved.

## What it decided once it could

| frame | 0 s | 0.25 s | 0.5 s | 0.75 s | 1 s |
|---|---|---|---|---|---|
| coverage | 0.059873047 | 0.054280599 | 0.041616753 | 0.045846354 | 0.059873047 |
| `picture_p99_delta_code` | **0** | **0** | **0** | **0** | **0** |

**All three interpolation modes, evaluated between keyframes, agree with Cycles to the code.** The
coverage moves by 30 % across the grid, so the samples are not a still under another name.

## Two engine questions it raised on the way, and one was a real gap

**A node scaled to nothing is a picture and not a refusal.** The asset pulses all three samplers from
`(1,1,1)` to `(0,0,0)` and back -- that IS its subject -- and the reader refused the whole pose:

```
node 0 carries a NORMAL and a transform with no inverse,
so the surface it is perpendicular to has collapsed
```

A zero scale is legal glTF. Such a node has every vertex on one point, every triangle degenerate and
nothing to draw, so **it exists and is infinitely small** -- and the engine's own rule is *degrade on
detail, refuse on existence*. The normal is now zero, which is the arithmetic's own answer rather than a
substitute for one, and it is the same statement the line beneath it already made for a zero-length
normal the file declares: *the consumer sees a zero and the picture shows it*.

**And the window had to follow the grid again.** Raising the rate changed what the grid sweeps, and a
vertex at frame 2 landed at 40.997298 m against a near plane of 41.273223 -- `board:1433`'s rule a third
time. This case's camera is **deliberately elsewhere**, 13.061 m from the framing rule's answer, which is
the *or a metre or more* arm of the placement rule, so the window was re-derived about the DECLARED eye:
the swept radius 8.537972820 m about the grid's own centre, at the declared eye's distance to it,
47.346721586 m.

## Comments

Found by asking a coverage question rather than a precision one: **which interpolation modes does this
corpus actually exercise?** The answer was LINEAR 1310 samplers, STEP 4, CUBICSPLINE 3 -- and then the
sharper question, whether any grid sample falls where the three differ. *A count of samplers is not a
count of what was tested.*

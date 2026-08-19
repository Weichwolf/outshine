Type: bug
Area: clients
Tags: perf, instrument

**The near-plane precondition is answered by eight corners on the frame path**

Aiming a camera at a subject costs a number that does not grow with the subject. The precondition
*no vertex sits inside the near plane* is decided from the subject's bounding box, exactly, and the
per-vertex walk survives only on the path that refuses -- which is not a frame.

## What it was

`ClearsNearPlane` walked **every vertex of the subject on every `Aim`**, and `Aim` is what a moving
camera calls each frame. `ABeautifulGame` carries **934 309 vertices**, so a turning camera read
roughly 22 MB of positions per frame -- on a unified-memory device, over the same bus the GPU was
drawing from.

**`CLAUDE.md` names this exactly**: *the frame path is made of bounded terms -- every step in it costs a
number somebody can name*. A scan whose length is the subject's vertex count is the one shape that rule
forbids, and it sat behind a function whose name promises a test.

## What it measures, over a declared population

`ABeautifulGame`, 1280x720, 80 frames after a declared warmup, serialised on the device.

| | camera standing | camera turning, before | camera turning, after |
|---|---|---|---|
| the engine's own side, p50 | 0.0675 ms | **1.0555 ms** | **0.0795 ms** |
| the engine's own side, p99 | 0.2507 ms | **4.1179 ms** | **0.3113 ms** |

**Turning the camera cost sixteen times the whole of the rest of the frame path.** The standing arm is
the control and it is what named the term: the difference of two measurements, not an attribution
somebody assumed.

## The second-order effect, which is larger than the first and is named because it is not obvious

| | before | after |
|---|---|---|
| serialised frame, p50 | 10.97 ms | **6.21 ms** |
| serialised frame, p99 | **23.24 ms** | **7.65 ms** |

**The whole frame fell by 15.6 ms at p99 while the CPU term fell by 3.8.** The remaining 11.8 ms is the
device: 22 MB per frame of position reads is memory bandwidth the GPU was competing for on a shared bus,
and a CPU stalled in that scan is a GPU with nothing queued. *The arithmetic saved is the small half.*

**The run went from missing the frame budget at p99 to holding it with 54 % of it unspent**, and it was
one precondition.

## Why the box is exact and not conservative in the direction that matters

Distance along the view axis is a **linear functional**, so its minimum over an axis-aligned box is
attained at a corner; every vertex of the subject lies in the box. A box whose minimum clears the plane
therefore **proves** every vertex clears it. Only a box that fails falls through, because there the box
is genuinely conservative -- a corner inside the plane says nothing about any real vertex -- and that
path ends in a refusal naming the vertex, which is not a frame.

## Comments

**No instrument in this tree could have found it.** The render suites draw one frame per pose, so a
per-frame cost is paid once and disappears into a case's total; the frame suite measures a still camera.
It took a run over a MOVING camera, which is what `board:1457` is for, and it was the second thing that
suite measured.

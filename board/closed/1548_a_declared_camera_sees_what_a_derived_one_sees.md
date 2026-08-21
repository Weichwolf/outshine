Type: bug
Area: clients
Tags: bug, instrument

**A declared camera sees what a derived one sees**

**The reviewer found this and his proof is the kind that ends an argument:**

> *A ground plane fills from the horizon TO THE BOTTOM EDGE, for any camera pitch, always. A 74-pixel
> band with emptiness BELOW it is incompatible with any camera pose over a continuous surface.*

**And the decisive pattern is that the same data gives opposite results per camera:**

| Station | `framed` (derived eye) | `first` / `third` (declared eye) |
|---|---|---|
| km 17.3 | a solid ground plate | **no ground surface at all** |
| km 708.1 / 728.5 | **0.00 % opaque** | **80.7 % / 93.29 % opaque** |

One camera sees everything, the other sees nothing, in the same frame of the same world. That rules
out missing data and rules out streaming.

## What has been ruled out, each by a run

| Guess | Ruled out by |
|---|---|
| the camera pose is wrong | the printed eye is right: 6.8 m behind the car, forward horizontal at y = 0.008, up +Y. And the same camera kind produces a correct image at km 267.1, where occupancy grows monotonically downward 2.3 % → 56.2 % |
| it is correct behaviour | the reviewer's geometric argument above |
| the road floats over raw terrain and buries the camera | **measured 0.41 m** at km 17.3 -- raw ground 471.97 m asl, the car at 472.38. That does not bury an eye 1.5 m up |
| the ground patch does not reach the car | centred it on the car and re-laid on a 280 m stray. **No change** |
| the near plane clips it | 0.1 m to 2.0 m. **No change**. And the projection is reverse-Z with an INFINITE far plane, so distance cannot cull either |

## Two more ruled out, and the first one EXONERATES the path this item is named after

| Guess | Ruled out by |
|---|---|
| **the declared path itself** -- `standsInside`, or anything `Look`'s declared branch skips | **The derived camera's own placement, read back and re-set as a DECLARED eye, draws a byte-identical picture.** 13 805 bytes against 13 805. The declared path is not the defect; the VALUES I hand it are |
| depth precision at a 0.1 m near plane | the depth target is `D32_FLOAT` and the projection is reverse-Z -- which is exactly the pairing that makes a 0.1 m near plane sound. Unreal ships a 10 cm near clip on the same technique |

**What the two cameras actually are**, printed side by side at km 17.3:

```
DERIVED  at 5522.8 2452.5 3737.9   fwd -0.770 -0.342 -0.539   yfov 0.4711   near 6201.2416
MINE     at  -45.6    0.2  -214.8  fwd -0.221  0.008 -0.975   yfov 1.1345   near 0.1
```

The derived eye stands **7 km away** framing the whole 1400 m patch. Mine stands 1.5 m over the car.
**The geometry says mine should see ground**: the patch spans z from -878.7 to +581.3, the car sits at
z = -221.7 and looks toward -z, so roughly 660 m of ground lies ahead of it.

## And two more, each by a run

| Guess | Ruled out by |
|---|---|
| the ground patch is degenerate, or flat, or somewhere else | **measured, per re-lay**: `min -700.0 -14.5 -692.0  max 692.0 11.8 700.0`, 13 689 posts. A real 1400 m patch with 26 m of relief, centred on the origin the eye is 222 m from |
| `Restand` fails silently and the geometry is stale | the return value is checked and printed now. **278 log lines, zero refusals.** It succeeds every time |

**Everything measured is consistent, and the picture is still empty.** The subject is where it should
be, the eye is where it should be, the patch is real, the re-stand lands, the path is exonerated and
the depth is sound. That is the state this item hands on.

## What is left

**The difference between the two paths is exactly one bool and one placement.** `Live::Look` takes the
declared branch when an eye was set and the derived branch otherwise; both reach `Aim`, both run
`Anchored` and `EcefFromGltf` over eye, forward, right and up. Whatever differs is inside those values
or inside what `standsInside` skips.

## FOUND -- and it was an ORDER, not a value

**`Stand()` resets `Stood_ = Studio{}`, which empties `PartPlacement`. `BuildDrawList` read exactly
that**: `part < studio.PartPlacement.size() ? part : 0`. With the table empty at compile time, **every
part took `ModelSlot` 0 -- the car's** -- so the road and the ground were transformed by the vehicle's
pose and landed roughly twice as far out as the car.

**It explains both halves of what the reviewer measured.** The declared camera at the car saw nothing
because the world had been moved away from it; the derived camera, framing from kilometres out,
sometimes caught the displaced geometry. *Same data, opposite result per camera.*

**Every part takes its own slot unconditionally now**, and `Stand()` seeds the table with identity for
every part before anything compiles against it, so the table is never shorter than the draws naming it.

**Measured on `km0017.3-third`, the reviewer's own instrument:**

| | before | after |
|---|---|---|
| opaque fraction | **1.09 %** | **49.04 %** |

The ground fills from a horizon at half height to the bottom edge. *A ground plane fills to the bottom
edge for any pitch, always* -- met.

## What must be true

- [ ] **A picture drawn from a declared eye holds the same geometry a derived eye holds**, and a test
      decides it: stand one subject up, draw it from a derived camera and from a declared camera at
      the same place, and compare the opaque coverage
- [ ] **The occupancy of a frame is a published number**, because it is the one the reviewer measured
      first both times and the one no existing test looks at

## Comments

**No test in the tree looks at whether a frame has anything in it.** The windowed drive published
p50 0.025 ms over 1 469 407 frames while every one of them was `(0,0,0,0)`. The reviewer's first
instrument -- opaque fraction -- is the cheapest possible guard and it does not exist here.

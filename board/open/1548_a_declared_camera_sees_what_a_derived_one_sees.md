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

## What is left

**The difference between the two paths is exactly one bool and one placement.** `Live::Look` takes the
declared branch when an eye was set and the derived branch otherwise; both reach `Aim`, both run
`Anchored` and `EcefFromGltf` over eye, forward, right and up. Whatever differs is inside those values
or inside what `standsInside` skips.

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

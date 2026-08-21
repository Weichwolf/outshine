Type: bug
Area: corridor
Tags: perf

**Grading the ground costs what a LOOKUP costs, not what a scan costs**

The deformation field finds each ground post's nearest point on the corridor by **scanning every
centreline sample**. Both samplings were tightened to resolve what they must -- and the product is the
defect:

| | |
|---|---|
| posts in one patch | 267 x 267 = **71 289** |
| centreline samples over 900 m at 1.5 m | **601** |
| distance computations per laying | **42 844 689** |
| layings over the route, one per 400 m | **1 937** |
| **over Munich to Hamburg** | **83 000 000 000** |

**Measured**: the stills tool no longer finishes inside 540 s where it took ~250 s before the
refinement. Nothing was wrong with the refinement -- 3.0 m posts and 1.5 m samples are each derived
from Nyquist against a declared width, and `board:1529` needed both to put the carriageway in the
frame. What is wrong is the **brute force between them**.

## What must be true

- [ ] **A post finds its station in O(1) or O(log n)**, not by scanning 601 of them. The corridor is a
      polyline in station order and a post's east-north is a monotone query along it
- [ ] **The cost is stated where it is spent** -- *everything that grows states its bound*, and a
      product of two sampling rates is exactly the kind of growth that hides inside two reasonable
      numbers
- [ ] **The tool finishes**, so the drive can be looked at again

## Comments

**Introduced knowingly and measured immediately.** The two refinements were right and are keeping the
road visible; what they exposed is that the grading was written as a scan because at 12 m posts and
10 m samples it was 1.9 million operations per laying and nobody noticed. The same code at the sampling
the picture actually needs is 22 000 times that.

**`ReferenceLine::Nearest` already exists and already does this properly** -- golden section over the
station axis, and `board:1523` sharpened it. The grading should ask it rather than carry its own scan.

Type: issue
Area: corpus
Tags: instrument, oracle, scope

**The subject fills six tenths of the frame, and the detail nobody can see is the detail nobody measures**

**The owner: use the render target fully where possible -- the details must be visible.** There are two
levers behind that sentence and they cost very differently, so they are separated here rather than
answered together.

## Lever one: resolution, and it is nearly free

| | |
|---|---|
| still cases | already **1280x720** |
| **animated cases** | **320x180**, and it is a stated cost decision: an animated case multiplies every product by its frame count, one quantity's raw at 1280x720 is 14.75 MB, and five raws over a grid would be **2.29 GB against a corpus already measured at 19.9 GB** |

**So the animated grid is where this bites, and the trade is real rather than a habit.** The middle
ground nobody has priced: **raise the resolution and shorten the grid**. A case proving an interpolation
does not need eighteen frames as much as it needs frames a person can look at, and the product volume is
the product of the two.

- [ ] **Price it before choosing**: frames x resolution against what each case's claim actually needs.
  *Not run here, because the answer changes what is stored and that is worth a measurement rather than a
  guess.*

## Lever two: the framing fill, and it is the expensive one

[MEASURED] `src/gltf/Framing.h` carries `kFramingFill = 0.6` `[SET]`. **The subject fills six tenths of
the frame height and four tenths of every render is empty.** At `0.9` the subject's image area grows by
`(0.9/0.6)^2 = 2.25x`.

**That is not cosmetic and this is the argument for it.** Every number this suite publishes is taken over
a population of covered pixels — the picture bound's tail, the coverage comparison, the channel counts.
**A larger subject is a larger population, and a larger population is a sharper measurement**, not merely
a bigger picture. The 40 % that is empty today contributes nothing to any verdict.

**The blast radius, stated so it is not discovered:**

| | |
|---|---|
| every manifest's camera | **derived from 0.6 and quoted verbatim** as fifteen-digit doubles. All of them become wrong at once |
| `test/outshine/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp` | recomputes the rule per case and scores the declaration — it goes red for every case until every camera is re-derived |
| every oracle render | the cache key covers the declared scene, so **every product is re-rendered**. Cached hits become misses corpus-wide |
| the `exact` cases | `Triangle` spends its camera on a lattice-offset condition instead of on the rule. **Those cameras are not the rule's and must not be moved by this** — which the tree already knows: the two determinations of the camera distance are named in that unit test |

## The recommendation, and its timing is the point

**Raise the fill, and do it now rather than later.** It touches **34 cases today** and it would touch
**148** once the corpus is populated — and `board:1227` has just shown that adding a case is cheap, so
the population grows from here. *The cost of this change only ever goes up.*

**A number rather than a direction**: `0.9` leaves a tenth of the frame as margin on each side, which a
subject whose silhouette is being compared needs — a subject touching the frame edge has its boundary
clipped, and the coverage comparison would then be measuring the frame instead of the model.

- [ ] **Whether the `exact` cases are exempt or re-derived is a second decision** and it is not this
  issue's. They are the cases where the camera answers to a construction rather than to the rule

## Comments

**Filed and not applied, because the number is the owner's and the sweep is a round of its own.** *What
it must not become is a constant edited in a round about something else, with 34 cameras re-derived
underneath a diff nobody reads.*

## DECIDED: the fill goes to 0.9, and the animated grid trades frames for pixels

**The owner delegated it: be reasonable, and it must be achievable.**

### `kFramingFill` 0.6 -> 0.9

[DERIVED] the rule frames the **bounding sphere**, which contains every vertex by construction, so a
fill of `f` guarantees a margin of `1 - f` of the half-height on every side **whatever the subject's
shape**. At 0.9 that is a true 10 % margin: no silhouette can touch the frame edge, so the coverage
comparison never measures the frame instead of the model. **The subject's image area grows by
`(0.9/0.6)^2 = 2.25x`**, and every population this suite takes a number over grows with it.

**Why not 1.0**: a subject inscribed in the frame has its boundary clipped at four points, and a clipped
silhouette is a coverage disagreement neither renderer is responsible for. **Why not 0.95**: the margin
would be 36 px of 720, which is under the framing rule's own bounding-sphere slack for an elongated
subject and buys 11 % of area for the risk. *0.9 is the largest value whose margin is still legible as a
margin.*

### The animated grid: 320x180 stays, and the frame count is what gets spent

**Not raised, and the reason is that the constraint is real**: five raws over an 18-frame grid at
1280x720 is 2.29 GB against a corpus already at 19.9 GB. **What is traded instead is the grid** — an
interpolation claim needs frames a person can look at more than it needs eighteen of them.

- [ ] **Priced rather than assumed**: frames x resolution per animated case, against what its claim
  needs. *A case proving `STEP` needs the frames either side of a step and not a sweep.*

### What this costs, and it is why it happens now rather than later

Every quoted camera is re-derived, `ADerivedCameraIsTheFramingRuleAndNotAQuotation` goes red until they
all are, and **every oracle product is re-rendered** because the cache key covers the declared scene.
**34 cases today, 148 later** — and `board:1227` has just shown a case costs no engine work, so the
population only grows. *The cost of this change never goes down.*

- [ ] **The `exact` cases are exempt and that is not a loophole.** `Triangle` spends its camera on a
  lattice-offset condition rather than on the rule, and the tree already knows these are two
  determinations of one quantity. **An exempt case is one whose `acceptanceClass` is `exact`**, which is
  a declaration already in its manifest and not a list somebody maintains

**Closed on its own answer.** The sweep is `board:1361`.

Type: bug
Area: assets
Tags: bug, scope
Depends: 1543

**The declared F31 has a ROUTE INTO THE TREE**

**`tools/driver/f31.scenario` declares an asset that nothing in this repository can obtain.**

```
<asset uri="scene.gltf" kind="gltf"
       digest="c60068fcd0f8c25e73225cd3725a422fca46c00a2a68ca481988a6680cc5fb1d"/>
```

**Searched and absent, all four places:**

| Where | Result |
|---|---|
| the tree | no `scene.gltf` outside the prepared corpus |
| the prepared corpus | **1184 cases**, none of them the car -- the nearest are `CarConcept` and `ToyCar` |
| the fetch cache and the content store | nothing under that digest |
| `test/harness/shared/corpus/prepare.py` | **no mention** of the F31, DisneyCars or BMW |

`board:1511` measured this asset -- 519 nodes, 258 meshes, 23 materials, `KHR_materials_clearcoat`,
CC-BY-4.0 -- so it existed on this machine once. **What is missing is the one offline script's recipe
for it**, and the digest is already pinned to check the answer against.

## Why this is filed rather than worked around

The goal's own rule decides it: *a road the data cannot support is a NAMED REFUSAL, and refusing
loudly is a pass*. Drawing a box where the car should be would be a fallback path, and *a dead path is
worse than a missing one*. So the car is **not drawn**, this says why, and nothing pretends otherwise.

## What must be true

- [ ] **`prepare.py` fetches the F31**, idempotently, checked against the digest the scenario already
      declares -- the same fetch-and-verify every other subject in the corpus gets
- [ ] **The windowed driver stands the car beside the road**, at the placement `board:1543` now
      carries per part, with the corridor at slot 0 and the car at slot 1
- [ ] **Its headlights and cabin lights come with it** as the `Gltf::PlacedLight`s `Live::Stand`
      already gathers -- no new interface, and the mechanism is built

## Comments

**Everything downstream of the asset is ready and measured.** A batch never spans two placements
(`test/unit/render/draw/TwoThingsInOnePictureKeepTheirOwnPlacements.cpp`), the encoder pushes a
transform per batch and only when it changes, and a picture of one placement submits exactly what it
did before. What is missing is the car itself.

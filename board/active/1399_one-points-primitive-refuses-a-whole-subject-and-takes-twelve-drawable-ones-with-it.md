Type: bug
Area: gltf

**One POINTS primitive refuses a whole subject and takes twelve drawable ones with it**

`Subject::Flatten` refuses any file carrying a primitive it cannot draw as a surface:

```
primitive of mesh 0 is POINTS, and this subject draws surfaces only --
TRIANGLES, TRIANGLE_STRIP or TRIANGLE_FAN
```

**All seven modes are glTF 2.0 and a file is entitled to all of them.** A rasteriser that draws
surfaces is entitled to draw only surfaces; it is not entitled to draw NOTHING because one primitive
is a point cloud.

## What it costs, counted rather than asserted

[MEASURED] over the 148 models at the pin, **2 carry a non-surface primitive**:

| model | modes present | surface primitives that would still draw |
|---|---|---|
| `MeshPrimitiveModes` | 0, 1, 2, 3, 4, 5, 6 | **3** |
| `PrimitiveModeNormalsTest` | 0, 3, 4 | **9** |

Twelve drawable primitives are lost to two refusals. **`MeshPrimitiveModes` is the case whose entire
purpose is that TRIANGLE_STRIP and TRIANGLE_FAN triangulate to the same surface the oracle draws** --
and the refusal fires before that question is asked.

## Why it is a bug and not strictness

**A hole is worse than a coarse tree, and this is a hole with a loud message attached to the wrong
thing.** `CLAUDE.md`: *degrade on detail; refuse only on existence, and refuse loudly.* A POINTS
primitive is not a subject that cannot exist -- it is a part of a subject this renderer has no pass
for. The rule the tree already carries for exactly this is **every capability answers what it achieved,
in both directions**: draw the surfaces, and PUBLISH how many primitives were not drawn and why.

*The current behaviour is also the silent-hole shape wearing a refusal: a case with one stray point
primitive reports "cannot draw" about a model that is 95 % triangles.*

## What must be true

- [ ] **A non-surface primitive is SKIPPED, not fatal**, and the subject keeps its surfaces
- [ ] **The count and the modes skipped are published on the subject**, so a caller can tell a subject
  that drew everything from one that drew most of it
- [ ] **A subject with NO surface primitive at all is still a refusal**, because then there is nothing
  to draw and silence would be the hole
- [ ] **The oracle's own answer is checked, not assumed**: Cycles renders neither loose vertices nor
  loose edges, so both sides are expected to leave those pixels empty -- and that agreement is a
  finding to be measured, never a premise

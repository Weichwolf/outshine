Type: feature
State: open
Area: import, engine, scenario
Tags: architecture, owner, determinism
Depends: 2127

# Every BODY is a closed manifold with outward normals, classified at import; a SURFACE is allowed and may claim no body verb

**Benchmark** -- Unreal: the mesh build welds vertices, fixes winding by adjacency and builds
collision from the result; Chaos wants closed convex pieces and Nanite tolerates open meshes
because a card is content. RAGE: collision bounds are authored closed and a fragment is a
closed piece. **Both agree** that a thing that COLLIDES, OCCLUDES or CASTS is a volume, and
that a leaf, a decal and a water plane are not and never have to be. Neither refuses an open
mesh at the door, and the Khronos corpora this tree proves against contain open meshes on
purpose -- so refusing them outright would take the oracle away. Decided with the owner
2026-09-05: the strictness holds for BODIES and the door CLASSIFIES rather than refuses.

## Where it stands, measured 2026-09-05

```
  glTF import              geometry taken as given; no manifold test, no orientation test
  physics (board:2127)     Rigid, gravity only; no shape needs a volume yet
  shadows, occlusion       drawn from whatever triangles arrive; an open mesh leaks light
  the ground               a window of a closed ellipsoid; its rim is the skirt, its level
                           boundaries are stitched (board:2115); a visible crack is board:2144
```

## The solution

At import (a preload cost, O(N log N) over an edge hash, never on the frame) every mesh is
measured and CLASSIFIED, the counts published:

| class | test | what happens |
|---|---|---|
| **body** | every edge has exactly two faces in opposite directions; signed volume > 0 after consistent orientation | all verbs: contact, cast, occlude, reflect, inside/outside |
| **repairable** | orientation inconsistent but propagable; vertices within `epsilon` unwelded; open caps with a planar boundary | repaired deterministically (weld, flip, cap), each repair counted |
| **surface** | open by design | drawn two-sided; no volume, no interior, no occlusion; a scenario asking a body verb of it is REFUSED at declare, loudly |

The Earth is closed by construction (an ellipsoid plus a height field) and the frame draws a
window of it; the invariant for the ground is therefore "no visible crack" (board:2144's
oracle), not "the drawn set is a manifold". Bodies may interpenetrate (a house's foundation in
the ground); each is closed on its own and no union is built.

## What will be true

- [ ] Every imported and generated mesh carries its class and the import publishes the three
      counts and the repairs made
- [ ] The Khronos corpus imports with its open meshes as surfaces and its textured path
      unchanged: the vendor cases stay green and the nine references bit-identical
- [ ] A generated building is a body (closed, outward) by construction, measured on every one
- [ ] Negative control: a cube missing one face is classified a surface; a scenario that gives
      it contact is refused at declare with the mesh named; welding the face back makes it a
      body and the same scenario is accepted

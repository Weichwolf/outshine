Type: feature
State: active
Area: world, generators, render
Tags: geometry, measured, determinism, benchmark

# "Perfect" geometry is DECIDED by a walk, at every rung

**Benchmark** — Unreal: `UStaticMesh` build reports open edges, degenerate triangles and
overlapping UVs, and Nanite REFUSES a mesh it cannot cluster; a level's geometry is validated on
import rather than trusted. RAGE: map geometry is authored and its export pipeline rejects what it
cannot ship. **Both agree** that the mesh is checked by a program before it is drawn, and neither
trusts an eye for it.

## The owner's rule

> Geometry may never interpenetrate geometry. It holds at EVERY rung -- a far rung is CHEAPER
> because the bodies there are simpler, never LOOSER. And "perfect" is programmatically verifiable.

**Why it pays rather than merely being tidy**: a still frame forgives an intersection, a moving one
does not. Two surfaces passing through each other flicker as the camera moves, and TAA integrates
the flicker into a smear that no sharpening removes. Getting geometry right is what lets the
temporal filter do its job, and the bill for getting it wrong arrives later, in motion, where it is
hardest to trace back.

## THE INSTRUMENT EXISTS, IT RUNS, AND IT IS RED

`--audit` already walks the world's geometry. Kaiserberg:

    solid: building edges on ONE triangle, so a HOLE               8 308 edges
    solid: building edges on MORE than two, so not a surface      83 298 edges
    solid: building triangles with two corners in one place           64 triangles
    buildings: triangles that are needles                             97 triangles
    buildings: triangles reaching over 20 m                      183 269 triangles
    solid: corners identical in POSITION AND NORMAL              109 903 corners
    solid: building vertices welded away as coincident         2 206 176 vertices

**Buildings are not closed manifolds today.** Eight thousand boundary edges are holes; eighty-three
thousand edges carry more than two triangles, which is not a surface at all. Nobody was looking,
because the walk costs 11.3 s of Shibuya's load and is off by default -- so it is a TOOL, and what
this item makes it is a GATE with declared ceilings that may only fall.

## THE WALK KEYED A POSITION BY A HASH, and the number that repair seemed to buy was NOT ITS OWN

Found while writing the case that would hold these numbers, which is exactly what a ceiling is for:
the count moved between runs while the picture's digest did not.

The weld keyed a vertex by `(cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791)` -- a SPATIAL HASH,
not a coordinate. Two distinct positions that collide were welded into ONE, and every such merge
invents edges that then appear to carry more than two triangles. A hash is not an identity, and a
walk that treats it as one reports defects that are artefacts of its own key.

**The repair stands on principle and its measured effect does not.** A drop of 1 683 edges was
credited to it and then withdrawn: board:2086 measures the world's own content varying by 2 947
footprints between runs while the digest holds, which is what actually moved the number. What can be
said is that the key can no longer merge two different positions; what cannot yet be said is how
many it was merging.

The second walk had the same defect: `solid: corners identical in POSITION AND NORMAL` keyed on an
FNV-1a of six floats, so a collision would have counted two DIFFERENT corners as identical. At this
scale it happened not to collide -- 109 903 either way -- which is worse than if it had, because a
latent lie is one nobody looks for.

**The repair is the same in both: the key is the VALUE, and the hash is the container's business.**
`std::unordered_map` resolves a bucket collision with `operator==`, so a proper key type cannot
merge what is not equal. This is `CLAUDE.md`'s named trap once more -- a measure that cannot see --
and this time the measure was reporting defects that were artefacts of its own key.

## Six properties, and the tree measures THREE

| property | decided today | by what |
|---|---|---|
| closed -- every edge on two triangles | **yes** | `edges on ONE triangle` = 8 308 |
| manifold -- no edge on more than two | **yes** | `edges on MORE than two` = 83 298 |
| meeting vertices share an INDEX, not a coordinate | **partly** | the weld counts what coincided; it cannot say what SHOULD have |
| consistently wound | **no** | -- |
| no self-intersection | **no** | -- |
| **no two bodies overlapping in plan AND height** | **no** | -- and this is the owner's rule itself |

The last one is the one the goal has been asking for all along. board:2076 measures a shadow of it
for one case -- `ends STILL crossing` = 3 581 of 12 720 -- by asking whether a trim was capped,
which is a proxy for an overlap rather than the overlap itself. The general test is a broadphase
over the bodies' boxes and an exact test on the pairs that survive it, which is what a physics
engine already does every frame and what this tree's own `TriangleBvh` is for.

## What will be true

- [ ] Every one of the six is decided by a walk, and each prints its number whether or not it is
      zero -- a check that prints nothing when it passes cannot be seen to have run
- [ ] Each number is a DECLARED ceiling that may only FALL, the same instrument as the lint
      baselines. 8 308 and 83 298 are today's, and they are ceilings rather than targets
- [ ] The walk runs at EVERY rung, not only rung zero. A far rung is simpler and therefore CHEAPER
      to check; it is not exempt
- [ ] Negative control: a deliberately intersecting pair -- two boxes sharing a volume -- makes the
      overlap check go red. A check that passes on a known-bad pair proves nothing
- [ ] The cost stops being a reason to skip it: 11.3 s on Shibuya is a full walk of a full world,
      and what a gate needs is the walk over what CHANGED

## What this does NOT cover

Whether the geometry is RIGHT. A closed, manifold, non-intersecting building can still be the wrong
building in the wrong place, and no walk can tell. That is what the vendor corpora and the pictures
are for, and this item does not weaken either.

It also does not cover the GROUND's own resolution: board:2084 measures 90.8 per cent of footprints
narrower than one ground cell, so a body cannot yet share a vertex with the ground beneath it. Until
the ground is tessellated, "meeting vertices share an index" cannot be true between a building and
the terrain -- only between one building and another.

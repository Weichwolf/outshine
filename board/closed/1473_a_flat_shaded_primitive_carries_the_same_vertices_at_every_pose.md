Type: bug
Area: gltf
Tags: bug

**A flat-shaded primitive carries the same vertices at every pose**

`board:1471` gave a primitive that declares no NORMAL the flat one the format requires, and it split a
vertex only at a CREASE -- the same bargain `BuildTangentsFor` makes for a seam, and cheaper than one
vertex per corner. **The crease test reads POSITIONS, and it runs after the pose is baked.** On a
skinned subject the answer therefore changes with the frame, so the vertex COUNT became a function of
time, and a static index buffer with a dynamic vertex stream is exactly the thing that cannot survive
that.

## What must be true

- [x] **The split is a function of the index run and of nothing else.** A corner that reaches a vertex
  first keeps it, a corner that arrives second takes a copy, and no coplanarity is asked about
- [x] **The vertex count of a posed subject is the same at every frame of a declared grid**, which is
  the invariant the harness already held and which is what caught this
- [x] The degenerate triangle keeps the zero normal rather than a guess -- it covers no pixel

## What it cost, and both halves were found by the same run

**Two defects, one round, and the second was hiding behind the first.**

`KeyOf(const double basis[4])` is a POINTER with a bound written on it for decoration. `FlatNormalsFor`
handed it a `double[3]`, so it read a fourth word off the stack; the key is a `memcmp` over the bits, so
the split fragmented at RANDOM. [MEASURED] **26 cases died under the sanitiser** -- the whole
`Animation_Skin` and `Animation_SkinType` groups plus every case that declares no normal at all
(`PrimitiveModeNormalsTest`, `SimpleTexture`, `TextureTransformTest`, `TextureEncodingTest`, `Cameras`,
`MultipleScenes`, `MeshPrimitiveModes`, `RecursiveSkeletons`) -- and 3 more moved their picture.

**And the random split MASKED the real defect.** `Animation_Skin_07` and `_09` had been reported as
moving, because uninitialised stack differed between frame 0 and frame 1 and the geometry differed with
it. With the read repaired, the motion measurement came back **0 px**, and the check that actually fired
was *the posed subject carries the same vertices at every frame of the grid* -- the count, not the
picture. *A number that was right for a reason nobody would accept.*

| case | frames | moved, before | moved, after |
|---|---|---|---|
| `Animation_Skin_07` | 2 | 0 px -- and the vertex count differed | **20.7958292 px** |
| `Animation_Skin_09` | 2 | 0 px | **230.055377 px** |
| `RecursiveSkeletons` | 2 | 0 px | **74.7730774 px** |

**The population is named because eight of the twelve say nothing.** `Animation_Skin_00,02,03,05,06,08,10,11`
declare a ONE-frame grid, so their `0 px` is trivially true and is not evidence of anything;
`Animation_Skin_01` and `_04` declare two frames and moved throughout. The claim is about the **four**
two-frame cases, and two of them were red.

## The caveat was sought first and it took most of the round

The first scan of the log directory reported `Spaces` and `With` as failing cases. **They are two days
old**: `board:1228` already holds `Box With Spaces`, whose log from this run is green. `CLAUDE.md`'s own
warning -- *a partial run leaves the previous run's logs in place* -- cost the round twice, because the
same scan also mixed 24 shader logs that carry no `CHECKS` trailer into a list of crashes. **Every count
below was retaken over logs newer than the run's start.**

## The mechanism is the shipped one and the assumption came with it

**A bake decides topology once; a pose writes values into it.** That is the same sentence as a static
index buffer with a dynamic vertex stream, and it is why the split may not consult geometry. The
assumption that travels with it: *the faceting of a mesh is a property of the ASSET*, not of the frame
-- which is true here because flat normals exist only where the file declared none, and the format's
answer for that is per-face and not per-crease.

## Comments

The head comment on `FlatNormalsFor` said **one vertex per corner, without the dedup -- there is
nothing to share** while the code below it deduplicated. The comment was right and the implementation
had drifted from it; the repair was to build what was already written down.

An independent implementation in Python -- inverse binds, joint globals, LINEAR quaternion blend, read
straight out of the `.gltf` -- put the vertex movement of `Animation_Skin_07` at **0.110755 m** and
**0.177208 m** for its two skins between 0 s and 1 s. That is what said the engine was wrong before any
engine code was read, and it is why the 0 px was not believed.

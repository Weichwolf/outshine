Type: feature
State: open
Parent: 1953
Area: render
Tags: benchmark, target, gpu-driven

# The render pipeline has ONE route, and the renderer knows no subject

**Benchmark** — Unreal: everything drawn is a `UPrimitiveComponent` with an `FPrimitiveSceneProxy` in `FScene`, drawn by the base pass — Landscape, foliage, water and a character take the same route, and there is no terrain pass. RAGE: terrain, props and vehicles are all map entities on one draw list. **Both agree, and neither has an exception** — one route from data to pixel, and what a thing DEPICTS is decided above the renderer, never inside it.

This is the parent of board:1989 · 1990 · 1991 · 1992 · 1993 · 1994. It states the route; they
build it.

## Where the pipeline starts, and it is not at the tiles

**`Geometry` is the seam.** Above it: the scenario, the physics, the generators, the readers.
Below it: one cooker, one cooked form, one pass. The render pipeline BEGINS at `Geometry` and
knows nothing of what is above.

    ABOVE THE SEAM -- not the renderer's business
      scenario declares · physics integrates bodies · a mind controls
      glTF reader          -->  Geometry
      every generator      -->  Geometry
      a client's own code  -->  Geometry

    ---------- Geometry: the one value 3D crosses on ----------

    BELOW THE SEAM -- the render pipeline
      ONE cooker      -->  cooked form: one-width GPU streams + cluster DAG  (board:1991)
      culling in compute decides what survives                              (board:1992)
      ONE indirect draw per pass                                            (board:1993)
      tonemap -> present -> 720p60

**Everything that produces 3D produces a `Geometry`** -- the glTF import, every generator, a
scenario's own declaration, a client's C++. There is no second entrance, which is what makes one
cooker possible at all. `include/Geometry.h` must therefore carry whatever a FILE can carry, or a
generator is weaker than a file and the interchange claim is false.

**Terrain enters where a tree enters.** The engine knows laws and no subjects, and `terrain` is as
much a subject as `car`.

## Motion belongs to the placement, not to the shape -- except when it does

TAA needs a velocity per pixel, and the tree already resolves that in two halves, correctly
separated:

| what moved | what carries it | where |
|---|---|---|
| a rigid thing | the PREVIOUS transform | `S::prevMvp`, `S::prevAnc` |
| a deforming thing (skin, morph) | the PREVIOUS vertex positions | `Posed::Previous()` -> `SubjectProxy.cpp:292` |
| the result | `Resource::SceneVelocity` | written by the geometry pass |

So an authored `Geometry` does NOT carry motion: a shape has no velocity, a placement over time
does. What the COOKED form carries is a second position stream, and only for geometry that
actually deforms.

**This constrains board:1989, and the premise it was written on was wrong.** "It works today
through a uniform nobody thinks about" was not measured. It does not work today: `Encode` sets
`before[i] = model[i]` and `SubjectProxy` assigns `PrevAnchor = Anchor` at both sites, so the
previous transform IS the current one and this tree carries no rigid motion vector at all. What
moves is the camera and per-vertex `prevP`; a rigid subject that changes place produces none.
So the previous placements still must move into the buffer with the current ones -- but that is
adding a capability, not preserving one, and it needs board:1998 first: a frame has to END
somewhere every render path passes through before "previous" can mean anything.

## GPU physics: optics yes, mechanics no

Unreal runs Chaos rigid bodies on the CPU and puts cloth and Niagara particles on the GPU; RAGE
does the same shape. **Neither runs the simulation that DECIDES anything on the GPU**, and the
reason is this tree's own invariant: *temporally DETERMINISTIC -- fixed step, one order,
interpolation to the display*. GPU floating-point reductions are not reproducible across drivers
or hardware, so a replay would not replay and a network peer would diverge.

CLAUDE.md already draws the line and it is the same one: *the simulation is MECHANICS, the
renderer is OPTICS*. Anything that feeds back into the world runs on the CPU at a fixed step.
Anything that only has to LOOK right -- particles, cloth that nothing collides with, foam on
water -- may run on the GPU, because being wrong there costs a pixel rather than a divergence.

## The world IS geometry, and changing it costs what changed

The world is not a scene graph with geometry hanging off it -- it is a set of `Geometry` values
and where they stand, and both are mutable. CLAUDE.md already says the half of this that is about
declaring: *a scenario is a STREAM; `Declare` seeds, then parts enter and leave, and the work a
declaration causes is proportional to what CHANGED*. The other half is the cost: adding one
building, moving one, or dropping one must cost one part's worth of work, never a rebuild. That
is why the cooked form is per-subject and the instance buffer is a table with rows rather than a
blob -- both are what make a mutation local.

## What this forbids, and it is enforceable

- **no pass named after a thing.** `Stage::Terrain`, `Buildings`, `Water` and `Models` declared
  resource edges and executed nothing; they are deleted. A subject noun in the renderer is a
  finding wherever it stands, and one that executes nothing is two
- **no second cooker.** `TilePool` builds `TerrainMesh` and its own DAG inside streaming, so
  today there are two routes from data to drawable geometry. CLAUDE.md allows two FORMS --
  authored and cooked -- and no third; two cookers is a third by another name (board:1991)
- **no CPU term that scales.** The invariant is board:1943's; the build is the five items below

## How it stays true

`harness/claims/TheRendererNamesNoSubject` walks `src/render/` for subject nouns and refuses. The
count is declared and may only fall, the same instrument as `TheEngineNamesNoSubject` -- which
walks `src/` and `include/` as a whole and did NOT catch these four, because `Terrain` was not on
its list. A guard is only as good as the words it knows, so this one names the renderer's own.

- [x] the renderer names no subject, and a claim holds it there
      proof: harness/claims/TheRendererNamesNoSubject
- [ ] the six children land in order: 1990 (one convention) · 1989 (instances) · 1991 (one
      cooker) · 1992 (culling) · 1993 (indirect draw) · 1994 (visibility buffer, decided)
- [ ] the driver client (deleted)'s frame time is taken BEFORE the first of them, so "faster" is a measurement
      rather than a hope
- [ ] every producer of 3D hands back a `Geometry` and there is no second entrance: the glTF
      reader, every generator, a scenario's declaration and a client's own code
- [ ] a mutation costs what CHANGED: adding, moving or dropping one part is one part's work, and
      the number that shows it is the frame cost of a scene that gained one building against the
      same scene that always had it

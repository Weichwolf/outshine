Type: debt
State: open
Area: engine, generators, compositor
Tags: architecture, owner

# The road and the building are GENERATORS, and the ground is asked rather than handed over

**Benchmark** -- Unreal: a `ULandscapeSplineComponent` never edits the landscape's vertices; it
writes a deformation request into the landscape's heightmap layer, and foliage asks
`GetHeightAtLocation`. `bAllowCPUAccess` is OFF by default, so the render mesh is not addressable
from the CPU at all. RAGE: terrain is a heightfield the streaming owns, props query it, and roads
are baked into it at COOK time -- no runtime hands vertices around. **Both agree on the shape**: a
thing that stands on the ground ASKS the ground and HANDS BACK a request; it never receives the
mesh.

## What the tree has, and where it is wrong

`harness/claims/TheEngineNamesNoSubject` is RED and names the file: the engine may not know a
street, a bridge or a house. Its vocabulary is body, mesh, material, instance, tile. Today
`src/engine/Laying.cpp` -- 3 000 lines, once two thirds of `Picturing.cpp` -- knows `lane`,
`Bridge`, `CoverRow`, `Frontage` and `RoofKind`. That derivation is a GENERATOR's work and it sits
in the motor.

**The good news is that the interface is already right and only the LOCATION is wrong.** `Paves`
writes no ground vertex. It builds `corridor` as a `std::vector<Yields>` -- outlines with a target
profile -- and `Grounds` hands those to `YieldGround` in the compositor, which presses the ring.
That is exactly Unreal's deformation request. What is missing is the DOOR: the engine reaches into
the derivation sideways instead of the derivation standing behind
`include/generate/Generate.h` where a client's own generator stands.

## What a generator is given, and it is never the mesh

Three capabilities, and a generator that needs none of them uses none -- there is no optional
parameter to get wrong.

| | what | where it already is |
|---|---|---|
| **ask** | `GroundQuery`: the height, the slope and the land class at a place | `sampleHeight` stands in the door |
| **press** | it hands back `Yields` -- an outline and the profile it wants -- and never touches a vertex | `YieldGround` in the compositor |
| **build** | it emits its OWN mesh | the generators already do |

**Handing a generator the ground mesh would cost three things**: every generator would have to know
the mesh's layout and LOD rules; two generators pressing the same triangles would give a result
that depends on the ORDER they ran in, which the fourth invariant forbids by name; and the mesh's
ownership would leave the compositor, which makes board 2100's memory worse rather than better.

## And the generic half is generic, so it leaves too

Six helpers in `Laying.cpp` are not about ground or roads at all. They are mesh tools any generator
could want, and while they sit in one engine file nobody else can reach them.

| helper | what it actually is | where it belongs |
|---|---|---|
| `EdgeKey(a, b)` | an undirected edge key | `src/base/spatial/` |
| `Divided(face, cut, finer)` | RED-GREEN triangle subdivision | `src/base/spatial/` |
| `CensusOverEveryTriangle` | a mesh soundness audit: a hole, a non-manifold edge, a reversed face | `src/base/spatial/`, returning a RECORD rather than publishing to the engine's ledger |
| `CarryIntoTheFrame` | ECEF corners into a tangent frame | beside `TangentFrame` |
| `Drape` / `DrapeCellM` | the height of a triangle soup at an east/south | `src/base/spatial/` |
| `WayEndKey(at)` | a quantised place key for matching nodes | `src/base/geo/` |

`CensusOverEveryTriangle` is the sharpest of these: it answers "is this a closed surface" and it
was written twice in this tree -- the second copy was `BuildingMesh`'s `Judged`, deleted this round
because nothing reached it. Two copies of a mesh audit is what happens when the first one is
locked inside an engine file.

**And pressing a mesh to a profile is the seventh.** Half of it is `YieldGround` already; the other
half is the levelling loops inside `Paves`, which iterate a corridor flat. A rail, a runway, a canal
and a building pad all want the same verb.

## What will be true

- [ ] `harness/claims/TheEngineNamesNoSubject` is GREEN: no `lane`, `Bridge`, `RoofKind` or
      `Frontage` anywhere under `src/engine/`
- [ ] The road derivation and the building derivation are generators, registered beside the tree
      grower, reached only through `include/generate/Generate.h`
- [ ] The generic mesh tools stand where any generator can use them, each with a case of its own
- [ ] No generator receives the ground mesh. A case tries to reach it through the door and cannot
- [ ] The pressers are applied in a DECLARED order, and a case that shuffles two generators'
      pressers renders the same bytes
- [ ] `src/engine/Laying.cpp` is gone the way `Picturing.cpp` went, and what is left of it in the
      engine is the SEQUENCE: ask, press, compose

## The order

1. the generic mesh tools out first -- they are the layer the rest stands on and they move without
   touching behaviour
2. then the derivation behind the generators' door, one subject at a time: the road, then the
   building
3. the engine keeps `Grounds` as the caller and nothing else

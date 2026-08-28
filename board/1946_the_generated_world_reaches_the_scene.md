Type: feature
State: active
Progress: streaming
Area: world
Tags: benchmark, target

# What the generators build reaches the scene a scenario declares

**Benchmark** — Unreal: PCG output becomes actors in the same world. RAGE: generated content ships in the map. **Both agree** — what a generator builds reaches the scene through the same path authored content does.

Both benchmarks are world-streaming engines first and everything else second: Unreal's World
Partition streams cells with their actors, RAGE streams its map by node. outshine has the
generators and no path from them to a picture.

Measured: `src/world/generators/` plus `draw/`, `RegionForge` and `Clients::Sim` are 6528 of the
tree's 49769 lines -- buildings, tree growers with leaf-angle distributions, forests, roof
surfaces, water, infrastructure. `RegionForge` is held only by `Clients::Sim`, and
`grep -rn '"Sim.h"'` finds exactly ONE line: its own `.cpp`. Nothing else in the tree includes it.

- [ ] `Clients::Sim` has a consumer, or its capability moves to something that does (board:1805)
- [ ] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` execute, so what a generator makes
      can land in a pass (board:1805)
- [ ] a generator is a LIBRARY emitting the representation the compositor consumes, rather than
      a program with its own loop (board:1197)
- [ ] a building, a tree and a water surface stand beside the road in the still (board:1936)
- [ ] the world composed for a drive and the world composed for a viewer are ONE path
      (board:1805)
- [ ] **the world a drive moves through is re-composed INCREMENTALLY, not rebuilt per step.**
      Moved here from board:1574, whose own measurement sent it: that item blamed
      `SubjectDraw::SetMesh` for re-uploading every stream at every relay, and a `mesh-relay` tag
      over a 48-step drive reads ZERO -- the renderer costs nothing per step. What the drive
      takes, per step, over `apps/driver/src/f31.scenario` at 320x180:

          world-restand         1 106 280 B      GroundStack::Restand
          untagged              1 365 596 B      a thread no tag covers yet
          world-grow               60 726 B
          frame-tells               3 484 B
          render-frame              1 328 B
          mesh-relay                    0        the cause board:1574 named

      Unreal's World Partition streams a CELL in and out and does not rebuild the ones that did
      not change; RAGE streams by map node with the same rule. **Both agree**, and 1.1 MB a step
      for a drive that moves a few metres is a rebuild wearing a stream's name.
      The instrument is `heap taken under <tag>` through the door, differenced by the consumer;
      `apps/bench --steps 48 --heap --cache <store>` is the reading above, verbatim.
- [ ] a route crosses a continent over a graph that STREAMS rather than one held whole
      (board:1503)
- [ ] cut and fill meet the road, and the road is drawn (board:1505)

## MEASURED: the generators say WHAT each instance is, and the engine's sink drops it

Five places on Earth now fetch their terrain and their OSM, and the numbers are the places
themselves -- Shibuya 15 833 streets and 91 208 footprints, Rothenburg 2 633 and 6 423, the canyon
rim mostly trails. The pictures in `build/places/` show terrain and **not one building or street**.

The chain is: OSM -> `BuildingField` and `StreetField` -> `Generators::SnapshotOver` ->
`Shipping::Drawing().Draw(...)` -> a `DrawSink` -> `World.Instances`. It ends there:
`grep -rn 'World.Instances' src/` finds the fill and the SIZE being published, and no third reader.

And the reason nothing downstream *could* draw them is one line in `src/engine/Picturing.cpp`:

    [[nodiscard]] bool Add(Generators::BodyId, Generators::ClusterId,
                           const Generators::Instance &instance) noexcept override {
      Into_->push_back(instance);
      ...

**`BodyId` and `ClusterId` are unnamed parameters.** The generator states which body and which
cluster every instance is, and the engine's sink throws both away and keeps the bare transform:

    struct Instance { float Em, Nm; float AslM; float YawRad; float Scale; };

So `World.Instances` is a list of places with no subjects. Nothing can draw it because nothing in
it says what to draw. `ClusterId` is declared in the same header, beside `Instance`, and `Instance`
does not carry one.

Unreal: `FInstancedStaticMeshComponent` holds a MESH and its per-instance transforms together, and
`FGPUScene` keeps the primitive id beside every instance. RAGE: an entity carries its drawable
dictionary index. **Both keep the identity with the transform**, because a transform alone cannot
be drawn.

- [ ] the sink keeps the body and the cluster it is handed, so an instance says what it is
- [ ] the instances reach the picture through the same instanced draw a body uses -- one call, N
      instances, N rows (landed in board:1574)
- [ ] a place's footprints appear in `build/places/`, judged by eye against the written expectation

**The measurement that would show I am wrong:** if the ids are kept and the pictures still show no
buildings, the sink was not the blocker and the fault is downstream in the draw sources. The count
to watch is `instances its draw sources made` against what a place HOLDS -- Shibuya makes 4 096
against 91 208 footprints, which is the vegetation budget rather than the buildings, so those two
numbers should converge on the same subject when this is right.
